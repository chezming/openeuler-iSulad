/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: lifeng
 * Create: 2019-11-14
 * Description: provide runtime functions
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "runtime_api.h"
#include "engine.h"
#include "isulad_config.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "lcr_rt_ops.h"
#include "isula_rt_ops.h"
#ifdef ENABLE_SHIM_V2
#include "shim_rt_ops.h"
#endif

static const struct rt_ops g_lcr_rt_ops = {
    .detect = rt_lcr_detect,
    .rt_create = rt_lcr_create,
    .rt_start = rt_lcr_start,
    .rt_restart = rt_lcr_restart,
    .rt_clean_resource = rt_lcr_clean_resource,
    .rt_rm = rt_lcr_rm,
    .rt_status = rt_lcr_status,
    .rt_exec = rt_lcr_exec,
    .rt_pause = rt_lcr_pause,
    .rt_resume = rt_lcr_resume,
    .rt_attach = rt_lcr_attach,
    .rt_update = rt_lcr_update,
    .rt_listpids = rt_lcr_listpids,
    .rt_resources_stats = rt_lcr_resources_stats,
    .rt_resize = rt_lcr_resize,
    .rt_exec_resize = rt_lcr_exec_resize,
    .rt_kill = rt_lcr_kill,
    .rt_rebuild_config = rt_lcr_rebuild_config,
    .rt_checkpoint = rt_lcr_checkpoint,
};

static const struct rt_ops g_isula_rt_ops = {
    .detect = rt_isula_detect,
    .rt_create = rt_isula_create,
    .rt_start = rt_isula_start,
    .rt_restart = rt_isula_restart,
    .rt_clean_resource = rt_isula_clean_resource,
    .rt_rm = rt_isula_rm,
    .rt_status = rt_isula_status,
    .rt_exec = rt_isula_exec,
    .rt_pause = rt_isula_pause,
    .rt_resume = rt_isula_resume,
    .rt_attach = rt_isula_attach,
    .rt_update = rt_isula_update,
    .rt_listpids = rt_isula_listpids,
    .rt_resources_stats = rt_isula_resources_stats,
    .rt_resize = rt_isula_resize,
    .rt_exec_resize = rt_isula_exec_resize,
    .rt_kill = rt_isula_kill,
    .rt_rebuild_config = rt_isula_rebuild_config,
    .rt_checkpoint = rt_isula_checkpoint,
};

#ifdef ENABLE_SHIM_V2
static const struct rt_ops g_shim_rt_ops = {
    .detect = rt_shim_detect,
    .rt_create = rt_shim_create,
    .rt_start = rt_shim_start,
    .rt_restart = rt_shim_restart,
    .rt_clean_resource = rt_shim_clean_resource,
    .rt_rm = rt_shim_rm,
    .rt_status = rt_shim_status,
    .rt_exec = rt_shim_exec,
    .rt_pause = rt_shim_pause,
    .rt_resume = rt_shim_resume,
    .rt_attach = rt_shim_attach,
    .rt_update = rt_shim_update,
    .rt_listpids = rt_shim_listpids,
    .rt_resources_stats = rt_shim_resources_stats,
    .rt_resize = rt_shim_resize,
    .rt_exec_resize = rt_shim_exec_resize,
    .rt_kill = rt_shim_kill,
    .rt_rebuild_config = rt_shim_rebuild_config,
    .rt_checkpoint = rt_shim_checkpoint,
};
#endif

static const struct rt_ops *g_rt_ops[] = {
    &g_lcr_rt_ops,
#ifdef ENABLE_SHIM_V2
    &g_shim_rt_ops,
#endif
    &g_isula_rt_ops,
};

static const size_t g_rt_nums = sizeof(g_rt_ops) / sizeof(struct rt_ops *);

static const struct rt_ops *rt_ops_query(const char *runtime)
{
    size_t i;

    for (i = 0; i < g_rt_nums; i++) {
        bool r = g_rt_ops[i]->detect(runtime);
        if (r) {
            break;
        }
    }

    if (i == g_rt_nums) {
        return NULL;
    }
    return g_rt_ops[i];
}

int runtime_create(const char *name, const char *runtime, const rt_create_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime create");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops for %s", runtime);
        ret = -1;
        goto out;
    }

    ret = ops->rt_create(name, runtime, params);

out:
    return ret;
}

int runtime_start(const char *name, const char *runtime, const rt_start_params_t *params, pid_ppid_info_t *pid_info)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL || pid_info == NULL) {
        ERROR("Invalid arguments for runtime start");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_start(name, runtime, params, pid_info);

out:
    return ret;
}

int runtime_kill(const char *name, const char *runtime, const rt_kill_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime kill");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_kill(name, runtime, params);

out:
    return ret;
}

int runtime_restart(const char *name, const char *runtime, const rt_restart_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime restart");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_restart(name, runtime, params);

out:
    return ret;
}

int runtime_clean_resource(const char *name, const char *runtime, const rt_clean_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime clean");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_clean_resource(name, runtime, params);

out:
    return ret;
}

int runtime_rm(const char *name, const char *runtime, const rt_rm_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime rm");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_rm(name, runtime, params);

out:
    return ret;
}

int runtime_status(const char *name, const char *runtime, const rt_status_params_t *params,
                   struct runtime_container_status_info *status)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL || status == NULL) {
        ERROR("Invalid arguments for runtime status");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_status(name, runtime, params, status);

out:
    return ret;
}

int runtime_resources_stats(const char *name, const char *runtime, const rt_stats_params_t *params,
                            struct runtime_container_resources_stats_info *rs_stats)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL || rs_stats == NULL) {
        ERROR("Invalid arguments for runtime stats");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_resources_stats(name, runtime, params, rs_stats);

out:
    return ret;
}

int runtime_exec(const char *name, const char *runtime, const rt_exec_params_t *params, int *exit_code)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL || exit_code == NULL) {
        ERROR("Invalid arguments for runtime exec");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_exec(name, runtime, params, exit_code);

out:
    return ret;
}

int runtime_pause(const char *name, const char *runtime, const rt_pause_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime pause");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_pause(name, runtime, params);

out:
    return ret;
}

int runtime_resume(const char *name, const char *runtime, const rt_resume_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime resume");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_resume(name, runtime, params);

out:
    return ret;
}

int runtime_attach(const char *name, const char *runtime, const rt_attach_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime attach");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_attach(name, runtime, params);

out:
    return ret;
}

int runtime_update(const char *name, const char *runtime, const rt_update_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime update");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_update(name, runtime, params);

out:
    return ret;
}

int runtime_checkpoint(const char *name, const char *runtime, const rt_checkpoint_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime checkpoint");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_checkpoint(name, runtime, params);

out:
    return ret;
}

void free_rt_listpids_out_t(rt_listpids_out_t *out)
{
    if (out == NULL) {
        return;
    }

    free(out->pids);
    out->pids = NULL;
    free(out);
}

int runtime_listpids(const char *name, const char *runtime, const rt_listpids_params_t *params, rt_listpids_out_t *out)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL || out == NULL) {
        ERROR("Invalid arguments for runtime listpids");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_listpids(name, runtime, params, out);

out:
    return ret;
}

int runtime_rebuild_config(const char *name, const char *runtime, const rt_rebuild_config_params_t *params)
{
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime rebuild config");
        return -1;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        return -1;
    }

    return ops->rt_rebuild_config(name, runtime, params);
}

int runtime_resize(const char *name, const char *runtime, const rt_resize_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime resize");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_resize(name, runtime, params);

out:
    return ret;
}

int runtime_exec_resize(const char *name, const char *runtime, const rt_exec_resize_params_t *params)
{
    int ret = 0;
    const struct rt_ops *ops = NULL;

    if (name == NULL || runtime == NULL || params == NULL) {
        ERROR("Invalid arguments for runtime exec resize");
        ret = -1;
        goto out;
    }

    ops = rt_ops_query(runtime);
    if (ops == NULL) {
        ERROR("Failed to get runtime ops");
        ret = -1;
        goto out;
    }

    ret = ops->rt_exec_resize(name, runtime, params);

out:
    return ret;
}

bool is_default_runtime(const char *name)
{
    const char *runtimes[] = { "lcr", "runc", "kata-runtime" };
    int i = 0;

    for (; i < sizeof(runtimes) / sizeof(runtimes[0]); ++i) {
        if (strcmp(name, runtimes[i]) == 0) {
            return true;
        }
    }

#ifdef ENABLE_GVISOR
    if (strcmp(name, "runsc") == 0) {
        return true;
    }
#endif

#ifdef ENABLE_SHIM_V2
    if (is_valid_v2_runtime(name)) {
        return true;
    }
#endif
    ERROR("runtime %s is not supported", name);

    return false;
}

int runtime_init(void)
{
    if (engines_global_init()) {
        ERROR("Init engines global failed");
        return -1;
    }

    return 0;
}
