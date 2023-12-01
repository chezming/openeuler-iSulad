/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: liuxu
 * Create: 2023-12-07
 * Description: provide container checkpoint common value
 ******************************************************************************/
#ifndef DAEMON_CHECKPOINT_COMMON_H
#define DAEMON_CHECKPOINT_COMMON_H

#ifdef ENABLE_CRI_API_V1

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CRIU_DIRECTORY "isulad_checkpoint"
#define CHECKPOINT_IMAGE_DIRECTORY "checkpoint"
#define CHECKPOINT_DIFF_FILE "diff.tar"

const char **get_checkpoint_config_files(void);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_CRI_API_V1 */

#endif
