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

#ifndef DAEON_NRI_PLUGIN_CLIENT_H
#define DAEON_NRI_PLUGIN_CLIENT_H

#include "nri.grpc.pb.h"
#include "errors.h"
#include "nri.pb.h"

using namespace nri::pkg::api::v1alpha1;

class NRIPluginClient {
public:
    NRIPluginClient(const std::string &plugin_name, const std::string &plugin_id, const std::string &address);

    ~NRIPluginClient() = default;

    // use to ensure plugin is alive or not
    auto IsAlive(void) -> bool;

    auto Configure(const ConfigureRequest &params, ConfigureResponse &resp, Errors &error) -> bool;

    auto Synchronize(const SynchronizeRequest &sandboxInfo, SynchronizeResponse &resp, Errors &error) -> bool;

    auto Shutdown(Errors &error) -> bool;

    auto CreateContainer(const CreateContainerRequest &params, CreateContainerResponse &resp, Errors &error) -> bool;

    auto UpdateContainer(const UpdateContainerRequest &params, UpdateContainerResponse &resp, Errors &error) -> bool;

    auto StopContainer(const StopContainerRequest &params, StopContainerResponse &resp, Errors &error) -> bool;

    auto StateChange(const StateChangeEvent &event, Errors &error) -> bool;

private:

protected:
    std::string m_plugin_name;
    std::string m_plugin_id;
    std::string m_address;
    std::shared_ptr<grpc::Channel> m_channel;
    std::unique_ptr<nri::pkg::api::v1alpha1::Plugin::StubInterface> m_stub;
};

#endif // DAEON_NRI_PLUGIN_CLIENT_H