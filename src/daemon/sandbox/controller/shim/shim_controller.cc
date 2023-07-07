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
 * Author: xuxuepeng
 * Create: 2023-07-06
 * Description: provide shim controller implementation
 *********************************************************************************/

#include "shim_controller.h"

namespace sandbox {

ShimController::ShimController(const std::string &sandboxer): m_sandboxer(sandboxer) {}

ShimController::~ShimController() {}

bool ShimController::Init(Errors &error)
{
    return true;
}

bool ShimController::Create(const std::string &sandboxId,
                            const ControllerCreateParams &params,
                            Errors &error)
{
    return true;
}

std::unique_ptr<ControllerSandboxInfo> ShimController::Start(const std::string &sandboxId, Errors &error)
{
    return nullptr;
}

std::unique_ptr<ControllerPlatformInfo> ShimController::Platform(const std::string &sandboxId, Errors &error)
{
    return nullptr;
}

std::string ShimController::Prepare(const std::string &sandboxId,
                                    const ControllerPrepareParams &params,
                                    Errors &error)
{
    return std::string("");
}

bool ShimController::Purge(const std::string &sandboxId, const std::string &containerId,
                           const std::string &execId, Errors &error)
{
    return true;
}

bool ShimController::UpdateResources(const std::string &sandboxId,
                                     const ControllerUpdateResourcesParams &params,
                                     Errors &error)
{
    return true;
}

bool ShimController::Stop(const std::string &sandboxId, uint32_t timeoutSecs, Errors &error)
{
    return true;
}

bool ShimController::Wait(std::shared_ptr<SandboxExitCallback> cb, const std::string &sandboxId, Errors &error)
{
    return true;
}

std::unique_ptr<ControllerSandboxStatus> ShimController::Status(const std::string &sandboxId, bool verbose,
                                                                Errors &error)
{
    return nullptr;
}

bool ShimController::Shutdown(const std::string &sandboxId, Errors &error)
{
    return true;
}

bool ShimController::UpdateNetworkSettings(const std::string &sandboxId, const std::string &networkSettings,
                                           Errors &error)
{
    return true;
}

} // namespace