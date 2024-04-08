/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: liuxu
 * Create: 2024-03-06
 * Description: provide cdi annotations function
 ******************************************************************************/
#include "cdi_annotations.h"

#include <ctype.h>
#include <isula_libutils/log.h>
#include <isula_libutils/auto_cleanup.h>
#include <isula_libutils/utils_string.h>

#include "error.h"
#include "utils.h"
#include "cdi_parser.h"

#define CDI_ANNOTATIONS_PREFIX "cdi.k8s.io/"

char *cdi_parse_annotations(json_map_string_string *annotations, string_array **keys, string_array **devices)
{
    return NULL;
}
