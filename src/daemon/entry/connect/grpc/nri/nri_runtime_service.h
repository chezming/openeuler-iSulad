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
 * Description: provide nri services
 ******************************************************************************/
#ifndef DAEMON_ENTRY_CONNECT_GRPC_NRI_NRI_RUNTIME_SERVICE_H
#define DAEMON_ENTRY_CONNECT_GRPC_NRI_NRI_RUNTIME_SERVICE_H
#include <grpcpp/server_builder.h>

#include <isula_libutils/isulad_daemon_configs.h>

#include "nri.grpc.pb.h"
#include "v1_cri_container_manager_service.h"
#include "callback.h"

class NRIRuntimeService : public nri::pkg::api::v1alpha1::Runtime::Service  {
public:
    int Init(const isulad_daemon_configs *config);
    virtual ~NRIRuntimeService() = default;
    void Wait(void);
    void Shutdown(void);
    void Register(grpc::ServerBuilder &sb);

    grpc::Status RegisterPlugin(grpc::ServerContext* context, const nri::pkg::api::v1alpha1::RegisterPluginRequest* request, nri::pkg::api::v1alpha1::Empty* response) override;
    grpc::Status UpdateContainers(grpc::ServerContext* context, const nri::pkg::api::v1alpha1::UpdateContainersRequest* request, nri::pkg::api::v1alpha1::UpdateContainersResponse* response) override;
private:
    std::string plugin_config_path;
    // NRI adaption 实例，直接用的时候GetInstance()
    // cri con service实例，更新容器
    std::unique_ptr<CRIV1::ContainerManagerService> m_containerManager;
};

#endif