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
 * Description: provide websocket server functions
 ******************************************************************************/

#include "stream_server.h"
#include <memory>
#include <string>
#include "exec_serve.h"
#include "attach_serve.h"
#include "ws_server.h"
#include "isulad_config.h"
#ifdef ENABLE_PORTFORWARD
#include "portforward_serve.h"
#endif

static url::URLDatum m_url;

void cri_stream_server_init(Errors &err)
{
    int streamPort = conf_get_websocket_server_listening_port();
    if (streamPort == 0) {
        err.SetError("Failed to get stream server listening port from daemon config");
        return;
    }
    auto *server = WebsocketServer::GetInstance();
    // set listen port before get Url, becasue Url format by listen port
    server->SetListenPort(streamPort);

    m_url = server->GetWebsocketUrl();
    // register support service
    server->RegisterCallback(std::string("exec"), std::make_shared<ExecServe>());
    server->RegisterCallback(std::string("attach"), std::make_shared<AttachServe>());
#ifdef ENABLE_PORTFORWARD
    server->RegisterCallback(std::string("portforward"), std::make_shared<PortForwardServe>());
#endif

    server->Start(err);
}

void cri_stream_server_wait(void)
{
    auto *server = WebsocketServer::GetInstance();
    server->Wait();
}

void cri_stream_server_shutdown(void)
{
    auto *server = WebsocketServer::GetInstance();
    server->Shutdown();
}

url::URLDatum cri_stream_server_url(void)
{
    return m_url;
}

auto BuildURL(const std::string &method, const std::string &token) -> std::string
{
    url::URLDatum url;
    url.SetPathWithoutEscape("/cri/" + method + "/" + token);

    return cri_stream_server_url().ResolveReference(&url)->String();
}

