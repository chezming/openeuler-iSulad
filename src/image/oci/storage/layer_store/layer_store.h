/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: liuhao
 * Create: 2020-03-24
 * Description: provide layer store function definition
 ******************************************************************************/
#ifndef __OCI_STORAGE_LAYER_STORE_H
#define __OCI_STORAGE_LAYER_STORE_H

#include <stdint.h>

#include "storage.h"
#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif

struct layer_store_mount_opts {
    char *mount_label;
    json_map_string_string *mount_opts;
};

struct layer_opts {
    char *parent;
    char **names;
    size_t names_len;
    bool writable;

    // mount options
    struct layer_store_mount_opts *opts;
};

struct layer_store_status {
    char *driver_name;
    char *backing_fs;
    char *status;
};

int layer_store_init(const struct storage_module_init_options *conf);

bool layer_store_check(const char *id);
int layer_store_create(const char *id, const struct layer_opts *opts, const struct io_read_wrapper *content,
                       char **new_id);
int layer_store_remove_layer(const char *id);
int layer_store_delete(const char *id);
bool layer_store_exists(const char *id);
struct layer** layer_store_list();
bool layer_store_is_used(const char *id);
struct layer** layer_store_by_compress_digest(const char *digest);
struct layer** layer_store_by_uncompress_digest(const char *digest);
char *layer_store_lookup(const char *name);
char *layer_store_mount(const char *id, const struct layer_store_mount_opts *opts);
int layer_store_umount(const char *id, bool force);
int layer_store_mounted(const char *id);
int layer_store_set_names(const char *id, const char * const* names, size_t names_len);
struct layer_store_status* layer_store_status();
int layer_store_try_repair_lowers(const char *id);

void free_layer_store_mount_opts(struct layer_store_mount_opts *ptr);
void free_layer_opts(struct layer_opts *opts);
void free_layer_store_status(struct layer_store_status *ptr);

#ifdef __cplusplus
}
#endif

#endif
