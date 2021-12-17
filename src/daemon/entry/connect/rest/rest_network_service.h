/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2021. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: zhangxiaoyu
 * Create: 2020-09-17
 * Description: provide network restful service definition
 ******************************************************************************/
#ifndef DAEMON_ENTRY_CONNECT_REST_REST_NETWORK_SERVICE_H
#define DAEMON_ENTRY_CONNECT_REST_REST_NETWORK_SERVICE_H

#include <evhtp.h>

#ifdef __cplusplus
extern "C" {
#endif

int rest_register_network_handler(evhtp_t *htp);

#ifdef __cplusplus
}
#endif

#endif // DAEMON_ENTRY_CONNECT_REST_REST_NETWORK_SERVICE_H

