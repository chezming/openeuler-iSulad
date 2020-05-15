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
 * Author: tanyifeng
 * Create: 2019-04-02
 * Description: provide graphdriver function definition
 ******************************************************************************/
#ifndef __GRAPHDRIVER_H
#define __GRAPHDRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "isula_libutils/container_inspect.h"
#include "json_common.h"
#include "console.h"
#include "driver_overlay2_types.h"
#include "storage.h"
#include "image.h"
#include "container_inspect.h"

#ifdef __cplusplus
extern "C" {
#endif

struct graphdriver;

struct driver_create_opts {
    char *mount_label;
    json_map_string_string *storage_opt;
};

struct driver_mount_opts {
    char *mount_label;
    char **options;
    size_t options_len;
};

struct graphdriver_ops {
    int (*init)(struct graphdriver *driver, const char *drvier_home, const char **options, size_t len);

    int (*create_rw)(const char *id, const char *parent, const struct graphdriver *driver,
                     struct driver_create_opts *create_opts);


    int (*create_ro)(const char *id, const char *parent, const struct graphdriver *driver,
                     const struct driver_create_opts *create_opts);

    int (*rm_layer)(const char *id, const struct graphdriver *driver);

    char *(*mount_layer)(const char *id, const struct graphdriver *driver, const struct driver_mount_opts *mount_opts);

    int (*umount_layer)(const char *id, const struct graphdriver *driver);

    bool (*exists)(const char *id, const struct graphdriver *driver);

    int (*apply_diff)(const char *id, const struct graphdriver *driver, const struct io_read_wrapper *content,
                      int64_t *layer_size);

    int (*get_layer_metadata)(const char *id, const struct graphdriver *driver, json_map_string_string *map_info);

    int (*get_driver_status)(const struct graphdriver *driver, struct graphdriver_status *status);

    int (*clean_up)(const struct graphdriver *driver);

    int (*try_repair_lowers)(const char *id, const char *parent, const struct graphdriver *driver);

    int (*get_layer_fs_info)(const char *id, const struct graphdriver *driver, imagetool_fs_info *fs_info);
};

struct graphdriver {
    // common implement
    const struct graphdriver_ops *ops;
    const char *name;
    const char *home;
    char *backing_fs;
    bool support_dtype;

    bool support_quota;
    struct pquota_control *quota_ctrl;

    // options for overlay2
    struct overlay_options *overlay_opts;
};

int graphdriver_init(const struct storage_module_init_options *opts);

int graphdriver_create_rw(const char *id, const char *parent, struct driver_create_opts *create_opts);

int graphdriver_create_ro(const char *id, const char *parent, const struct driver_create_opts *create_opts);

int graphdriver_rm_layer(const char *id);

char *graphdriver_mount_layer(const char *id, const struct driver_mount_opts *mount_opts);

int graphdriver_umount_layer(const char *id);

bool graphdriver_layer_exists(const char *id);

int graphdriver_apply_diff(const char *id, const struct io_read_wrapper *content, int64_t *layer_size);

struct graphdriver_status *graphdriver_get_status(void);

void free_graphdriver_status(struct graphdriver_status *status);

void free_graphdriver_mount_opts(struct driver_mount_opts *opts);

int graphdriver_cleanup(void);

int graphdriver_try_repair_lowers(const char *id, const char *parent);

container_inspect_graph_driver *graphdriver_get_metadata(const char *id);

int graphdriver_get_layer_fs_info(const char *id, imagetool_fs_info *fs_info);

#ifdef __cplusplus
}
#endif

#endif

