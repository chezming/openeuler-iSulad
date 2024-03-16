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
 * Create: 2024-03-16
 * Description: provide nri convert functions
 *********************************************************************************/
#ifndef DAEMON_COMMON_NRI_NRI_CONVERT_H
#define DAEMON_COMMON_NRI_NRI_CONVERT_H
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "sandbox.h"
#include "nri.pb.h"
#include "container_api.h"
#include "api_v1.pb.h"

auto PodSandboxToNRI(sandbox::Sandbox &sandbox, nri::pkg::api::v1alpha1::PodSandbox &pod) -> bool;
auto ContainerToNRI(container_t *container, nri::pkg::api::v1alpha1::Container &con) -> bool;
auto PodSandboxesToNRI(std::vector<std::unique_ptr<sandbox::Sandbox>>, nri::pkg::api::v1alpha1::PodSandbox &pod) -> bool;

auto LinuxResourcesFromNRI(const nri::pkg::api::v1alpha1::LinuxResources &src, runtime::v1::LinuxContainerResources &resources) -> bool;

auto LinuxResourcesToNRI(const runtime::v1::LinuxContainerResources &src, nri::pkg::api::v1alpha1::LinuxResources &resources) -> bool;


#endif // DAEMON_COMMON_NRI_NRI_CONVERT_H
