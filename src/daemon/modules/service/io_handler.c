/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: lifeng
 * Create: 2020
 * Description: provide container stream callback function definition
 ********************************************************************************/
#define _GNU_SOURCE
#include "io_handler.h"

#include <stdio.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <limits.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <time.h>

#include "isula_libutils/log.h"
#include "console.h"
#include "isulad_config.h"
#include "io_wrapper.h"
#include "utils.h"
#include "utils_file.h"
#include "utils_queue.h"
#include "err_msg.h"

static char *create_single_fifo(const char *statepath, const char *subpath, const char *stdflag)
{
    int nret = 0;
    char *fifo_name = NULL;
    char fifo_path[PATH_MAX] = { 0 };

    fifo_name = util_common_calloc_s(PATH_MAX);
    if (fifo_name == NULL) {
        return NULL;
    }

    nret = console_fifo_name(statepath, subpath, stdflag, fifo_name, PATH_MAX, fifo_path, sizeof(fifo_path), true);
    if (nret != 0) {
        ERROR("Failed to get console fifo name.");
        goto out;
    }

    if (console_fifo_create(fifo_name)) {
        ERROR("Failed to create console fifo.");
        goto out;
    }

    return fifo_name;

out:
    free(fifo_name);
    return NULL;
}

static int do_create_daemon_fifos(const char *statepath, const char *subpath, bool attach_stdin, bool attach_stdout,
                                  bool attach_stderr, char *fifos[])
{
    int ret = -1;

    if (attach_stdin) {
        fifos[0] = create_single_fifo(statepath, subpath, "in");
        if (fifos[0] == NULL) {
            goto cleanup;
        }
    }

    if (attach_stdout) {
        fifos[1] = create_single_fifo(statepath, subpath, "out");
        if (fifos[1] == NULL) {
            goto cleanup;
        }
    }

    if (attach_stderr) {
        fifos[2] = create_single_fifo(statepath, subpath, "err");
        if (fifos[2] == NULL) {
            goto cleanup;
        }
    }

    ret = 0;

cleanup:
    if (ret != 0) {
        console_fifo_delete(fifos[0]);
        free(fifos[0]);
        fifos[0] = NULL;
        console_fifo_delete(fifos[1]);
        free(fifos[1]);
        fifos[1] = NULL;
        console_fifo_delete(fifos[2]);
        free(fifos[2]);
        fifos[2] = NULL;
    }
    return ret;
}

int create_daemon_fifos(const char *id, const char *runtime, bool attach_stdin, bool attach_stdout, bool attach_stderr,
                        const char *operation, char *fifos[], char **fifopath)
{
    int nret;
    int ret = -1;
    char *statepath = NULL;
    char subpath[PATH_MAX] = { 0 };
    char fifodir[PATH_MAX] = { 0 };
    struct timespec now;
    pthread_t tid;

    nret = clock_gettime(CLOCK_REALTIME, &now);
    if (nret != 0) {
        ERROR("Failed to get time");
        goto cleanup;
    }

    tid = pthread_self();

    statepath = conf_get_routine_statedir(runtime);
    if (statepath == NULL) {
        ERROR("State path is NULL");
        goto cleanup;
    }

    nret = snprintf(subpath, PATH_MAX, "%s/%s/%u_%u_%u", id, operation, (unsigned int)tid, (unsigned int)now.tv_sec,
                    (unsigned int)(now.tv_nsec));
    if (nret >= PATH_MAX || nret < 0) {
        ERROR("Failed to print string");
        goto cleanup;
    }

    nret = snprintf(fifodir, PATH_MAX, "%s/%s", statepath, subpath);
    if (nret >= PATH_MAX || nret < 0) {
        ERROR("Failed to print string");
        goto cleanup;
    }
    *fifopath = util_strdup_s(fifodir);

    if (do_create_daemon_fifos(statepath, subpath, attach_stdin, attach_stdout, attach_stderr, fifos) != 0) {
        goto cleanup;
    }

    ret = 0;

cleanup:
    free(statepath);
    return ret;
}

void delete_daemon_fifos(const char *fifopath, const char *fifos[])
{
    if (fifopath == NULL || fifos == NULL) {
        return;
    }
    if (fifos[0] != NULL) {
        console_fifo_delete(fifos[0]);
    }
    if (fifos[1] != NULL) {
        console_fifo_delete(fifos[1]);
    }
    if (fifos[2] != NULL) {
        console_fifo_delete(fifos[2]);
    }
    if (util_recursive_rmdir(fifopath, 0)) {
        WARN("Failed to rmdir:%s", fifopath);
    }
}

typedef enum { IO_FD = 0, IO_FIFO, IO_FUNC, IO_MAX } io_type;
typedef enum { IO_READ_FROM_ISULA = 0, IO_READ_FROM_LXC, IO_WRITE_TO_ISULA, IO_WRITE_TO_LXC, IO_RDWR_MAX } io_fifo_type;

struct io_copy_arg {
    io_type srctype;
    void *src;
    io_type dsttype;
    void *dst;
    io_fifo_type dstfifotype;
};

struct io_copy_thread_arg {
    struct io_copy_arg *copy_arg;
    bool detach;
    size_t len;
    int sync_fd;
    sem_t wait_sem;
    int *infds;
    int *outfds; // recored fds to close
    int *srcfds;
    struct io_write_wrapper *writers;
    volatile struct queue_buffers buffers;
    volatile bool iocopy_reader_finish;
    sem_t iocopy_writer_finish_sem;
};

static void free_queue_buffers(volatile struct queue_buffers *buffers)
{
    sem_destroy(buffers->io_sem);
    free(buffers->io_sem);
    buffers->io_sem = NULL;

    circular_queue_destroy(buffers->stdout_queue_buffer);
    buffers->stdout_queue_buffer = NULL;
    circular_queue_destroy(buffers->stderr_queue_buffer);
    buffers->stderr_queue_buffer = NULL;
}

static void io_copy_thread_cleanup(struct io_copy_thread_arg *thread_arg)
{
    size_t i = 0;
    struct io_write_wrapper *writers = thread_arg->writers;
    int *infds = thread_arg->infds;
    int *outfds = thread_arg->outfds;
    int *srcfds = thread_arg->srcfds;
    size_t len = thread_arg->len;


    sem_destroy(&thread_arg->iocopy_writer_finish_sem);
    free_queue_buffers(&thread_arg->buffers);

    for (i = 0; i < len; i++) {
        if (writers != NULL && writers[i].close_func != NULL) {
            (void)writers[i].close_func(writers[i].context, NULL);
        }
    }
    free(srcfds);
    for (i = 0; i < len; i++) {
        if ((infds != NULL) && (infds[i] >= 0)) {
            console_fifo_close(infds[i]);
        }
        if ((outfds != NULL) && (outfds[i] >= 0)) {
            console_fifo_close(outfds[i]);
        }
    }
    free(infds);
    free(outfds);
    free(writers);
    free(thread_arg);
}

static int io_copy_init_fds(size_t len, struct io_copy_thread_arg *thread_arg)
{
    int ret = 0;
    size_t i;

    if (len > SIZE_MAX / sizeof(struct io_write_wrapper)) {
        ERROR("Invalid arguments");
        return -1;
    }

    thread_arg->srcfds = util_common_calloc_s(sizeof(int) * len);
    if (thread_arg->srcfds == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    thread_arg->infds = util_common_calloc_s(sizeof(int) * len);
    if (thread_arg->infds == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    for (i = 0; i < len; i++) {
        (thread_arg->infds)[i] = -1;
    }

    thread_arg->outfds = util_common_calloc_s(sizeof(int) * len);
    if (thread_arg->outfds == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    for (i = 0; i < len; i++) {
        (thread_arg->outfds)[i] = -1;
    }

    thread_arg->writers = util_common_calloc_s(sizeof(struct io_write_wrapper) * len);
    if (thread_arg->writers == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

out:
    return ret;
}

typedef int (*io_type_handle)(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg);

struct io_copy_handler {
    io_type type;
    io_type_handle handle;
};

static int handle_src_io_fd(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    thread_arg->srcfds[index] = *(int *)(copy_arg[index].src);

    return 0;
}

static int handle_src_io_fifo(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    if (console_fifo_open((const char *)copy_arg[index].src, &(thread_arg->infds[index]), O_RDONLY | O_NONBLOCK)) {
        ERROR("failed to open console fifo.");
        return -1;
    }
    thread_arg->srcfds[index] = thread_arg->infds[index];

    return 0;
}

static int handle_src_io_fun(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    ERROR("Got invalid src fd type");
    return -1;
}

static int handle_src_io_max(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    ERROR("Got invalid src fd type");
    return -1;
}

static int io_copy_make_srcfds(size_t len, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    size_t i;

    struct io_copy_handler src_handler_jump_table[] = {
        { IO_FD,      handle_src_io_fd},
        { IO_FIFO,    handle_src_io_fifo},
        { IO_FUNC,    handle_src_io_fun},
        { IO_MAX,     handle_src_io_max},
    };

    for (i = 0; i < len; i++) {
        if (src_handler_jump_table[(int)(copy_arg[i].srctype)].handle(i, copy_arg, thread_arg) != 0) {
            return -1;
        }
    }

    return 0;
}

static ssize_t write_to_fifo(void *context, const void *data, size_t len)
{
    ssize_t ret;
    int fd;

    fd = *(int *)context;
    ret = util_write_nointr_for_fifo(fd, data, len);
    // Ignore EAGAIN to prevent hang, do not report error
    if (errno == EAGAIN) {
        return (ssize_t)len;
    }
    if ((ret <= 0) || (ret != (ssize_t)len)) {
        ERROR("Failed to write %d: %s", fd, strerror(errno));
        return -1;
    }
    return ret;
}

static ssize_t write_to_fd(void *context, const void *data, size_t len)
{
    ssize_t ret;

    ret = util_write_nointr(*(int *)context, data, len);
    if ((ret <= 0) || (ret != (ssize_t)len)) {
        ERROR("Failed to write: %s", strerror(errno));
        return -1;
    }

    return ret;
}

static int handle_dst_io_fd(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    thread_arg->writers[index].context = copy_arg[index].dst;
    thread_arg->writers[index].write_func = write_to_fd;

    return 0;
}

static int handle_dst_io_fifo(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    int *outfds = thread_arg->outfds;
    struct io_write_wrapper *writers = thread_arg->writers;

    int flag = copy_arg[index].dstfifotype == IO_WRITE_TO_LXC ? O_RDWR : O_WRONLY;
    if (console_fifo_open_withlock((const char *)copy_arg[index].dst, &outfds[index], flag | O_NONBLOCK)) {
        ERROR("Failed to open console fifo.");
        return -1;
    }
    writers[index].context = &outfds[index];
    writers[index].write_func = write_to_fifo;

    return 0;
}

static int handle_dst_io_fun(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    struct io_write_wrapper *io_write = copy_arg[index].dst;
    thread_arg->writers[index].context = io_write->context;
    thread_arg->writers[index].write_func = io_write->write_func;
    thread_arg->writers[index].close_func = io_write->close_func;

    return 0;
}

static int handle_dst_io_max(int index, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    ERROR("Got invalid dst fd type");
    return -1;
}

static int io_copy_make_dstfds(size_t len, struct io_copy_arg *copy_arg, struct io_copy_thread_arg *thread_arg)
{
    size_t i;

    struct io_copy_handler dst_handler_jump_table[] = {
        { IO_FD,      handle_dst_io_fd},
        { IO_FIFO,    handle_dst_io_fifo},
        { IO_FUNC,    handle_dst_io_fun},
        { IO_MAX,     handle_dst_io_max},
    };

    for (i = 0; i < len; i++) {
        if (dst_handler_jump_table[(int)(copy_arg[i].dsttype)].handle(i, copy_arg, thread_arg) != 0) {
            return -1;
        }
    }

    return 0;
}

static void *io_copy_producer_thread_main(void *arg)
{
    int ret = -1;
    struct io_copy_thread_arg *thread_arg = (struct io_copy_thread_arg *)arg;
    bool posted = false;

    if (thread_arg->detach) {
        ret = pthread_detach(pthread_self());
        if (ret != 0) {
            CRIT("Set thread detach fail");
            goto err;
        }
    }

    (void)prctl(PR_SET_NAME, "IoCopyReader");

    sem_post(&thread_arg->wait_sem);
    posted = true;

    (void)console_loop_io_copy_reader(thread_arg->sync_fd, thread_arg->srcfds, &thread_arg->buffers, thread_arg->writers,
                                      thread_arg->len);

err:
    if (!posted) {
        sem_post(&thread_arg->wait_sem);
    }
    thread_arg->iocopy_reader_finish = true;
    // Ensure that the consumer thread ends
    sem_post(thread_arg->buffers.io_sem);

    sem_wait(&thread_arg->iocopy_writer_finish_sem);

    io_copy_thread_cleanup(thread_arg);
    DAEMON_CLEAR_ERRMSG();
    return NULL;
}

static void *io_copy_consumer_thread_main(void *arg)
{
    int ret = -1;
    struct io_copy_thread_arg *thread_arg = (struct io_copy_thread_arg *)arg;

    if (thread_arg->detach) {
        ret = pthread_detach(pthread_self());
        if (ret != 0) {
            CRIT("Set thread detach fail");
            goto err;
        }
    }

    (void)prctl(PR_SET_NAME, "IoCopyWriter");

    console_loop_io_copy_writer(thread_arg->sync_fd, thread_arg->srcfds, &thread_arg->iocopy_reader_finish,
                                &thread_arg->buffers, thread_arg->writers, thread_arg->len);

err:
    DAEMON_CLEAR_ERRMSG();
    sem_post(&thread_arg->iocopy_writer_finish_sem);
    return NULL;
}

static int thread_arg_buffers_init(struct io_copy_thread_arg *thread_arg)
{
    int ret = 0;

    thread_arg->buffers.io_sem = util_common_calloc_s(sizeof(sem_t));
    if (thread_arg->buffers.io_sem == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (sem_init(thread_arg->buffers.io_sem, 0, 0)) {
        ERROR("Failed to init io copy semaphore");
        ret = -1;
        goto out;
    }

    thread_arg->buffers.stdout_queue_buffer = circular_queue_init();
    if (thread_arg->buffers.stdout_queue_buffer == NULL) {
        ERROR("Failed to init stdout circular queue buffer");
        ret = -1;
        goto out;
    }

    thread_arg->buffers.stderr_queue_buffer = circular_queue_init();
    if (thread_arg->buffers.stderr_queue_buffer == NULL) {
        ERROR("Failed to init stderr circular queue buffer");
        ret = -1;
        goto out;
    }

    return ret;

out:
    free_queue_buffers(&thread_arg->buffers);
    return ret;
}

static int io_copy_thread_arg_init(struct io_copy_thread_arg *thread_arg, int sync_fd, bool detach,
                                   struct io_copy_arg *copy_arg, size_t len)
{
    int ret = 0;

    thread_arg->detach = detach;
    thread_arg->copy_arg = copy_arg;
    thread_arg->len = len;
    thread_arg->sync_fd = sync_fd;
    thread_arg->iocopy_reader_finish = false;

    if (sem_init(&thread_arg->wait_sem, 0, 0)) {
        ERROR("Failed to init start semaphore");
        ret = -1;
        goto out;
    }

    if (sem_init(&thread_arg->iocopy_writer_finish_sem, 0, 0)) {
        ERROR("Failed to init writer finish semaphore");
        ret = -1;
        goto out;
    }

    if (io_copy_init_fds(len, thread_arg) != 0) {
        ERROR("Failed to init fds");
        ret = -1;
        goto out;
    }

    if (io_copy_make_srcfds(len, copy_arg, thread_arg) != 0) {
        ERROR("Failed to init source fds");
        ret = -1;
        goto out;
    }

    if (io_copy_make_dstfds(len, copy_arg, thread_arg) != 0) {
        ERROR("Failed to init dst fds");
        ret = -1;
        goto out;
    }

    if (thread_arg_buffers_init(thread_arg) != 0) {
        ERROR("Failed to init queue buffers");
        ret = -1;
        goto out;
    }

out:
    return ret;
}


static int create_producer_consumer_thread(struct io_copy_thread_arg *thread_arg,
                                           pthread_t *reader_tid, pthread_t *writer_tid)
{
    if (pthread_create(reader_tid, NULL, io_copy_producer_thread_main, (void *)thread_arg) != 0) {
        CRIT("Thread creation failed");
        return -1;
    }

    if (pthread_create(writer_tid, NULL, io_copy_consumer_thread_main, (void *)thread_arg) != 0) {
        CRIT("Consumer thread creation failed");
        return -1;
    }

    return 0;
}

static int start_io_copy_thread(int sync_fd, bool detach, struct io_copy_arg *copy_arg, size_t len,
                                pthread_t *reader_tid, pthread_t *writer_tid)
{
    int ret = 0;
    struct io_copy_thread_arg *thread_arg = NULL;

    if (copy_arg == NULL || len == 0) {
        return 0;
    }

    thread_arg = (struct io_copy_thread_arg *)util_common_calloc_s(sizeof(struct io_copy_thread_arg));
    if (thread_arg == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (io_copy_thread_arg_init(thread_arg, sync_fd, detach, copy_arg, len) != 0) {
        ERROR("Failed to init io copy thread arg");
        ret = -1;
        goto err_out;
    }

    if (create_producer_consumer_thread(thread_arg, reader_tid, writer_tid) != 0) {
        ERROR("Failed to create producer & consumer thread for io copy");
        ret = -1;
        goto err_out;
    }

    sem_wait(&thread_arg->wait_sem);
    sem_destroy(&thread_arg->wait_sem);

    return 0;

err_out:
    io_copy_thread_cleanup(thread_arg);
    return ret;
}

static void add_io_copy_element(struct io_copy_arg *element,
                                io_type srctype, void *src,
                                io_type dsttype, void *dst,
                                io_fifo_type dstfifotype)
{
    element->srctype = srctype;
    element->src = src;
    element->dsttype = dsttype;
    element->dst = dst;
    element->dstfifotype = dstfifotype;
}

/*
    -----------------------------------------------------------------------------------
    |  CHANNEL |      iSula                          iSulad                    lxc    |
    -----------------------------------------------------------------------------------
    |          |                    fifoin                         fifos[0]           |
    |    IN    |       RDWR       -------->       RD      RDWR     -------->      RD  |
    -----------------------------------------------------------------------------------
    |          |                   fifoout                         fifos[1]           |
    |    OUT   |       RD         <--------       WR       RD      <--------      WR  |
    -----------------------------------------------------------------------------------
    |          |                   fifoerr                         fifos[2]           |
    |    ERR   |       RD         <--------       WR       RD      <--------     WR   |
    -----------------------------------------------------------------------------------
*/
int ready_copy_io_data(int sync_fd, bool detach, const char *fifoin, const char *fifoout, const char *fifoerr,
                       int stdin_fd, struct io_write_wrapper *stdout_handler, struct io_write_wrapper *stderr_handler,
                       const char *fifos[], pthread_t *reader_tid, pthread_t *writer_tid)
{
    size_t len = 0;
    struct io_copy_arg io_copy[6];

    if (fifoin != NULL) {
        add_io_copy_element(&io_copy[len++], IO_FIFO, (void *)fifoin, IO_FIFO, (void *)fifos[0], IO_WRITE_TO_LXC);
    }

    if (fifoout != NULL) {
        add_io_copy_element(&io_copy[len++], IO_FIFO, (void *)fifos[1], IO_FIFO, (void *)fifoout, IO_WRITE_TO_ISULA);
    }

    if (fifoerr != NULL) {
        add_io_copy_element(&io_copy[len++], IO_FIFO, (void *)fifos[2], IO_FIFO, (void *)fifoerr, IO_WRITE_TO_ISULA);
    }

    if (stdin_fd > 0) {
        add_io_copy_element(&io_copy[len++], IO_FD,  &stdin_fd, IO_FIFO, (void *)fifos[0], IO_RDWR_MAX);
    }

    if (stdout_handler != NULL) {
        add_io_copy_element(&io_copy[len++], IO_FIFO, (void *)fifos[1], IO_FUNC, stdout_handler, IO_RDWR_MAX);
    }

    if (stderr_handler != NULL) {
        add_io_copy_element(&io_copy[len++], IO_FIFO, (void *)fifos[2], IO_FUNC, stderr_handler, IO_RDWR_MAX);
    }

    if (start_io_copy_thread(sync_fd, detach, io_copy, len, reader_tid, writer_tid) != 0) {
        return -1;
    }

    return 0;
}
