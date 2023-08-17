/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: xuxuepeng
 * Create: 2023-08-17
 * Description: shim v2 runtime monitor definition
 ******************************************************************************/

#include "shim_rt_monitor.h"

#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <shim_v2.h>
#include <isula_libutils/auto_cleanup.h>
#include "isula_libutils/log.h"

#include "utils.h"
#include "error.h"

struct shim_monitor_data {
    char *id;
    char *exec_id;
    char *exit_fifo;
};

static void free_shim_monitor_data(struct shim_monitor_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->id != NULL) {
        free(data->id);
    }
    if (data->exec_id != NULL) {
        free(data->exec_id);
    }
    if (data->exit_fifo != NULL) {
        free(data->exit_fifo);
    }
    free(data);
}

static int write_to_exit_fifo(const char *exit_fifo, int exit_status)
{
    int ret = 0;
    int exit_fifo_fd;
 
    if (exit_fifo == NULL) {
        return -1;
    }
 
    if (!util_file_exists(exit_fifo)) {
        ERROR("Exit FIFO %s does not does not exist", exit_fifo);
        return -1;
    }
 
    exit_fifo_fd = util_open(exit_fifo, O_WRONLY | O_NONBLOCK, 0);
    if (exit_fifo_fd < 0) {
        ERROR("Failed to open exit FIFO %s: %s.", exit_fifo, strerror(errno));
        return -1;
    }
 
    if (util_write_nointr(exit_fifo_fd, &exit_status, sizeof(int)) <= 0) {
        ERROR("Failed to write exit fifo fd %s", exit_fifo);
        ret = -1;
    }
 
    close(exit_fifo_fd);
    return ret;
}

static void *shim_monitor_thread(void *args)
{
    int exit_status = 0;
    __isula_auto_free struct shim_monitor_data *datap = (struct shim_monitor_data *)args;
    if (shim_v2_wait(datap->id, datap->exec_id, &exit_status) != 0) {
        ERROR("%s: failed to wait for container", datap->id);
    }
 
    if (write_to_exit_fifo(datap->exit_fifo, exit_status) != 0) {
        ERROR("Failed to write to exit fifo %s", datap->exit_fifo);
    } else {
        DEBUG("Write to exit fifo successfully, %s", datap->exit_fifo);
    }
    return NULL;
}

int shim_rt_monitor(const char *id, const char *exec_id, const char *exit_fifo)
{
    pthread_t tid;
    pthread_attr_t attr;
    struct shim_monitor_data *data = NULL;
 
    if (id == NULL || exit_fifo == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    if (!util_file_exists(exit_fifo)) {
        ERROR("Exit FIFO %s does not does not exist", exit_fifo);
        return -1;
    }
 
    data = util_common_calloc_s(sizeof(struct shim_monitor_data));
    if (data == NULL) {
        ERROR("Out of memory");
        return -1;
    }
 
    data->id = util_strdup_s(id);
    data->exec_id = util_strdup_s(exec_id);
    data->exit_fifo = util_strdup_s(exit_fifo);
 
    if (pthread_attr_init(&attr) != 0) {
        ERROR("Failed to init pthread attr");
        goto err_out;
    }
 
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        ERROR("Failed to set pthread attr");
        goto err_out;
    }
 
    if (pthread_create(&tid, &attr, shim_monitor_thread, data) != 0) {
        ERROR("Failed to create monitor thread");
        goto err_out;
    }

    return 0;

err_out:
    free_shim_monitor_data(data);
    return -1;
}
