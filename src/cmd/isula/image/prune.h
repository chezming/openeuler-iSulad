/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
 *
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Author: Chen Wei
 * Create: 2023-05-11
 * Description: command image prune
 ******************************************************************************/

#ifndef CMD_ISULA_IMAGE_PRUNE_H
#define CMD_ISULA_IMAGE_PRUNE_H

#include <stdbool.h>
#include <stddef.h>

#include "client_arguments.h"
#include "command_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PRUNE_OPTIONS(cmdargs) \
    {CMD_OPT_TYPE_BOOL, false, "force", 'f', &(cmdargs).force, "Do not prompt for confirmation", NULL}, \
    {CMD_OPT_TYPE_BOOL, false, "all", 'a', &(cmdargs).all, "Remove all unused images, not just dangling ones", NULL}, \
    {CMD_OPT_TYPE_CALLBACK, false, "filter", 'l', &(cmdargs).filters,    \
     "Filter until. Remove unused images created befor until time. The format is like 1970-01-30T00:00:00", command_append_array}


extern const char g_cmd_image_prune_desc[];
extern const char g_cmd_image_prune_usage[];
extern struct client_arguments g_cmd_image_prune_args;
int cmd_image_prune_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif // CMD_ISULA_IMAGE_PRUNE_H
