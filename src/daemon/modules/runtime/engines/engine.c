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
 * Description: provide container engine functions
 ******************************************************************************/
#include "engine.h"

#include <pthread.h>
#include <isula_libutils/isulad_daemon_configs.h>
#include <string.h>
#include <strings.h>

#include "constants.h"
#include "isulad_config.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "lcr_engine.h"
#include "err_msg.h"
#include "daemon_arguments.h"
#include "utils_file.h"

struct isulad_engine_operation {
    pthread_rwlock_t isulad_engines_op_rwlock;
    struct engine_operation *op;
};

static struct isulad_engine_operation g_isulad_engines;

/* engine global init */
int engines_global_init()
{
    int ret = 0;

    (void)memset(&g_isulad_engines, 0, sizeof(struct isulad_engine_operation));
    /* init isulad_engines_op_rwlock */

    ret = pthread_rwlock_init(&g_isulad_engines.isulad_engines_op_rwlock, NULL);
    if (ret != 0) {
        ERROR("Failed to init isulad conf rwlock");
        ret = -1;
        goto out;
    }

out:
    return ret;
}

/* engine routine log init */
static int engine_routine_log_init(const struct engine_operation *eop)
{
    int ret = 0;
    char *engine_log_path = NULL;
    struct service_arguments *args = NULL;

    if (eop == NULL || eop->engine_log_init_op == NULL) {
        ERROR("Failed to get engine log init operations");
        ret = -1;
        goto out;
    }

    engine_log_path = conf_get_engine_log_file();
    if (isulad_server_conf_rdlock()) {
        ret = -1;
        goto out;
    }

    args = conf_get_server_conf();
    if (args == NULL) {
        ERROR("Failed to get isulad server config");
        ret = -1;
        goto unlock_out;
    }

    if (engine_log_path == NULL) {
        ERROR("Log fifo path is NULL");
        ret = -1;
        goto unlock_out;
    }
    // log throught fifo, so we need disable stderr by quiet (set to 1)
    ret = eop->engine_log_init_op(args->progname, engine_log_path, args->json_confs->log_level, eop->engine_type, 1,
                                  NULL);
    if (ret != 0) {
        ret = -1;
        goto unlock_out;
    }

unlock_out:
    if (isulad_server_conf_unlock()) {
        ret = -1;
        goto out;
    }
out:
    free(engine_log_path);
    return ret;
}

/* engine operation free */
void engine_operation_free(struct engine_operation *eop)
{
    if (eop != NULL && eop->engine_type != NULL) {
        free(eop->engine_type);
        eop->engine_type = NULL;
    }
}

/* create engine root path */
static int create_engine_root_path(const char *path)
{
    int ret = -1;
#ifdef ENABLE_USERNS_REMAP
    char *tmp_path = NULL;
    char *p = NULL;
    char *userns_remap = conf_get_isulad_userns_remap();
#endif
    mode_t mode = CONFIG_DIRECTORY_MODE;

    if (path == NULL) {
        goto out;
    }

    if (util_dir_exists(path)) {
        ret = 0;
        goto out;
    }

#ifdef ENABLE_USERNS_REMAP
    if (userns_remap != NULL) {
        mode = USER_REMAP_DIRECTORY_MODE;
    }
#endif

    if (util_mkdir_p(path, mode) != 0) {
        ERROR("Unable to create engine root path: %s", path);
        goto out;
    }

#ifdef ENABLE_USERNS_REMAP
    if (userns_remap != NULL) {
        if (set_file_owner_for_userns_remap(path, userns_remap) != 0) {
            ERROR("Unable to change engine root %s owner for user remap.", path);
            goto out;
        }

        // find parent directory
        tmp_path = util_strdup_s(path);
        p = strrchr(tmp_path, '/');
        if (p == NULL) {
            ERROR("Failed to find parent directory for %s", tmp_path);
            goto out;
        }
        *p = '\0';

        if (set_file_owner_for_userns_remap(tmp_path, userns_remap) != 0) {
            ERROR("Unable to change engine root %s owner for user remap.", tmp_path);
            goto out;
        }
    }
#endif
    ret = 0;

out:
#ifdef ENABLE_USERNS_REMAP
    free(tmp_path);
    free(userns_remap);
#endif
    return ret;
}

static struct engine_operation *query_engine(const char *name)
{
    struct engine_operation *engine_op = NULL;

    if (g_isulad_engines.op != NULL && g_isulad_engines.op->engine_type != NULL &&
        strcasecmp(name, g_isulad_engines.op->engine_type) == 0) {
        engine_op = g_isulad_engines.op;
    }

    return engine_op;
}

static struct engine_operation *new_engine_locked(const char *name)
{
    struct engine_operation *engine_op = NULL;
    char *rootpath = NULL;

    /* now we just support lcr engine */
    if (strcasecmp(name, "lcr") == 0) {
        engine_op = lcr_engine_init();
    }

    if (engine_op == NULL) {
        ERROR("Failed to initialize engine or runtime: %s", name);
        isulad_set_error_message("Failed to initialize engine or runtime: %s", name);
        return NULL;
    }

    /* First init engine log */
    if (engine_routine_log_init(engine_op) != 0) {
        ERROR("Init engine: %s log failed", name);
        goto out;
    }

    rootpath = conf_get_routine_rootdir(name);
    if (rootpath == NULL) {
        ERROR("Root path is NULL");
        goto out;
    }

    if (create_engine_root_path(rootpath)) {
        ERROR("Create engine path failed");
        free(rootpath);
        goto out;
    }

    free(rootpath);
    return engine_op;

out:
    engine_operation_free(engine_op);
    free(engine_op);

    return NULL;
}

/* engines discovery */
int engines_discovery(const char *name)
{
    int ret = 0;
    struct engine_operation *engine_op = NULL;

    if (name == NULL) {
        return -1;
    }

    if (pthread_rwlock_wrlock(&g_isulad_engines.isulad_engines_op_rwlock)) {
        ERROR("Failed to acquire isulad engines list write lock");
        return -1;
    }

    engine_op = query_engine(name);
    if (engine_op != NULL) {
        goto unlock_out;
    }

    engine_op = new_engine_locked(name);
    if (engine_op == NULL) {
        ret = -1;
        goto unlock_out;
    }

    engine_operation_free(g_isulad_engines.op);
    g_isulad_engines.op = engine_op;

unlock_out:
    if (pthread_rwlock_unlock(&g_isulad_engines.isulad_engines_op_rwlock)) {
        ERROR("Failed to release isulad engines list write lock");
        ret = -1;
    }

    return ret;
}

/* engines check handler exist */
static struct engine_operation *engines_check_handler_exist(const char *name)
{
    struct engine_operation *engine_op = NULL;

    if (name == NULL) {
        goto out;
    }

    if (pthread_rwlock_rdlock(&g_isulad_engines.isulad_engines_op_rwlock)) {
        ERROR("Failed to acquire isulad engines list read lock");
        engine_op = NULL;
        goto out;
    }

    engine_op = query_engine(name);

    if (pthread_rwlock_unlock(&g_isulad_engines.isulad_engines_op_rwlock)) {
        CRIT("Failed to release isulad engines list read lock");
        engine_op = NULL;
        goto out;
    }

out:
    return engine_op;
}

/*
 * get the engine operation by engine name,
 * if not exist in the list, try to discovery it, and then get it again
 */
struct engine_operation *engines_get_handler(const char *name)
{
    struct engine_operation *engine_op = NULL;

    if (name == NULL) {
        ERROR("Runtime is NULL");
        engine_op = NULL;
        goto out;
    }

    engine_op = engines_check_handler_exist(name);
    if (engine_op != NULL) {
        goto out;
    }

    if (engines_discovery(name)) {
        engine_op = NULL;
        goto out;
    }

    engine_op = engines_check_handler_exist(name);
    if (engine_op != NULL) {
        goto out;
    }

out:
    return engine_op;
}
