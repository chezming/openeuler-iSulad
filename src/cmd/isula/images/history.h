/******************************************************************************
 * Copyright (c) KylinSoft  Co., Ltd. 2022. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: huangsong
 * Create: 2022-10-04
 * Description: provide image history definition
 ********************************************************************************/

#ifndef CMD_ISULA_IMAGES_HISTORY_H
#define CMD_ISULA_IMAGES_HISTORY_H

#include "client_arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HISTORY_OPTIONS(cmdargs)                                                                             \
    { CMD_OPT_TYPE_BOOL, false, "quiet", 'q', &(cmdargs).dispname, "Only display numeric IDs", NULL },       \
    { CMD_OPT_TYPE_BOOL, false, "no-trunc", 0, &(cmdargs).no_trunc, "Don't truncate output", NULL },         \
    { CMD_OPT_TYPE_STRING, false, "format", 0, &(cmdargs).format, "Format the output using the given go template", NULL}

extern const char g_cmd_history_desc[];
extern const char g_cmd_history_usage[];
extern struct client_arguments g_cmd_history_args;
int client_history(const struct client_arguments *args);

int cmd_history_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif // CMD_ISULA_IMAGES_HISTORY_H