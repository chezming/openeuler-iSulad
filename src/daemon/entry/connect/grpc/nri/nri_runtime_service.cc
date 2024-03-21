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
 * Create: 2024-03-14
 * Description: provide nri unify services
 ******************************************************************************/
#include "nri_runtime_service.h"

#include <isula_libutils/log.h>

#include "stream_server.h"
#include "errors.h"
#include "nri_convert.h"
#include "nri_adaption.h"

int NRIRuntimeService::Init(const isulad_daemon_configs *config)
{
    if (config == nullptr) {
        ERROR("isulad config socket address is empty");
        return -1;
    }

    Errors err;
    // 1.设置默认值

    // 2.获得daemon.json中runtime service相关的配置，进行初始化

    if (config != nullptr) {
        if (config->plugin_config_path != NULL) {
            plugin_config_path = config->plugin_config_path;
        }
    }

    // Assembly implementation for CRIRuntimeServiceImpl
    service_executor_t *cb = get_service_executor();
    if (cb == nullptr) {
        ERROR("Init isulad service executor failure.");
        err.SetError("Init isulad service executor failure.");
        return -1;
    }

    m_containerManager = std::unique_ptr<CRIV1::ContainerManagerService>(new CRIV1::ContainerManagerService(cb));

    return 0;
}
void NRIRuntimeService::Wait()
{
}

void NRIRuntimeService::Shutdown()
{
}

void NRIRuntimeService::Register(grpc::ServerBuilder &sb)
{

}

// 
grpc::Status NRIRuntimeService::RegisterPlugin(grpc::ServerContext* context, const nri::pkg::api::v1alpha1::RegisterPluginRequest* request, nri::pkg::api::v1alpha1::Empty* response)
{
    auto plugin = NRIAdaptation::GetInstance()->GetPluginByIndex(request->plugin_idx());

    plugin->SetReady();

    EVENT("plugin %s registered as %s", request->plugin_name().c_str(), plugin->GetQualifiedName().c_str());
    return grpc::Status::OK;
}

grpc::Status NRIRuntimeService::UpdateContainers(grpc::ServerContext* context, const nri::pkg::api::v1alpha1::UpdateContainersRequest* request, nri::pkg::api::v1alpha1::UpdateContainersResponse* response)
{
    return grpc::Status::OK;
}
