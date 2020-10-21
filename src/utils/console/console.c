/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: tanyifeng
 * Create: 2018-11-08
 * Description: provide console definition
 ******************************************************************************/
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h> // IWYU pragma: keep
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

#include "console.h"
#include "mainloop.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "constants.h"
#include "utils_file.h"

static ssize_t fd_write_function(void *context, const void *data, size_t len)
{
    ssize_t ret;

    ret = util_write_nointr_for_fifo(*(int *)context, data, len);
    if ((ret <= 0) || (ret != (ssize_t)len)) {
        ERROR("Failed to write: %s", strerror(errno));
        return -1;
    }

    return ret;
}

/* console cb tty fifoin */
static int console_cb_tty_stdin_with_escape(int fd, uint32_t events, void *cbdata, struct epoll_descr *descr)
{
    struct tty_state *ts = cbdata;
    char c;
    int ret = 0;
    ssize_t r_ret, w_ret;

    if (fd != ts->stdin_reader) {
        ret = 1;
        goto out;
    }

    r_ret = util_read_nointr(ts->stdin_reader, &c, 1);
    if (r_ret <= 0) {
        ret = 1;
        goto out;
    }

    if (ts->tty_exit != -1) {
        if (c == ts->tty_exit && !(ts->saw_tty_exit)) {
            ts->saw_tty_exit = 1;
            goto out;
        }

        if (c == 'q' && ts->saw_tty_exit) {
            ret = 1;
            goto out;
        }

        ts->saw_tty_exit = 0;
    }

    if (ts->stdin_writer.context && ts->stdin_writer.write_func) {
        w_ret = ts->stdin_writer.write_func(ts->stdin_writer.context, &c, 1);
        if ((w_ret <= 0) || (w_ret != r_ret)) {
            ret = 1;
            goto out;
        }
    }

out:
    return ret;
}

static int console_writer_write_data(const struct io_write_wrapper *writer, const char *buf, ssize_t len)
{
    ssize_t ret;

    if (writer == NULL || writer->context == NULL || writer->write_func == NULL || len <= 0) {
        return 0;
    }
    ret = writer->write_func(writer->context, buf, (size_t)len);
    if (ret <= 0 || ret != len) {
        ERROR("failed to write, error:%s", strerror(errno));
        return -1;
    }

    return 0;
}

struct forwarding_stragery {
    int (*write_data)(int fd, struct tty_state *ts, const char *buf, ssize_t len);
};

struct console_writer_forwarding_data {
    struct forwarding_stragery super;
};

static int console_writer_forward(int fd, struct tty_state *ts, const char *buf, ssize_t len)
{
    if (fd == ts->stdin_reader) {
        return console_writer_write_data(&ts->stdin_writer, buf, len);
    }

    if (fd == ts->stdout_reader) {
        return console_writer_write_data(&ts->stdout_writer, buf, len);
    }

    if (fd == ts->stderr_reader) {
        return console_writer_write_data(&ts->stderr_writer, buf, len);
    }

    return 0;
}

static struct forwarding_stragery *console_writer_forwarding_data_init()
{
    struct console_writer_forwarding_data *op = util_common_calloc_s(sizeof(struct console_writer_forwarding_data));
    if (op == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    op->super.write_data = console_writer_forward;

    return &op->super;
}

struct queue_buffer_forwarding_data {
    struct forwarding_stragery super;
};

static int queue_buffer_forward(int fd, struct tty_state *ts, const char *buf, ssize_t len)
{
    int ret = 0;

    char *buffer = util_common_calloc_s(len);
    if (buffer == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    memcpy(buffer, buf, len);

    if (fd == ts->stdin_reader) {
        return console_writer_write_data(&ts->stdin_writer, buffer, len);
    }

    if (fd == ts->stdout_reader) {
        ts->buffers->outfd = *(int *)ts->stdout_writer.context;
        ret = circular_queue_push(ts->buffers->stdout_queue_buffer, buffer, len);
        (void)sem_post(ts->buffers->io_sem);
        return ret;
    }

    if (fd == ts->stderr_reader) {
        ts->buffers->errfd = *(int *)ts->stderr_writer.context;
        ret = circular_queue_push(ts->buffers->stderr_queue_buffer, buffer, len);
        (void)sem_post(ts->buffers->io_sem);
        return ret;
    }

    return ret;
}

static struct forwarding_stragery *queue_buffer_forwarding_data_init()
{
    struct queue_buffer_forwarding_data *op = util_common_calloc_s(sizeof(struct queue_buffer_forwarding_data));
    if (op == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    op->super.write_data = queue_buffer_forward;

    return &op->super;
}

struct forwarding_context {
    struct forwarding_stragery *stragery;
    int (*execute_stratery)(struct forwarding_context *ctx, int fd, struct tty_state *ts, char *buf, ssize_t len);
};

static int forwarding_context_execute(struct forwarding_context *ctx, int fd, struct tty_state *ts, char *buf,
                                      ssize_t len)
{
    return ctx->stragery->write_data(fd, ts, buf, len);
}

static void forwarding_context_set_stratery(struct forwarding_context *ctx, struct forwarding_stragery *s)
{
    ctx->stragery = s;
}

static struct forwarding_context *forwarding_context_init()
{
    struct forwarding_context *ctx = util_common_calloc_s(sizeof(struct forwarding_context));
    if (ctx == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    ctx->execute_stratery = forwarding_context_execute;

    return ctx;
}

static bool console_cb_stdio_copy_check_parameter(int fd, struct tty_state *ts)
{
    return fd == ts->sync_fd || (fd != ts->stdin_reader && fd != ts->stdout_reader && fd != ts->stderr_reader);
}

typedef int (*execute_forwarding_data_fun)(int, struct tty_state *, char *, ssize_t);

static int do_console_cb_stdio_copy(int fd, uint32_t events, void *cbdata, struct epoll_descr *descr,
                                    execute_forwarding_data_fun execute)
{
    struct tty_state *ts = cbdata;
    char *buf = NULL;
    size_t buf_len = MAX_BUFFER_SIZE;
    int ret = 0;
    ssize_t r_ret;

    buf = util_common_calloc_s(buf_len);
    if (buf == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (console_cb_stdio_copy_check_parameter(fd, ts)) {
        ret = 1;
        goto out;
    }

    r_ret = util_read_nointr(fd, buf, buf_len - 1);
    if (r_ret <= 0) {
        ret = 1;
        goto out;
    }

    if (execute(fd, ts, buf, r_ret) != 0) {
        ret = 1;
        goto out;
    }

out:
    free(buf);
    if (ret > 0) {
        epoll_loop_del_handler(descr, fd);
    }
    return ret;
}

static int execute_console_writer_forwarding_data(int fd, struct tty_state *ts, char *buf, ssize_t len)
{
    int ret = 0;
    struct forwarding_context *ctx = NULL;
    struct forwarding_stragery *s_console_writer_forwarding = NULL;

    ctx = forwarding_context_init();
    if (ctx == NULL) {
        ret = -1;
        goto out;
    }

    s_console_writer_forwarding = console_writer_forwarding_data_init();
    if (s_console_writer_forwarding == NULL) {
        ret = -1;
        goto out;
    }

    forwarding_context_set_stratery(ctx, s_console_writer_forwarding);
    if (ctx->execute_stratery(ctx, fd, ts, buf, len) != 0) {
        ret = -1;
        goto out;
    }

out:
    free(s_console_writer_forwarding);
    free(ctx);
    return ret;
}

static int execute_queue_buffer_forwarding_data(int fd, struct tty_state *ts, char *buf, ssize_t len)
{
    int ret = 0;
    struct forwarding_context *ctx = NULL;
    struct forwarding_stragery *s_queue_buffer_forwarding = NULL;

    ctx = forwarding_context_init();
    if (ctx == NULL) {
        ret = -1;
        goto out;
    }

    s_queue_buffer_forwarding = queue_buffer_forwarding_data_init();
    if (s_queue_buffer_forwarding == NULL) {
        ret = -1;
        goto out;
    }

    forwarding_context_set_stratery(ctx, s_queue_buffer_forwarding);
    if (ctx->execute_stratery(ctx, fd, ts, buf, len) != 0) {
        ret = -1;
        goto out;
    }

out:
    free(s_queue_buffer_forwarding);
    free(ctx);
    return ret;
}

/* console cb tty fifoin */
static int console_cb_stdio_copy(int fd, uint32_t events, void *cbdata, struct epoll_descr *descr)
{
    return do_console_cb_stdio_copy(fd, events, cbdata, descr, execute_console_writer_forwarding_data);
}

/* console cb tty fifoin fro daemon*/
static int console_cb_stdio_copy_from_runtime(int fd, uint32_t events, void *cbdata, struct epoll_descr *descr)
{
    return do_console_cb_stdio_copy(fd, events, cbdata, descr, execute_queue_buffer_forwarding_data);
}

/* console fifo name */
int console_fifo_name(const char *rundir, const char *subpath, const char *stdflag, char *fifo_name,
                      size_t fifo_name_sz, char *fifo_path, size_t fifo_path_sz, bool do_mkdirp)
{
    int ret = 0;
    int nret = 0;

    nret = snprintf(fifo_path, fifo_path_sz, "%s/%s/", rundir, subpath);
    if (nret < 0 || (size_t)nret >= fifo_path_sz) {
        ERROR("FIFO path:%s/%s/ is too long.", rundir, subpath);
        ret = -1;
        goto out;
    }

    if (do_mkdirp && util_mkdir_p(fifo_path, CONSOLE_FIFO_DIRECTORY_MODE) < 0) {
        COMMAND_ERROR("Unable to create console fifo directory %s: %s.", fifo_path, strerror(errno));
        ret = -1;
        goto out;
    }

    nret = snprintf(fifo_name, fifo_name_sz, "%s/%s/%s-fifo", rundir, subpath, stdflag);
    if (nret < 0 || (size_t)nret >= fifo_name_sz) {
        ERROR("FIFO name %s/%s/%s-fifo is too long.", rundir, subpath, stdflag);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

/* console fifo create */
int console_fifo_create(const char *fifo_path)
{
    int ret;

    ret = mknod(fifo_path, S_IFIFO | S_IRUSR | S_IWUSR, (dev_t)0);
    if (ret < 0 && errno != EEXIST) {
        ERROR("Failed to mknod monitor fifo %s: %s.", fifo_path, strerror(errno));
        return -1;
    }

    return 0;
}

/* console fifo delete */
int console_fifo_delete(const char *fifo_path)
{
    char *ret = NULL;
    char real_path[PATH_MAX + 1] = { 0x00 };

    if (fifo_path == NULL || strlen(fifo_path) > PATH_MAX) {
        ERROR("Invalid input!");
        return -1;
    }

    if (strlen(fifo_path) == 0) {
        return 0;
    }

    ret = realpath(fifo_path, real_path);
    if (ret == NULL) {
        if (errno != ENOENT) {
            ERROR("Failed to get real path: %s", fifo_path);
            return -1;
        }
        return 0;
    }

    if (unlink(real_path) && errno != ENOENT) {
        WARN("Unlink %s failed", real_path);
        return -1;
    }

    return 0;
}

/* console fifo open */
int console_fifo_open(const char *fifo_path, int *fdout, int flags)
{
    int fd = 0;

    fd = util_open(fifo_path, flags, (mode_t)0);
    if (fd < 0) {
        ERROR("Failed to open fifo %s to send message: %s.", fifo_path, strerror(errno));
        return -1;
    }

    *fdout = fd;
    return 0;
}

/* console fifo open withlock */
int console_fifo_open_withlock(const char *fifo_path, int *fdout, int flags)
{
    int fd = 0;
    struct flock lk;

    fd = util_open(fifo_path, flags, 0);
    if (fd < 0) {
        WARN("Failed to open fifo %s to send message: %s.", fifo_path, strerror(errno));
        return -1;
    }

    lk.l_type = F_WRLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = 0;
    lk.l_len = 0;
    if (fcntl(fd, F_SETLK, &lk) != 0) {
        /* another console instance is already running, don't start up */
        DEBUG("Another console instance already running with path : %s.", fifo_path);
        close(fd);
        return -1;
    }

    *fdout = fd;
    return 0;
}

/* console fifo close */
void console_fifo_close(int fd)
{
    close(fd);
}

/* setup tios */
int setup_tios(int fd, struct termios *curr_tios)
{
    struct termios tmptios;
    int ret = 0;

    if (!isatty(fd)) {
        ERROR("Specified fd: '%d' is not a tty", fd);
        return -1;
    }

    if (tcgetattr(fd, curr_tios)) {
        ERROR("Failed to get current terminal settings");
        ret = -1;
        goto out;
    }

    tmptios = *curr_tios;

    cfmakeraw(&tmptios);
    tmptios.c_oflag |= OPOST;

    if (tcsetattr(fd, TCSAFLUSH, &tmptios)) {
        ERROR("Set terminal settings failed");
        ret = -1;
        goto out;
    }

out:
    return ret;
}

static void client_console_tty_state_close(struct epoll_descr *descr, const struct tty_state *ts)
{
    if (ts->stdin_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stdin_reader);
    }
    if (ts->stdout_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stdout_reader);
    }
    if (ts->stderr_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stderr_reader);
    }
}

static int safe_epoll_loop(struct epoll_descr *descr)
{
    int ret;

    ret = epoll_loop(descr, -1);
    if (ret != 0) {
        ERROR("Epoll_loop error");
        return ret;
    }

    ret = epoll_loop(descr, 100);
    if (ret != 0) {
        ERROR("Repeat the epoll loop to ensure that all data is transferred");
    }

    return ret;
}

/* console loop */
/* data direction: */
/* read stdinfd, write fifoinfd */
/* read fifooutfd, write stdoutfd */
/* read stderrfd, write stderrfd */
int console_loop_with_std_fd(int stdinfd, int stdoutfd, int stderrfd, int fifoinfd, int fifooutfd, int fifoerrfd,
                             int tty_exit, bool tty)
{
    int ret;
    struct epoll_descr descr;
    struct tty_state ts;

    ret = epoll_loop_open(&descr);
    if (ret != 0) {
        ERROR("Create epoll_loop error");
        return ret;
    }

    ts.tty_exit = tty_exit;
    ts.saw_tty_exit = 0;
    ts.sync_fd = -1;
    ts.stdin_reader = -1;
    ts.stdout_reader = -1;
    ts.stderr_reader = -1;

    if (fifoinfd >= 0) {
        ts.stdin_reader = stdinfd;
        ts.stdin_writer.context = &fifoinfd;
        ts.stdin_writer.write_func = fd_write_function;
        if (tty) {
            ret = epoll_loop_add_handler(&descr, ts.stdin_reader, console_cb_tty_stdin_with_escape, &ts);
            if (ret != 0) {
                INFO("Add handler for stdinfd faied. with error %s", strerror(errno));
            }
        } else {
            ret = epoll_loop_add_handler(&descr, ts.stdin_reader, console_cb_stdio_copy, &ts);
            if (ret != 0) {
                INFO("Add handler for stdinfd faied. with error %s", strerror(errno));
            }
        }
    }

    if (fifooutfd >= 0) {
        ts.stdout_reader = fifooutfd;
        ts.stdout_writer.context = &stdoutfd;
        ts.stdout_writer.write_func = fd_write_function;
        ret = epoll_loop_add_handler(&descr, ts.stdout_reader, console_cb_stdio_copy, &ts);
        if (ret != 0) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }

    if (fifoerrfd >= 0) {
        ts.stderr_reader = fifoerrfd;
        ts.stderr_writer.context = &stderrfd;
        ts.stderr_writer.write_func = fd_write_function;
        ret = epoll_loop_add_handler(&descr, ts.stderr_reader, console_cb_stdio_copy, &ts);
        if (ret != 0) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }

    ret = safe_epoll_loop(&descr);

err_out:
    client_console_tty_state_close(&descr, &ts);
    epoll_loop_close(&descr);
    return ret;
}

/* console loop copy */
int console_loop_io_copy_reader(int sync_fd, const int *srcfds, volatile struct queue_buffers *buffers,
                                struct io_write_wrapper *writers, size_t len)
{
    int ret = 0;
    size_t i = 0;
    struct epoll_descr descr;
    struct tty_state *ts = NULL;

    if (len > (SIZE_MAX / sizeof(struct tty_state)) - 1) {
        ERROR("Invalid io size");
        return -1;
    }
    ts = util_common_calloc_s(sizeof(struct tty_state) * (len + 1));
    if (ts == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    ret = epoll_loop_open(&descr);
    if (ret != 0) {
        ERROR("Create epoll_loop error");
        free(ts);
        return ret;
    }

    for (i = 0; i < len; i++) {
        // Reusing ts.stdout_reader and ts.stdout_writer for coping io
        ts[i].stdout_reader = srcfds[i];
        ts[i].stdout_writer.context = writers[i].context;
        ts[i].stdout_writer.write_func = writers[i].write_func;
        ts[i].sync_fd = -1;
        ts[i].buffers = buffers;
        ret = epoll_loop_add_handler(&descr, ts[i].stdout_reader, console_cb_stdio_copy_from_runtime, &ts[i]);
        if (ret != 0) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }
    if (sync_fd >= 0) {
        ts[i].sync_fd = sync_fd;
        ret = epoll_loop_add_handler(&descr, ts[i].sync_fd, console_cb_stdio_copy, &ts[i]);
        if (ret != 0) {
            ERROR("Add handler for syncfd failed");
            goto err_out;
        }
    }

    ret = safe_epoll_loop(&descr);

err_out:
    for (i = 0; i < (len + 1); i++) {
        epoll_loop_del_handler(&descr, ts[i].stdout_reader);
    }
    epoll_loop_close(&descr);
    free(ts);
    return ret;
}

static void forward_data_to_client(circular_queue_buffer *queue_buffer, int fd, struct io_write_wrapper *writers,
                                   size_t len)
{
    size_t i;
    size_t buff_len = 0;

    if (circular_queue_length(queue_buffer) == 0) {
        return;
    }

    for (i = 0; i < len; i++) {
        if (*(int *)writers[i].context == fd) {
            char *data = (char *)circular_queue_back(queue_buffer, &buff_len);
            while (console_writer_write_data(&writers[i], data, buff_len) != 0) {
                DEBUG("Failed to forward data to client, retring...");
                return;
            };
            free(data);
            data = NULL;
            circular_queue_pop(queue_buffer);
        }
    }
}

/* console loop copy write */
void console_loop_io_copy_writer(int sync_fd, const int *srcfds, volatile bool *iocopy_reader_finish,
                                 volatile struct queue_buffers *buffers, struct io_write_wrapper *writers, size_t len)
{
    while (true) {
        sem_wait(buffers->io_sem);

        forward_data_to_client(buffers->stdout_queue_buffer, buffers->outfd, writers, len);
        forward_data_to_client(buffers->stderr_queue_buffer, buffers->errfd, writers, len);

        if (*iocopy_reader_finish) {
            break;
        }
    }
}
