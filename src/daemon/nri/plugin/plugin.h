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
 * Description: provide plugin class definition
 *********************************************************************************/

#ifndef DAEMON_NRI_PLUGIN_PLUGIN_H
#define DAEMON_NRI_PLUGIN_PLUGIN_H

#include <condition_variable>

#include "grpc_nri_plugin_client.h"
#include "errors.h"
#include "nri.pb.h"

using namespace nri::pkg::api::v1alpha1;

const std::string PluginSocketEnvVar = "NRI_PLUGIN_SOCKET";

struct PluginLinuxPodSandbox {
    std::string id;
    std::string name;
    uint32_t uid;
    std::string nameSpace;
    uint64_t createdAt;
    google::protobuf::Map<std::string, std::string> labels;
    google::protobuf::Map<std::string, std::string> annotations;
    std::string runtimeHandler;
};

struct PluginSandboxInfo {
    std::string id;
    std::string name;
    uint32_t uid;
    std::string nameSpace;
    uint64_t createdAt;
    google::protobuf::Map<std::string, std::string> labels;
    google::protobuf::Map<std::string, std::string> annotations;
    std::string runtimeHandler;
};

class NRIPlugin {
public:
    // init client conn
    NRIPlugin();
    // close client conn ?? or 单独的连接处理函数
    virtual ~NRIPlugin() = default;
    // Start Runtime service, wait for plugin to register, then configure it.
    auto Start() -> bool;
    // close a plugin shutting down its multiplexed ttrpc connections.
    auto Close() -> bool;
    // stop a plugin (if it was launched by us)
    auto Stop() -> bool;

    // Name returns a string indentication for the plugin.
    auto GetName() -> std::string;
    auto GetQualifiedName() -> std::string; 
    
    void SetReady(void);

    // 参数能在内部获得的直接在内部获得？
    auto Configure(std::string name, std::string version, std::string config, Errors &error) -> bool;
    auto Synchronize(std::vector<std::unique_ptr<PodSandbox>> pods, std::vector<std::unique_ptr<Container>> &containers, Errors &error) -> bool;
    auto CreateContainer(CreateContainerRequest &req, CreateContainerResponse &resp, Errors &error) -> bool;
    auto UpdateContainer(UpdateContainerRequest &req, UpdateContainerResponse &resp, Errors &error) -> bool;
    auto StopContainer(StopContainerRequest &req, StopContainerResponse &resp, Errors &error) -> bool;
    auto StateChange(StateChangeEvent &evt, Errors &error) -> bool;

private:
    auto WaitForReady(int64_t timeout) -> bool;

private:
    std::mutex m_mutex;
    std::string m_idx;
    std::string m_name;
    std::string m_config;
    int m_pid;
    // 怎么保存cmd最好？
    std::string m_cmd;
    std::unique_ptr<NRIPluginClient> m_client;
    bool m_closed;
    bool m_ready;
    std::condition_variable m_condition;
};

#endif // DAEMON_NRI_PLUGIN_PLUGIN_H