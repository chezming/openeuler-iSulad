/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: zhangxiaoyu
 * Create: 2020-09-02
 * Description: provide network inspect functions
 ******************************************************************************/
#include "network_inspect.h"

const char g_cmd_network_inspect_desc[] = "Inspect networks";
const char g_cmd_network_inspect_usage[] = "inspect [flags] NETWORK [NETWORK...]";

struct client_arguments g_cmd_network_inspect_args = {
    .format = NULL,
};

int cmd_network_inspect_main(int argc, const char **argv)
{
    // TODO
    return 0;
}