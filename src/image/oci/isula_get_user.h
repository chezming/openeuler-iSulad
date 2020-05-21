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
* Author: liuhao
* Create: 2019-07-15
* Description: isula image rootfs mount operator implement
*******************************************************************************/
#ifndef __IMAGE_ISULA_GET_USER_H
#define __IMAGE_ISULA_GET_USER_H

#include "oci_image_spec.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

int isula_get_user(const char *id, const char *basefs, host_config *hc, const char *userstr, defs_process_user *puser);

#ifdef __cplusplus
}
#endif

#endif
