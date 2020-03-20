/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: tanyifeng
 * Create: 2019-04-02
 * Description: provide overlay2 function definition
 ******************************************************************************/
#ifndef __GRAPHDRIVER_OVERLAY2_H
#define __GRAPHDRIVER_OVERLAY2_H

#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

int overlay2_init(struct graphdriver *driver, const char *drvier_home, const char **options, size_t len);

bool overlay2_is_quota_options(struct graphdriver *driver, const char *option);

int overlay2_create_rw(const char *id, const char *parent, const struct graphdriver *driver,
                       const struct driver_create_opts *create_opts);

int overlay2_rm_layer(const char *id, const struct graphdriver *driver);

char *overlay2_mount_layer(const char *id, const struct graphdriver *driver,
                           const struct driver_mount_opts *mount_opts);

int overlay2_umount_layer(const char *id, const struct graphdriver *driver);

bool overlay2_layer_exists(const char *id, const struct graphdriver *driver);

int overlay2_apply_diff(const char *id, const struct graphdriver *driver, const struct io_read_wrapper *content,
                        int64_t *layer_size);

int overlay2_get_layer_metadata(const char *id, const struct graphdriver *driver, json_map_string_string *map_info);

int overlay2_get_driver_status(const struct graphdriver *driver, struct graphdriver_status *status);

int overlay2_clean_up(const struct graphdriver *driver);

#ifdef __cplusplus
}
#endif

#endif

