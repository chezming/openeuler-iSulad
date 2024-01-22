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
 * Author: liuxu
 * Create: 2023-12-07
 * Description: provide container checkpoint common value
 ******************************************************************************/
#include "checkpoint_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <isula_libutils/log.h>
#include <isula_libutils/auto_cleanup.h>

#include "constants.h"
#include "path.h"
#include "utils.h"
#include "util_archive.h"

#ifdef ENABLE_CRI_API_V1

#define CHECKPOINT_CONFIG_FILE_LEN  4 
/* These files will be sent to new container. */
const char *g_checkpoint_config_files[CHECKPOINT_CONFIG_FILE_LEN] = { OCI_CONFIG_JSON, HOSTCONFIGJSON, CONFIG_V2_JSON, NULL };

const char **get_checkpoint_config_files(void)
{
    return g_checkpoint_config_files;
}

static char *untar_make_dst_dir()
{
    int nret;
    const char *isulad_tmpdir_env = NULL;
    char isula_tmpdir[PATH_MAX] = { 0 };
    char cleanpath[PATH_MAX] = { 0 };
    char tmp_dir[PATH_MAX] = { 0 };

    isulad_tmpdir_env = getenv("ISULAD_TMPDIR");
    if (!util_valid_isulad_tmpdir(isulad_tmpdir_env)) {
        INFO("if not setted isulad tmpdir or setted unvalid dir, use DEFAULT_ISULAD_TMPDIR");
        // if not setted isulad tmpdir, just use DEFAULT_ISULAD_TMPDIR
        isulad_tmpdir_env = DEFAULT_ISULAD_TMPDIR;
    }

    nret = snprintf(isula_tmpdir, PATH_MAX, "%s/isulad_tmpdir/%s", isulad_tmpdir_env, CRIU_DIRECTORY);
    if (nret < 0 || (size_t)nret >= PATH_MAX) {
        ERROR("Failed to snprintf");
        return NULL;
    }

    if (util_clean_path(isula_tmpdir, cleanpath, sizeof(cleanpath)) == NULL) {
        ERROR("clean path for %s failed", isula_tmpdir);
        return NULL;
    }

    nret = snprintf(tmp_dir, PATH_MAX, "%s/tar-chroot-XXXXXX", cleanpath);
    if (nret < 0 || (size_t)nret >= PATH_MAX) {
        ERROR("Failed to snprintf string");
        return NULL;
    }

    // ensure parent dir is exist
    if (util_mkdir_p(cleanpath, ISULAD_TEMP_DIRECTORY_MODE) != 0) {
        return NULL;
    }

    if (mkdtemp(tmp_dir) == NULL) {
        SYSERROR("Create temp dir failed");
        return NULL;
    }

    return util_strdup_s(tmp_dir);
}

void clean_checkpoint_files(const char *dst_path)
{
    if (dst_path == NULL) {
        return;
    }
    if (util_dir_exists(dst_path)) {
        if (util_recursive_rmdir(dst_path, 0) != 0) {
            WARN("Failed to remove directory %s", dst_path);
        }
        DEBUG("cleaned checkpoint files %s", dst_path);
    }
}

int untar_checkpoint_files(const char *image_path, const char *root_dir, char **dst_path, char **errmsg) 
{
    __isula_auto_free char *dst_path_tmp = NULL;

    if (image_path == NULL || root_dir == NULL || dst_path == NULL || errmsg == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    dst_path_tmp = untar_make_dst_dir();
    if (dst_path_tmp == NULL) {
        ERROR("Failed to make untar dir");
        return -1;
    }
    if (archive_chroot_untar(image_path, dst_path_tmp, root_dir, errmsg) != 0) {
        ERROR("Failed to untar image %s, %s", image_path, *errmsg);
        clean_checkpoint_files(dst_path_tmp);
        return -1;
    }

    *dst_path = dst_path_tmp;
    dst_path_tmp = NULL;
    return 0;
}

#endif /* ENABLE_CRI_API_V1 */