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

#include "plugin.h"

#include <string>
#include <mutex>
#include <google/protobuf/map.h>
#include <iostream>
#include <thread>
#include <chrono>

// init client conn
NRIPlugin::NRIPlugin()
{
    
}
// Start Runtime service, wait for plugin to register, then configure it.
auto NRIPlugin::Start() -> bool
{
    return true;
}

// close a plugin shutting down its multiplexed ttrpc connections.
auto NRIPlugin::Close() -> bool
{
    return true;
}

// stop a plugin (if it was launched by us)
auto NRIPlugin::Stop() -> bool
{
    return true;
}

// Name returns a string indentication for the plugin.
auto NRIPlugin::GetName() -> std::string
{
    return m_name;
}

auto NRIPlugin::GetQualifiedName() -> std::string
{
    return m_name + m_idx;
}

void NRIPlugin::SetReady(void)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready = true;
    m_condition.notify_all();
}

// 参数能在内部获得的直接在内部获得？
auto NRIPlugin::Configure(std::string name, std::string version, std::string config, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::Synchronize(std::vector<std::unique_ptr<PodSandbox>> pods, std::vector<std::unique_ptr<Container>> &containers, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::CreateContainer(CreateContainerRequest &req, CreateContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::UpdateContainer(UpdateContainerRequest &req, UpdateContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::StopContainer(StopContainerRequest &req, StopContainerResponse &resp, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::StateChange(StateChangeEvent &evt, Errors &error) -> bool
{
    return true;
}

auto NRIPlugin::WaitForReady(int64_t timeout) -> bool
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);

    while (!m_ready) {
            if (m_condition.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
    return true;
}