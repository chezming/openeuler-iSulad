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
 * Create: 2023-06-08
 * Description: provide helper function for container service
 ******************************************************************************/

#include <isula_libutils/container_config_v2.h>
#include <isula_libutils/auto_cleanup.h>

#include "container_helper.h"
#include "sandbox_api.h"
#include "controller_api.h"
#include "namespace.h"
#include "utils_mount_spec.h"

static int generate_ctrl_rootfs(const container_t *cont, ctrl_mount_t **rootfs, size_t *mounts_len)
{
    ctrl_mount_t *rootfs_mounts = NULL;

    if (cont->common_config->base_fs == NULL) {
        ERROR("Container %s has no base fs", cont->common_config->id);
        return -1;
    }

    rootfs_mounts = util_common_calloc_s(sizeof(ctrl_mount_t));
    if (rootfs_mounts == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    *mounts_len = 1;

    rootfs_mounts->source = cont->common_config->base_fs;
    // isulad mounts rootfs for container, so we use bind mount here
    rootfs_mounts->type = MOUNT_TYPE_BIND;

    *rootfs = rootfs_mounts;
    return 0;
}

static int validate_target_container(const container_t *container)
{
    int ret = 0;
    container_t *connected = NULL;
    char *connected_id = namespace_get_connected_container(container->hostconfig->pid_mode);
    if (connected_id != NULL) {
        connected = containers_store_get(connected_id);
        if (connected == NULL) {
            ERROR("Failed to get connected container, '%s'", connected_id);
            ret = -1;
            goto out;
        }
        if (connected->sandbox_id == NULL || strcmp(connected->sandbox_id, container->sandbox_id) != 0) {
            ERROR("Connected container are not in the sandbox, '%s'", connected_id);
            ret = -1;
        }
    }
out:
    container_unref(connected);
    free(connected_id);
    return ret;
}

static int do_sandbox_preparation(const container_t *cont, const char *exec_id, const char *oci_spec,
                                  const char *console_fifos[], bool tty)
{
    size_t rootfs_len;
    ctrl_prepare_params_t params = {0};
    ctrl_prepare_response_t response = {0};
    __isula_auto_free ctrl_mount_t* rootfs = NULL;
    __auto_cleanup_sandbox_ptr_t sandbox = NULL;
    // Check if sandbox is used
    if (cont->sandbox_id == NULL) {
        return 0;
    }

    sandbox = sandboxes_store_get(cont->sandbox_id);
    if (sandbox == NULL) {
        ERROR("Failed to get sandbox '%s' for preparing container", cont->sandbox_id);
        return -1;
    }

    if (validate_target_container(cont) != 0) {
        ERROR("Invalid target container");
        return -1;
    }

    if (generate_ctrl_rootfs(cont, &rootfs, &rootfs_len) != 0) {
        ERROR("Failed to generate rootfs mount points for appending container, %s", cont->common_config->id);
        return -1;
    }

    params.container_id = cont->common_config->id;
    if (exec_id != NULL) {
        params.exec_id = exec_id;
    }
    params.oci_spec = oci_spec;
    params.stdin = console_fifos[0];
    params.stdout = console_fifos[1];
    params.stderr = console_fifos[2];
    params.terminal = tty;
    params.rootfs = rootfs;
    params.rootfs_len = rootfs_len;

    if (sandbox_ctrl_prepare(sandbox->sandboxer, cont->sandbox_id, &params, &response) != 0) {
        ERROR("Failed to prepare container '%s' for sandbox '%s'", cont->common_config->id, cont->sandbox_id);
        return -1;
    }

    INFO("Successfully prepared container %s for sandbox %s", cont->common_config->id, cont->sandbox_id);

    return 0;
}

static int do_sandbox_purge(const container_t *cont, const char *exec_id)
{
    ctrl_purge_params_t params = {0};
    __auto_cleanup_sandbox_ptr_t sandbox = NULL;

    if (cont->sandbox_task_address == NULL || cont->sandbox_id == NULL) {
        return 0;
    }

    sandbox = sandboxes_store_get(cont->sandbox_id);
    if (sandbox == NULL) {
        ERROR("Failed to get sandbox '%s' for purging container", cont->sandbox_id);
        return -1;
    }

    params.container_id = cont->common_config->id;
    if (exec_id != NULL) {
        params.exec_id = exec_id;
    }

    if (sandbox_ctrl_purge(sandbox->sandboxer, cont->sandbox_id, &params) != 0) {
        ERROR("Failed to purge container '%s' for sandbox '%s'", cont->common_config->id, cont->sandbox_id);
        return -1;
    }

    INFO("Successfully purged container %s for sandbox %s", cont->common_config->id, cont->sandbox_id);

    return 0;
}

int sandbox_prepare_container(const container_t *cont, const oci_runtime_spec *oci_spec,
                              const char *console_fifos[], bool tty)
{
    __isula_auto_free char *json_oci_spec = NULL;
    __isula_auto_free parser_error err = NULL;

    INFO("Prepare container %s for sandbox %s", cont->common_config->id, cont->sandbox_id);

    json_oci_spec = oci_runtime_spec_generate_json(oci_spec, NULL, &err);
    if (json_oci_spec == NULL) {
        ERROR("Failed to generate container spec json: %s", err);
        return -1;
    }
    return do_sandbox_preparation(cont, NULL, json_oci_spec, console_fifos, tty);
}

int sandbox_prepare_exec(const container_t *cont, const char *exec_id, defs_process *process_spec,
                         const char *console_fifos[], bool tty)
{
    __isula_auto_free char *json_process = NULL;
    __isula_auto_free parser_error err = NULL;

    INFO("Prepare exec %s for container %s in sandbox %s", exec_id, cont->common_config->id, cont->sandbox_id);

    json_process = defs_process_generate_json(process_spec, NULL, &err);
    if (json_process == NULL) {
        ERROR("Failed to generate process spec json: %s", err);
        return -1;
    }

    return do_sandbox_preparation(cont, exec_id, json_process, console_fifos, tty);
}

int sandbox_purge_container(const container_t *cont) {
    return do_sandbox_purge(cont, NULL);
}

int sandbox_purge_exec(const container_t *cont, const char *exec_id) {
    return do_sandbox_purge(cont, exec_id);
}
