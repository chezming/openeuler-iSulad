/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2033. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Description: provide image filter functions
 ******************************************************************************/

#ifndef DAEMON_MODULES_IMAGE_FILTER_H
#define DAEMON_MODULES_IMAGE_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <isula_libutils/imagetool_image_summary.h>
#include "filters.h"

bool image_meet_dangling_filter(const imagetool_image_summary *src, const struct filters_args *filters);

#ifdef __cplusplus
}
#endif

#endif
