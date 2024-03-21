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
 * Description: provide plugin manager(NRI adaption) class definition
 *********************************************************************************/

#ifndef DAEMON_NRI_PLUGIN_NRI_ADAPTION_H
#define DAEMON_NRI_PLUGIN_NRI_ADAPTION_H

// #include "read_write_lock.h"
#include "plugin.h"
#include "v1_cri_container_manager_service.h"

const std::string DefaultPluginPath = "/opt/nri/plugins";
const std::string DefaultSocketPath = "/var/run/nri/nri.sock";
const std::string DefaultPluginConfigPath = "/etc/nri/conf.d";

class NRIAdaptation {
public:
    // Singleton
    static NRIAdaptation *GetInstance() noexcept;
    NRIAdaptation();
    virtual ~NRIAdaptation() = default;

    // initialize value
    auto Init(Errors &error) -> bool;

    auto IsSupport() -> bool;

    // Start up pre-installed plugins.
    auto startPlugins(Errors &error) -> bool;
    // Stop plugins.
    auto stopPlugins() -> bool;

    // 清除已被关闭的plugin
    auto RemoveClosedPlugins() -> bool;

    auto GetPluginByIndex(const std::string index) -> std::shared_ptr<NRIPlugin>;

    auto RunPodSandbox(StateChangeEvent &evt, Errors &error) ->bool;
    auto StopPodSandbox(StateChangeEvent &evt, Errors &error) ->bool;
    auto RemovePodSandbox(StateChangeEvent &evt, Errors &error) ->bool;
    auto CreateContainer(CreateContainerRequest &req, CreateContainerResponse &resp, Errors &error) ->bool;
    auto PostCreateContainer(StateChangeEvent &evt, Errors &error) ->bool;
    auto StartContainer(StateChangeEvent &evt, Errors &error) ->bool;
    auto PostStartContainer(StateChangeEvent &evt, Errors &error) ->bool;
    auto UpdateContainer(UpdateContainerRequest &req, UpdateContainerResponse &resp, Errors &error) ->bool;
    auto PostUpdateContainer(StateChangeEvent &evt, Errors &error) ->bool;
    auto StopContainer(StopContainerRequest &req, StopContainerResponse &resp, Errors &error) ->bool;
    auto RemoveContainer(StateChangeEvent &evt, Errors &error) ->bool;
    auto StateChange(StateChangeEvent &evt, Errors &error) ->bool;
    // Perform a set of unsolicited container updates requested by a plugin.
    auto updateContainers(ContainerUpdate &req, ContainerUpdate &resp, Errors &error) ->bool;

    auto getPluginConfig(std::string &idx, std::string &name) -> bool;

private:
    auto newLaunchedPlugin(std::string &dir, std::string &idx, std::string &name, std::string &config) -> bool;
    auto discoverPlugins(std::vector<std::string> plugins, std::vector<std::string> indices, std::vector<std::string> configs, Errors &error) -> bool;
    // 给plugin排序
    auto sortPlugins() -> bool;

    auto GetNRIPluginConfigPath(void) -> std::string;
    auto GetNRIPluginPath(void) -> std::string;
    auto GetNRIRegistrationTimeout(void) -> std::string;
    auto GetNRIRequstTimeout(void) -> std::string;
private:
    RWMutex m_mutex;
    static std::atomic<NRIAdaptation *> m_instance;
    bool m_support;
    std::string m_version;
    std::string m_pluginConfigPath;
    std::string m_pluginPath;
    std::string m_socketPath;
    std::string m_disableConnections;
    // update function??
    // id --> NRIPlugin map
    std::map<std::string, std::shared_ptr<NRIPlugin>> m_storeMap;
    // TODO:plugin monitor thread id??
    // shutdown() to clean resource
    // init to create thread
    std::unique_ptr<CRIV1::ContainerManagerService> m_containerManager;
    // timeout,全局的timeout
};

#endif // DAEMON_NRI_PLUGIN_NRI_ADAPTION_H