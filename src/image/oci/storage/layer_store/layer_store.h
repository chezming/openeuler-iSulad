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

#include "console.h"
#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

struct layer_config {
    /*configs for graph driver */
    char *driver_name;
    char *driver_root;

    struct driver_mount_opts *opts;
};

struct layer {
    char *id;
    char *parent;
    char *mount_point;
    int mount_count;
    char *compressed_digest;
    int64_t compress_size;
    char *uncompressed_digest;
    int64_t uncompress_size;
};

struct layer_opts {
    char *parent;
    char **names;
    size_t names_len;
    bool writable;
    struct driver_mount_opts *opts;
};

int layer_store_init(const struct layer_config *conf);

bool layer_check(const char *id);
int layer_create(const char *id, const struct layer_opts *opts, const struct io_read_wrapper *content, char **new_id);
int layer_delete(const char *id);
bool layer_exists(const char *id);
struct layer** layer_list();
bool layer_is_used(const char *id);
struct layer** layers_by_compress_digest(const char *digest);
struct layer** layers_by_uncompress_digest(const char *digest);
int layer_lookup(const char *name, char **found_id);
int layer_mount(const char *id, const struct driver_mount_opts *opts);
int layer_umount(const char *id, bool force);
int layer_mounted(const char *id);
int layer_set_names(const char *id, const char * const* names, size_t names_len);
struct graphdriver_status* layer_status();
int layer_try_repair_lowers(const char *id);

void free_layer(struct layer *l);
void free_layer_config(struct layer_config *conf);
void free_layer_opts(struct layer_opts *opts);

#ifdef __cplusplus
}
#endif

#endif
