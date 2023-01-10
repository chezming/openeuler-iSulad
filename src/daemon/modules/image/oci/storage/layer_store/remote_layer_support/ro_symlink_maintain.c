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
 * Author: wangrunze
 * Create: 2023-01-12
 * Description: provide remote layer store support
 ******************************************************************************/
#define _GNU_SOURCE
#include "map.h"
#include <stdio.h>

#include "ro_symlink_maintain.h"
#include "stdlib.h"
#include "isula_libutils/log.h"
#include "layer.h"
#include "layer_store.h"
#include "image_store.h"
#include "remote_support.h"
#include "utils.h"
#include "utils_file.h"
#include "path.h"
#include <pthread.h>
#include <unistd.h>

#define REMOTE_RO_LAYER_DIR "RO"

// overlay-layers and overlay-layers/RO
static char *layer_ro_dir;
static char *layer_home;

// overlay and overlay/RO
static char *overlay_ro_dir;
static char *overlay_home;

struct supporters {
    RemoteSupporter *image_supporter;
    RemoteSupporter *layer_supporter;
    RemoteSupporter *overlay_supporter;
};

static struct supporters supporters;


int remote_layer_init(const char *root_dir)
{
    if (root_dir == NULL) {
        goto out;
    }

    layer_home = util_strdup_s(root_dir);
    layer_ro_dir = util_path_join(root_dir, REMOTE_RO_LAYER_DIR);
    if (layer_ro_dir == NULL) {
        ERROR("Failed join path when init remote layer maintainer");
        goto out;
    }

    return 0;

out:
    remote_maintain_cleanup();
    return -1;
}

int remote_overlay_init(const char *driver_home)
{
    if (driver_home == NULL) {
        goto out;
    }

    overlay_home = util_strdup_s(driver_home);
    overlay_ro_dir = util_path_join(driver_home, REMOTE_RO_LAYER_DIR);
    if (overlay_ro_dir == NULL) {
        ERROR("Failed to join path when init remote maintainer");
        goto out;
    }

    return 0;

out:
    remote_maintain_cleanup();
    return -1;
}

void remote_maintain_cleanup()
{
    free(layer_home);
    layer_home = NULL;
    free(layer_ro_dir);
    layer_ro_dir = NULL;
    free(overlay_home);

    overlay_home = NULL;
    free(overlay_ro_dir);
    overlay_ro_dir = NULL;
}

// static bool overlay_walk_dir_cb(const char *path_name, const struct dirent *sub_dir, void *context)
// {
//     char *ro_symlink = NULL;
//     char *layer_dir = NULL;
//     char *root_dir = context;
//     bool ret = true;

//     ro_symlink = util_path_join(root_dir, sub_dir->d_name);
//     layer_dir = util_path_join(path_name, sub_dir->d_name);

//     if (ro_symlink == NULL) {
//         ERROR("Failed to join ro symlink path: %s", sub_dir->d_name);
//         ret = false;
//         goto free_out;
//     }

//     if (layer_dir == NULL) {
//         ERROR("Failed to join ro layer dir: %s", sub_dir->d_name);
//         ret = false;
//         goto free_out;
//     }

//     if (!util_fileself_exists(ro_symlink) && symlink(layer_dir, ro_symlink) != 0) {
//         SYSERROR("Unable to create symbol link to layer directory: %s", layer_dir);
//         ret = false;
//     }

// free_out:
//     free(ro_symlink);
//     free(layer_dir);

//     return ret;
// }

// static bool layer_walk_dir_cb(const char *path_name, const struct dirent *sub_dir, void *context)
// {
//     int nret = 0;

//     char *ro_symlink = NULL;
//     char *layer_dir = NULL;
//     char *root_dir = context;
//     bool ret = true;
//
//     ro_symlink = util_path_join(root_dir, sub_dir->d_name);
//     layer_dir = util_path_join(path_name, sub_dir->d_name);

//     if (ro_symlink == NULL) {
//         ERROR("Failed to join ro symlink path: %s", sub_dir->d_name);
//         ret = false;
//         goto free_out;
//     }

//     if (layer_dir == NULL) {
//         ERROR("Failed to join ro layer dir: %s", sub_dir->d_name);
//         ret = false;
//         goto free_out;
//     }

//     if (util_fileself_exists(ro_symlink)) {
//         ret = true;
//         goto free_out;
//     }

//     if (symlink(layer_dir, ro_symlink) != 0) {
//         SYSERROR("Unable to create symbol link to layer directory: %s", layer_dir);
//         ret = false;
//         goto free_out;
//     }

//     nret = load_one_layer(sub_dir->d_name);
//     if (nret != 0) {
//         ERROR("Failed to load layer: %s into memory when updating remote layer", sub_dir->d_name);
//         ret = false;
//     }

// free_out:
//     free(ro_symlink);
//     free(layer_dir);

//     return ret;
// }

// to maintain the symbol links, add new symbol link and delete invalid symbol link
// arg is const char *driver_home
// scanning driver->home/RO/ directory, build symlink in driver->home
static void *remote_refresh_ro_symbol_link(void *arg)
{
    struct supporters *supporters = (struct supporters *)arg;
    int ret = 0;
    while (true) {
        util_usleep_nointerupt(5 * 1000 * 1000);
        printf("remote refresh\n");

        ret = scan_remote_dir(supporters->image_supporter);
        ret = scan_remote_dir(supporters->overlay_supporter);
        ret = load_item(supporters->overlay_supporter);
        // printf("scan overlay dir: %d\n", ret);
        ret = scan_remote_dir(supporters->layer_supporter);
        ret = load_item(supporters->layer_supporter);
        // printf("scan layer dir: %d\n", ret);
        ret = load_item(supporters->image_supporter);
        // printf("scan image dir: %d\n", ret);
        remote_load_new_images_from_json();
    }
    return NULL;
}

int start_refresh_thread(void)
{
    int res = 0;
    pthread_t a_thread;

    supporters.image_supporter = create_image_supporter(NULL, NULL);
    supporters.layer_supporter = create_layer_supporter(layer_home, layer_ro_dir);
    supporters.overlay_supporter = create_overlay_supporter(overlay_home, overlay_ro_dir);

    if (supporters.image_supporter == NULL) {
        goto free_out;
    }

    if (supporters.layer_supporter == NULL) {
        goto free_out;
    }

    if (supporters.overlay_supporter == NULL) {
        goto free_out;
    }

    scan_remote_dir(supporters.overlay_supporter);
    scan_remote_dir(supporters.layer_supporter);

    res = pthread_create(&a_thread, NULL, remote_refresh_ro_symbol_link, (void *)&supporters);
    if (res != 0) {
        CRIT("Thread creation failed");
        return -1;
    }

    if (pthread_detach(a_thread) != 0) {
        SYSERROR("Failed to detach 0x%lx", a_thread);
        return -1;
    }

    return 0;

free_out:
    destroy_suppoter(supporters.image_supporter);
    destroy_suppoter(supporters.layer_supporter);
    destroy_suppoter(supporters.overlay_supporter);

    return -1;
}

static int do_build_ro_dir(const char *home, const char *id)
{
    char *ro_symlink = NULL;
    char *ro_layer_dir = NULL;
    int nret = 0;
    // bool ret = true;
    int ret = 0;

    nret = asprintf(&ro_symlink, "%s/%s", home, id);
    if (nret < 0 || nret > PATH_MAX) {
        SYSERROR("Failed create ro layer dir sym link path");
        return -1;
    }

    nret = asprintf(&ro_layer_dir, "%s/%s/%s", home, REMOTE_RO_LAYER_DIR, id);
    if (nret < 0 || nret > PATH_MAX) {
        SYSERROR("Failed to create ro layer dir path");
        return -1;
    }

    if (util_mkdir_p(ro_layer_dir, IMAGE_STORE_PATH_MODE) != 0) {
        ret = -1;
        ERROR("Failed to create layer direcotry %s", ro_layer_dir);
        goto out;
    }

    if (symlink(ro_layer_dir, ro_symlink) != 0) {
        ret = -1;
        SYSERROR("Failed to create symlink to layer dir %s", ro_layer_dir);
        goto err_out;
    }

    goto out;

err_out:
    if (util_recursive_rmdir(ro_layer_dir, 0)) {
        ERROR("Failed to delete layer path: %s", ro_layer_dir);
    }

out:
    free(ro_layer_dir);
    free(ro_symlink);
    return ret;
}

int remote_overlay_build_ro_dir(const char *id)
{
    return do_build_ro_dir(overlay_home, id);
}

int remote_layer_build_ro_dir(const char *id)
{
    return do_build_ro_dir(layer_home, id);
}

int do_remove_ro_dir(const char *home, const char *id)
{
    char *ro_layer_dir = NULL;
    char *ro_symlink = NULL;
    char clean_path[PATH_MAX] = { 0 };
    int ret = 0;
    int nret = 0;

    if (id == NULL) {
        return 0;
    }

    nret = asprintf(&ro_symlink, "%s/%s", home, id);
    if (nret < 0 || nret > PATH_MAX) {
        SYSERROR("Create layer sym link path failed");
        return -1;
    }

    if (util_clean_path(ro_symlink, clean_path, sizeof(clean_path)) == NULL) {
        ERROR("Failed to clean path: %s", ro_symlink);
        ret = -1;
        goto out;
    }

    if (util_path_remove(clean_path) != 0) {
        SYSERROR("Failed to remove link path %s", clean_path);
    }

    nret = asprintf(&ro_layer_dir, "%s/%s/%s", home, REMOTE_RO_LAYER_DIR, id);
    if (nret < 0 || nret > PATH_MAX) {
        SYSERROR("Create layer json path failed");
        ret = -1;
        goto out;
    }

    ret = util_recursive_rmdir(ro_layer_dir, 0);

out:
    free(ro_layer_dir);
    free(ro_symlink);
    return ret;
}

int remote_layer_remove_ro_dir(const char *id)
{
    return do_remove_ro_dir(layer_home, id);
}

int remote_overlay_remove_ro_dir(const char *id)
{
    return do_remove_ro_dir(overlay_home, id);
}
