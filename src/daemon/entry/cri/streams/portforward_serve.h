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
 * Description: PortForward streaming service implementation.
 * Author: zhongtao
 * Create: 2023-09-15
 ******************************************************************************/

#ifndef DAEMON_ENTRY_CRI_STREAMS_SERVICE_PORTFORWARD_SERVE_H
#define DAEMON_ENTRY_CRI_STREAMS_SERVICE_PORTFORWARD_SERVE_H

#include "route_callback_register.h"
#include <string>
#include <isula_libutils/container_portforward_request.h>
#include <isula_libutils/container_portforward_response.h>

class PortForwardServe : public StreamingServeInterface {
public:
    PortForwardServe() = default;
    PortForwardServe(const PortForwardServe &) = delete;
    PortForwardServe &operator=(const PortForwardServe &) = delete;
    virtual ~PortForwardServe() = default;

private:
    virtual void SetServeThreadName() override;
    virtual void *SetContainerStreamRequest(StreamRequest *grequest, const std::string &suffix) override;
    virtual int ExecuteStreamCommand(SessionData *lwsCtx, void *request) override;
    virtual void CloseConnect(SessionData *lwsCtx) override;
    virtual void FreeRequest(void *m_request) override;
};
#endif // DAEMON_ENTRY_CRI_STREAMS_SERVICE_PORTFORWARD_SERVE_H

