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
 * Description: provide container package configure definition
 ******************************************************************************/
#ifndef CLIENT_CONNECT_PACK_CONFIG_H
#define CLIENT_CONNECT_PACK_CONFIG_H

#include "libisula.h"

#ifdef __cplusplus
extern "C" {
#endif

int generate_hostconfig(const isula_host_config_t *srcconfig, char **hostconfigstr);

int generate_container_config(const isula_container_config_t *custom_conf,
                              char **container_config_str);

#ifdef __cplusplus
}
#endif

#endif

