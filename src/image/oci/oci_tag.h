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
* Author: jikui
* Create: 2020-05-08
* Description: isula tag operator implement
*******************************************************************************/
#ifndef __IMAGE_OCI_TAG_H
#define __IMAGE_OCI_TAG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int oci_do_tag(const char *src_name, const char *dest_name, char **errmsg);

#ifdef __cplusplus
}
#endif

#endif
