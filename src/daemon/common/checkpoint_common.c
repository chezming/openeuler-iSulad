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
#include "checkpoint_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <isula_libutils/log.h>

#include "constants.h"
#include "path.h"
#include "utils.h"

#define CHECKPOINT_CONFIG_FILE_LEN  4 
/* These files will be sent to new container. */
const char *g_checkpoint_config_files[CHECKPOINT_CONFIG_FILE_LEN] = { OCI_CONFIG_JSON, HOSTCONFIGJSON, CONFIG_V2_JSON, NULL };

const char **get_checkpoint_config_files(void)
{
    return g_checkpoint_config_files;
}