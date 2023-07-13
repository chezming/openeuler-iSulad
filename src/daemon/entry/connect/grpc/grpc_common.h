/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: lifeng
 * Create: 2018-11-08
 * Description: provide grpc container functions
 ******************************************************************************/
#ifndef DAEMON_ENTRY_CONNECT_GRPC_GRPC_COMMON_H
#define DAEMON_ENTRY_CONNECT_GRPC_GRPC_COMMON_H

bool grpc_is_call_cancelled(void *context);

bool grpc_add_initial_metadata(void *context, const char *header, const char *val);

bool grpc_event_write_function(void *writer, void *data);

#endif
