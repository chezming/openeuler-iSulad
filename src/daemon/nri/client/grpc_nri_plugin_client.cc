/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: zhongtao
 * Create: 2024-03-15
 * Description: nri plugin grpc client
 ******************************************************************************/

#include "grpc_nri_plugin_client.h"

#include <grpc++/grpc++.h>
#include <iostream>
#include <memory>
#include <string>
#include <random>

#include "nri.grpc.pb.h"
#include "utils.h"
#include "isula_libutils/log.h"


NRIPluginClient::NRIPluginClient(const std::string &plugin_name, const std::string &plugin_id, const std::string &address):
    m_plugin_name(plugin_name), m_plugin_id(plugin_id), m_address(address)
{
    std::string unixPrefix(UNIX_SOCKET_PREFIX);

    // Only support unix domain socket
    if (m_address.compare(0, unixPrefix.length(), unixPrefix) != 0) {
        m_address = unixPrefix + m_address;
    }
    m_channel = grpc::CreateChannel(m_address, grpc::InsecureChannelCredentials());

    m_stub = nri::pkg::api::v1alpha1::Plugin::NewStub(m_channel);
}

auto NRIPluginClient::Configure(const ConfigureRequest &params, ConfigureResponse &resp, Errors &error) -> bool
{
    grpc::ClientContext context;
    nri::pkg::api::v1alpha1::ConfigureRequest request;
    nri::pkg::api::v1alpha1::ConfigureResponse response;
    grpc::Status status;

    status = m_stub->Configure(&context, request, &response);
    if (!status.ok()) {
        error.SetError(status.error_message());
        ERROR("Plugin %s Configure request failed, error_code: %d: %s", m_plugin_name.c_str(), status.error_code(),
              status.error_message().c_str());
        return false;
    }

    return true;

}

auto NRIPluginClient::Synchronize(const SynchronizeRequest &sandboxInfo, SynchronizeResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::Shutdown(Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::CreateContainer(const CreateContainerRequest &params, CreateContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::UpdateContainer(const UpdateContainerRequest &params, UpdateContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::StopContainer(const StopContainerRequest &params, StopContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::StateChange(const StateChangeEvent &event, Errors &error) -> bool
{
    return true;
}

auto NRIPluginClient::IsAlive() -> bool
{
    bool ret = true;
    grpc_connectivity_state state = m_channel->GetState(true);

    switch (state) {
        case GRPC_CHANNEL_IDLE:
            // 连接处于空闲状态
            break;
        case GRPC_CHANNEL_CONNECTING:
            // 连接正在尝试建立中
            break;
        case GRPC_CHANNEL_READY:
            // 连接已建立
            break;
        case GRPC_CHANNEL_TRANSIENT_FAILURE:
            ret = false;
            break;
        case GRPC_CHANNEL_SHUTDOWN:
            // Channel 已关闭
            break;
        default:
            // 其他状态
            break;
    }

    return ret;
}