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

#include "nri_adaption.h"

#include "isulad_config.h"

NRIAdaptation *NRIAdaptation::GetInstance() noexcept
{
    static std::once_flag flag;

    std::call_once(flag, [] { m_instance = new NRIAdaptation; });

    return m_instance;
}

// initialize value
auto NRIAdaptation::Init(Errors &error) -> bool
{
    m_pluginConfigPath = GetNRIPluginConfigPath();
    m_pluginPath = GetNRIPluginPath();
    // m_support = conf_get_nri_support();
    return true;
}

// Start up pre-installed plugins.
auto NRIAdaptation::startPlugins(Errors &error) -> bool
{
    return true;
}
// Stop plugins.
auto NRIAdaptation::stopPlugins() -> bool
{
    return true;
}

// 清除已被关闭的plugin
auto NRIAdaptation::RemoveClosedPlugins() -> bool
{
    return true;
}

auto NRIAdaptation::IsSupport() -> bool
{
    return m_support;
}

auto NRIAdaptation::GetPluginByIndex(const std::string index) -> std::shared_ptr<NRIPlugin>
{
    return nullptr;
}

auto NRIAdaptation::RunPodSandbox(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}
auto NRIAdaptation::StopPodSandbox(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::RemovePodSandbox(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::CreateContainer(CreateContainerRequest &req, CreateContainerResponse &resp, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::PostCreateContainer(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::StartContainer(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::PostStartContainer(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::UpdateContainer(UpdateContainerRequest &req, UpdateContainerResponse &resp, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::PostUpdateContainer(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::StopContainer(StopContainerRequest &req, StopContainerResponse &resp, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::RemoveContainer(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::StateChange(StateChangeEvent &evt, Errors &error) ->bool
{
    return true;
}

// Perform a set of unsolicited container updates requested by a plugin.
auto NRIAdaptation::updateContainers(ContainerUpdate &req, ContainerUpdate &resp, Errors &error) ->bool
{
    return true;
}

auto NRIAdaptation::getPluginConfig(std::string &idx, std::string &name) -> bool
{
    return true;
}

auto NRIAdaptation::newLaunchedPlugin(std::string &dir, std::string &idx, std::string &name, std::string &config) -> bool
{
    return true;
}

auto NRIAdaptation::discoverPlugins(std::vector<std::string> plugins, std::vector<std::string> indices, std::vector<std::string> configs, Errors &error) -> bool
{
    return true;
}

// 给plugin排序
auto NRIAdaptation::sortPlugins() -> bool
{
    return true;
}
