/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wujing
 * Create: 2020-12-15
 * Description: provide cri pod sandbox manager service implementation
 *********************************************************************************/
#include "v1_cri_pod_sandbox_manager_service.h"

#include <sys/mount.h>
#include <isula_libutils/log.h>
#include <isula_libutils/host_config.h>
#include <isula_libutils/container_config.h>
#include <isula_libutils/auto_cleanup.h>
#include <algorithm>

#include "checkpoint_handler.h"
#include "utils.h"
#include "v1_cri_helpers.h"
#include "cri_helpers.h"
#include "v1_cri_security_context.h"
#include "cri_constants.h"
#include "v1_naming.h"
#include "service_container_api.h"
#include "cxxutils.h"
#include "network_namespace.h"
#include "v1_cri_image_manager_service_impl.h"
#include "namespace.h"
#include "sandbox.h"
#include "sandbox_manager.h"

namespace CRIV1 {
void PodSandboxManagerService::PrepareSandboxData(const runtime::v1::PodSandboxConfig &config,
                                                  const std::string &runtimeHandler, std::string &sandboxName,
                                                  RuntimeInfo &runtimeInfo, std::string &networkMode)
{
    // Prepare sandboxName
    sandboxName = CRINamingV1::MakeSandboxName(config.metadata());

    // Prepare runtimeInfo
    runtimeInfo.runtimeHandler = runtimeHandler;
    runtimeInfo.runtime = CRIHelpers::CRIRuntimeConvert(runtimeHandler);
    if (runtimeInfo.runtime.empty()) {
        runtimeInfo.runtime = std::string(runtimeHandler);
    }
    runtimeInfo.sandboxer = CRIHelpers::CRISandboxerConvert(runtimeHandler);
    if (runtimeInfo.sandboxer.empty()) {
        runtimeInfo.sandboxer = std::string(DEFAULT_SANDBOXER_NAME);
    }

    // Prepare network mode
    networkMode = CRI::Constants::namespaceModeCNI;
    if (config.linux().security_context().namespace_options().network() == runtime::v1::NamespaceMode::NODE) {
        networkMode = CRI::Constants::namespaceModeHost;
    }
}

auto PodSandboxManagerService::EnsureSandboxImageExists(const std::string &image, const std::string &sandboxer,
                                                        Errors &error) -> bool
{
    ImageManagerServiceImpl imageServiceImpl;
    ImageManagerService &imageService = imageServiceImpl;
    runtime::v1::ImageSpec imageRef;
    runtime::v1::AuthConfig auth;
    runtime::v1::ImageSpec imageSpec;
    Errors err;

    if (sandboxer != std::string(DEFAULT_SANDBOXER_NAME)) {
        // Skip pull image if sandboxer controller,
        // because sandboxer controller does not need image
        return true;
    }

    imageSpec.set_image(image);
    std::unique_ptr<runtime::v1::Image> imageStatus = imageService.ImageStatus(imageSpec, err);
    if (err.Empty()) {
        return true;
    }
    imageStatus.reset();

    imageRef.set_image(image);
    std::string outRef = imageService.PullImage(imageRef, auth, error);
    return !(!error.Empty() || outRef.empty());
}

void PodSandboxManagerService::PrepareSandboxKey(std::string &sandboxKey, Errors &error)
{
    __isula_auto_free char *sandboxKeyChars = new_sandbox_network_key();
    if (sandboxKeyChars == NULL || strlen(sandboxKeyChars) == 0) {
        error.SetError("Failed to generate sandbox key");
        return
    }

    if (create_network_namespace_file(sandboxKeyChars) != 0) {
        error.SetError("Failed to create network namespace");
        return
    }

    sandboxKey = std::string(sandboxKeyChars);
}

void PodSandboxManagerService::ApplySandboxDefaultResources(runtime::v1::LinuxPodSandboxConfig *linuxConfig)
{
    if (!linuxConfig->has_resources()) {
        linuxConfig->mutable_resources()->set_memory_swap_limit_in_bytes((google::protobuf::int64)CRI::Constants::DefaultMemorySwap)
        linuxConfig->mutable_resources()->set_cpu_shares((google::protobuf::int64)CRI::Constants::DefaultSandboxCPUshares);
        linuxConfig->mutable_resources()->set_cpu_quota((google::protobuf::int64)CRI::Constants::DefaultSandboxCPUQuota);
        linuxConfig->mutable_resources()->set_cpu_period((google::protobuf::int64)CRI::Constants::DefaultSandboxCPUPeriod);
        linuxConfig->mutable_resources()->set_memory_limit_in_bytes((google::protobuf::int64)CRI::Constants::DefaultSandboxMemoryLimitInBytes);
    }

    // set default oom score adj
    linuxConfig->mutable_resources()->set_oom_score_adj((google::protobuf::int64)(CRI::Constants::PodInfraOOMAdj));
}

auto PodSandboxManagerService::ParseCheckpointProtocol(runtime::v1::Protocol protocol) -> std::string
{
    switch (protocol) {
        case runtime::v1::UDP:
            return "udp";
        case runtime::v1::TCP:
        default:
            return "tcp";
    }
}

void PodSandboxManagerService::ConstructPodSandboxCheckpoint(const runtime::v1::PodSandboxConfig &config,
                                                             CRI::PodSandboxCheckpoint &checkpoint)
{
    checkpoint.SetName(config.metadata().name());
    checkpoint.SetNamespace(config.metadata().namespace_());
    checkpoint.SetData(new CRI::CheckpointData);

    int len = config.port_mappings_size();
    for (int i = 0; i < len; i++) {
        CRI::PortMapping item;

        const runtime::v1::PortMapping &iter = config.port_mappings(i);
        item.SetProtocol(ParseCheckpointProtocol(iter.protocol()));
        item.SetContainerPort(iter.container_port());
        item.SetHostPort(iter.host_port());
        (checkpoint.GetData())->InsertPortMapping(item);
    }
    if (config.linux().security_context().namespace_options().network() == runtime::v1::NamespaceMode::NODE) {
        (checkpoint.GetData())->SetHostNetwork(true);
    }
}

void PodSandboxManagerService::PrepareSandboxCheckpoint(const runtime::v1::PodSandboxConfig &config,
                                                        std::string &jsonCheckpoint, Errors &error)
{
    CRI::PodSandboxCheckpoint checkpoint;
    ConstructPodSandboxCheckpoint(config, checkpoint);
    jsonCheckpoint = CRIHelpers::CreateCheckpoint(checkpoint, error);
}

void PodSandboxManagerService::UpdateSandboxConfig(const runtime::v1::PodSandboxConfig &config,
                                                   std::string &jsonCheckpoint, Errors &error)
{
    auto labels = config->mutable_labels();
    auto annotations = config->mutable_annotations();

    (*labels)[CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY] = CRIHelpers::Constants::CONTAINER_TYPE_LABEL_SANDBOX;
    // Apply a container name label for infra container. This is used in summary v1.
    (*labels)[CRIHelpers::Constants::KUBERNETES_CONTAINER_NAME_LABEL] = CRIHelpers::Constants::POD_INFRA_CONTAINER_NAME;

    (*annotations)[CRIHelpers::Constants::CONTAINER_TYPE_ANNOTATION_KEY] = CRIHelpers::Constants::CONTAINER_TYPE_ANNOTATION_SANDBOX;
    // Add checkpoint into annotations
    (*annotations)[CRIHelpers::Constants::POD_CHECKPOINT_KEY] = jsonCheckpoint;

    if (config->has_metadata()) {
        (*annotations)[CRIHelpers::Constants::SANDBOX_NAMESPACE_ANNOTATION_KEY] = config->metadata().namespace_();
        (*annotations)[CRIHelpers::Constants::SANDBOX_NAME_ANNOTATION_KEY] = config->metadata().name();
        (*annotations)[CRIHelpers::Constants::SANDBOX_UID_ANNOTATION_KEY] = config->metadata().uid();
        (*annotations)[CRIHelpers::Constants::SANDBOX_ATTEMPT_ANNOTATION_KEY] = std::to_string(config->metadata().attempt());
    }

    ApplySandboxDefaultResources(config->mutable_linux());
}

void PodSandboxManagerService::SetupSandboxFiles(const std::string &resolvPath,
                                                 const runtime::v1::PodSandboxConfig &config, Errors &error)
{
    if (resolvPath.empty()) {
        return;
    }
    std::vector<std::string> resolvContentStrs;

    /* set DNS options */
    int len = config.dns_config().searches_size();
    if (len > CRI::Constants::MAX_DNS_SEARCHES) {
        error.SetError("DNSOption.Searches has more than 6 domains");
        return;
    }

    std::vector<std::string> servers(config.dns_config().servers().begin(), config.dns_config().servers().end());
    if (!servers.empty()) {
        resolvContentStrs.push_back("nameserver " + CXXUtils::StringsJoin(servers, "\nnameserver "));
    }

    std::vector<std::string> searches(config.dns_config().searches().begin(), config.dns_config().searches().end());
    if (!searches.empty()) {
        resolvContentStrs.push_back("search " + CXXUtils::StringsJoin(searches, " "));
    }

    std::vector<std::string> options(config.dns_config().options().begin(), config.dns_config().options().end());
    if (!options.empty()) {
        resolvContentStrs.push_back("options " + CXXUtils::StringsJoin(options, " "));
    }

    if (!resolvContentStrs.empty()) {
        std::string resolvContent = CXXUtils::StringsJoin(resolvContentStrs, "\n") + "\n";
        if (util_write_file(resolvPath.c_str(), resolvContent.c_str(), resolvContent.size(),
                            DEFAULT_SECURE_FILE_MODE) != 0) {
            error.SetError("Failed to write resolv content");
        }
    }
}

void PodSandboxManagerService::SetupSandboxNetwork(const std::shared_ptr<sandbox::Sandbox> sandbox,
                                                   std::string &network_settings_json, Errors &error)
{
    const auto config = sandbox->GetSandboxConfig();

    sandbox->SetNetworkReady(false);
    // Setup sandbox files
    if (config.has_dns_config() && !sandbox->GetResolvPath().empty()) {
        INFO("Overwrite resolv.conf: %s", sandbox->GetResolvPath().c_str());
        SetupSandboxFiles(sandbox->GetResolvPath(), config, error);
        if (error.NotEmpty()) {
            ERROR("Failed to setup sandbox files");
            return;
        }
    }

    if (!namespace_is_cni(sandbox.GetNetworkMode())) {
        return;
    }

    std::string &sandboxKey = sandbox->GetNetNsPath();
    if (sandboxKey.empty()) {
        error.Errorf("Sandbox key is invalid");
        ERROR("Sandbox key is invalid");
        return;
    }

    std::map<std::string, std::string> stdAnnos;
    CRIHelpers::ProtobufAnnoMapToStd(config.annotations(), stdAnnos);
    stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, sandboxKey));

    std::map<std::string, std::string> networkOptions;
    networkOptions["UID"] = config.metadata().uid();

    if (prepare_network_namespace(sandboxKey.c_str(), false, 0) != 0) {
        error.Errorf("Failed to prepare network namespace: %s", sandboxKey.c_str());
        ERROR("Failed to prepare network namespace: %s", sandboxKey.c_str());
        return;
    }

    // Setup networking for the sandbox.
    m_pluginManager->SetUpPod(config.metadata().namespace_(), config.metadata().name(),
                              Network::DEFAULT_NETWORK_INTERFACE_NAME, sandbox->GetId(), stdAnnos, networkOptions,
                              network_settings_json, error);
    if (error.NotEmpty()) {
        ERROR("SetupPod failed: %s", error.GetCMessage());
        if (remove_network_namespace(sandboxKey.c_str()) != 0) {
            ERROR("Failed to remove network namespace: %s", sandboxKey.c_str());
        }
        return;
    }

    sandbox->SetNetworkReady(true);
    DEBUG("set %s ready", sandbox->GetId().c_str());
}

auto PodSandboxManagerService::RunPodSandbox(const runtime::v1::PodSandboxConfig &config,
                                             const std::string &runtimeHandler, Errors &error) -> std::string
{
    std::string response_id;
    std::string sandboxName;
    RuntimeInfo runtimeInfo,
    std::string networkMode;
    std::string sandboxKey;
    std::string jsonCheckpoint;
    std::string network_setting_json;

    // Step 1: Parepare sandbox name, runtime and networkMode
    PrepareSandboxData(config, runtimeHandler, sandboxName, runtimeInfo, networkMode);

    // Step 2: Pull the image for the sandbox.
    // Maybe we should pull image in shim controller ?
    // But pull image interface is only in CRI image service, and it can;t be called in shim controller,
    // so we pull image in CRI pod service.
    const std::string &image = m_podSandboxImage;
    if (!EnsureSandboxImageExists(image, sandboxer, error)) {
        ERROR("Failed to pull sandbox image %s: %s", image.c_str(), error.NotEmpty() ? error.GetCMessage() : "");
        error.Errorf("Failed to pull sandbox image %s: %s", image.c_str(), error.NotEmpty() ? error.GetCMessage() : "");
        return response_id;
    }

    // Step 3: Prepare sandbox checkpoint
    PrepareSandboxCheckpoint(config, jsonCheckpoint, error);
    if (error.NotEmpty()) {
        goto response_id;
    }

    // Step 4: Update sandbox instance config
    UpdateSandboxConfig(config, jsonCheckpoint, error);
    if (error.NotEmpty()) {
        goto response_id;
    }

    // Step 5: Prepare sandboxKey
    if (namespace_is_cni(networkMode)) {
        // cleanup sandboxKey file in DeleteSandbox
        PrepareSandboxKey(error, sandboxKey);
        if (error.NotEmpty()) {
            return response_id;
        }
    }

    // Step 6: Create sandbox
    auto sandbox = sandbox::SandboxManager::GetInstance().CreateSandbox(sandboxName, runtimeInfo, sandboxKey,
                                                                        networkMode, config, error);
    if (error.NotEmpty()) {
        if (namespace_is_cni(networkMode)) {
            (void)remove_network_namespace_file(sandboxKey.c_str());
        }
        return response_id;
    }

    // Step 7: Setup networking for the sandbox.
    SetupSandboxNetwork(sandbox, network_setting_json, error);
    if (error.NotEmpty()) {
        return response_id;
    }

    // Step 8: Sandbox Start.
    sandbox->Start(error);
    if (error.NotEmpty()) {
        goto cleanup_network;
    }

    // Step 9: Save network settings json to disk
    if (namespace_is_cni(networkMode)) {
        Errors tmpErr;
        sandbox->UpdateNetworkSettings(network_setting_json, error);
        // If saving network settings failed, ignore error
        if (tmpErr.NotEmpty()) {
            WARN("%s", tmpErr.GetCMessage());
        }
    }
    return sandbox->GetId();

cleanup_network:
    if (namespace_is_cni(sandbox->GetNetworkMode())) {
        if (ClearCniNetwork(sandbox, error) != 0) {
            ERROR("Failed to clean cni network");
        }
    }
    return response_id;
}

void PodSandboxManagerService::ClearCniNetwork(const std::shared_ptr<sandbox::Sandbox> sandbox, Errors &error)
{
    Errors networkErr;

    std::string networkMode = sandbox->GetNetworkMode();
    if (!namespace_is_cni(networkMode) || !sandbox->GetNetworkReady()) {
        return 0;
    }

    std::string sandboxKey = sandbox->GetNetNsPath();
    if (sandboxKey.size() == 0) {
        ERROR("Failed to get network namespace path");
        return;
    }

    const auto config = sandbox->GetSandboxConfig();
    std::map<std::string, std::string> stdAnnos;
    CRIHelpers::ProtobufAnnoMapToStd(config.annotations(), stdAnnos);
    stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, sandboxKey));

    Errors pluginErr;
    m_pluginManager->TearDownPod(config.metadata().namespace_(), config.metadata().name(), Network::DEFAULT_NETWORK_INTERFACE_NAME, 
                                 sandbox->GetId(), stdAnnos, pluginErr);
    if (pluginErr.NotEmpty()) {
        WARN("TearDownPod cni network failed: %s", pluginErr.GetCMessage());
        errlist.push_back(pluginErr.GetMessage());
        return;
    }

    INFO("TearDownPod cni network: success");
    sandbox->SetNetworkReady(false);

    // umount netns when cni removed network successfully
    if (remove_network_namespace(sandboxKey.c_str()) != 0) {
        ERROR("Failed to umount directory %s:%s", sandboxKey.c_str(), strerror(errno));
    }
}

void PodSandboxManagerService::StopContainerHelper(const std::string &containerID, Errors &error)
{
    int ret = 0;
    container_stop_request *request { nullptr };
    container_stop_response *response { nullptr };
    // Termination grace period
    constexpr int32_t DefaultSandboxGracePeriod { 10 };

    if (m_cb == nullptr || m_cb->container.stop == nullptr) {
        error.SetError("Unimplemented callback");
        goto cleanup;
    }

    request = (container_stop_request *)util_common_calloc_s(sizeof(container_stop_request));
    if (request == nullptr) {
        error.SetError("Out of memory");
        goto cleanup;
    }
    request->id = util_strdup_s(containerID.c_str());
    request->timeout = DefaultSandboxGracePeriod;

    ret = m_cb->container.stop(request, &response);
    if (ret != 0) {
        std::string msg = (response != nullptr && response->errmsg != nullptr) ? response->errmsg : "internal";
        ERROR("Failed to stop sandbox %s: %s", containerID.c_str(), msg.c_str());
        error.SetError(msg);
    }
cleanup:
    free_container_stop_request(request);
    free_container_stop_response(response);
}

auto PodSandboxManagerService::GetSandboxKey(const container_inspect *inspect_data) -> std::string
{
    if (inspect_data == nullptr || inspect_data->network_settings == nullptr ||
        inspect_data->network_settings->sandbox_key == nullptr) {
        ERROR("Inspect data does not have network settings");
        return std::string("");
    }

    return std::string(inspect_data->network_settings->sandbox_key);
}

auto PodSandboxManagerService::GetRealSandboxIDToStop(const std::string &podSandboxID, bool &hostNetwork,
                                                      std::string &name, std::string &ns, std::string &realSandboxID,
                                                      std::map<std::string, std::string> &stdAnnos, Errors &error)
-> int
{
    Errors statusErr;

    auto status = PodSandboxStatus(podSandboxID, statusErr);
    if (statusErr.Empty()) {
        if (status->linux().namespaces().has_options()) {
            hostNetwork = (status->linux().namespaces().options().network() == runtime::v1::NamespaceMode::NODE);
        }
        // if metadata is invalid, don't return -1 and continue stopping pod
        if (status->has_metadata()) {
            name = status->metadata().name();
            ns = status->metadata().namespace_();
        }
        realSandboxID = status->id();
        CRIHelpers::ProtobufAnnoMapToStd(status->annotations(), stdAnnos);
    } else {
        if (CRIHelpers::IsContainerNotFoundError(statusErr.GetMessage())) {
            WARN("Both sandbox container and checkpoint for id %s could not be found. "
                 "Proceed without further sandbox information.",
                 podSandboxID.c_str());
        } else {
            error.Errorf("failed to get sandbox status: %s", statusErr.GetCMessage());
            return -1;
        }
    }
    if (realSandboxID.empty()) {
        realSandboxID = podSandboxID;
    }
    return 0;
}

auto PodSandboxManagerService::StopAllContainersInSandbox(const std::string &realSandboxID, Errors &error) -> int
{
    int ret = 0;
    container_list_request *list_request = nullptr;
    container_list_response *list_response = nullptr;

    if (m_cb == nullptr || m_cb->container.list == nullptr) {
        error.SetError("Unimplemented callback");
        return -1;
    }

    // list all containers to stop
    list_request = (container_list_request *)util_common_calloc_s(sizeof(container_list_request));
    if (list_request == nullptr) {
        error.SetError("Out of memory");
        return -1;
    }
    list_request->all = true;

    list_request->filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
    if (list_request->filters == nullptr) {
        error.SetError("Out of memory");
        ret = -1;
        goto cleanup;
    }

    // Add sandbox label
    if (CRIHelpers::FiltersAddLabel(list_request->filters, CRIHelpers::Constants::SANDBOX_ID_LABEL_KEY,
                                    realSandboxID) != 0) {
        error.SetError("Failed to add label");
        ret = -1;
        goto cleanup;
    }

    ret = m_cb->container.list(list_request, &list_response);
    if (ret != 0) {
        if (list_response != nullptr && list_response->errmsg != nullptr) {
            error.SetError(list_response->errmsg);
        } else {
            error.SetError("Failed to call list container callback");
        }
        ret = -1;
        goto cleanup;
    }

    // Remove all containers in the sandbox.
    for (size_t i = 0; i < list_response->containers_len; i++) {
        Errors stopError;
        CRIHelpers::StopContainer(m_cb, list_response->containers[i]->id, 0, stopError);
        if (stopError.NotEmpty() && !CRIHelpers::IsContainerNotFoundError(stopError.GetMessage())) {
            ERROR("Error stop container: %s: %s", list_response->containers[i]->id, stopError.GetCMessage());
            error.SetError(stopError.GetMessage());
            ret = -1;
            goto cleanup;
        }
    }
cleanup:
    free_container_list_request(list_request);
    free_container_list_response(list_response);
    return ret;
}

auto PodSandboxManagerService::GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    bool ready { false };
    auto iter = m_networkReady.find(podSandboxID);
    if (iter != m_networkReady.end()) {
        ready = iter->second;
    } else {
        error.Errorf("Do not find network: %s", podSandboxID.c_str());
    }

    return ready;
}

auto PodSandboxManagerService::ClearCniNetwork(const std::string &realSandboxID, bool hostNetwork,
                                               const std::string &ns, const std::string &name,
                                               std::vector<std::string> &errlist,
                                               std::map<std::string, std::string> &stdAnnos, Errors &
                                               /*error*/) -> int
{
    Errors networkErr;

    bool ready = GetNetworkReady(realSandboxID, networkErr);
    if (hostNetwork || (!ready && networkErr.Empty())) {
        return 0;
    }

    Errors pluginErr;
    container_inspect *inspect_data = nullptr;

    // hostNetwork has indicated network mode which render host config unnecessary
    // so that with_host_config is set to be false.
    inspect_data = CRIHelpers::InspectContainer(realSandboxID, pluginErr, false);
    if (pluginErr.NotEmpty()) {
        ERROR("Failed to inspect container");
    }

    std::string netnsPath = GetSandboxKey(inspect_data);
    if (netnsPath.size() == 0) {
        ERROR("Failed to get network namespace path");
        goto cleanup;
    }

    stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, netnsPath));
    pluginErr.Clear();
    m_pluginManager->TearDownPod(ns, name, Network::DEFAULT_NETWORK_INTERFACE_NAME, realSandboxID, stdAnnos,
                                 pluginErr);
    if (pluginErr.NotEmpty()) {
        WARN("TearDownPod cni network failed: %s", pluginErr.GetCMessage());
        errlist.push_back(pluginErr.GetMessage());
    } else {
        INFO("TearDownPod cni network: success");
        SetNetworkReady(realSandboxID, false, pluginErr);
        if (pluginErr.NotEmpty()) {
            WARN("set network ready: %s", pluginErr.GetCMessage());
        }
        // umount netns when cni removed network successfully
        if (remove_network_namespace(netnsPath.c_str()) != 0) {
            ERROR("Failed to umount directory %s:%s", netnsPath.c_str(), strerror(errno));
        }
    }

cleanup:
    free_container_inspect(inspect_data);
    return 0;
}

void PodSandboxManagerService::StopPodSandbox(const std::string &podSandboxID, Errors &error)
{
    std::string name;
    std::string ns;
    std::string realSandboxID;
    bool hostNetwork = false;
    Errors statusErr;
    Errors networkErr;
    std::map<std::string, std::string> stdAnnos;
    std::vector<std::string> errlist;

    if (m_cb == nullptr || m_cb->container.list == nullptr || m_cb->container.stop == nullptr) {
        error.SetError("Unimplemented callback");
        return;
    }

    INFO("TearDownPod begin");
    if (podSandboxID.empty()) {
        error.SetError("Invalid empty sandbox id.");
        return;
    }

    if (GetRealSandboxIDToStop(podSandboxID, hostNetwork, name, ns, realSandboxID, stdAnnos, error) != 0) {
        return;
    }

    if (StopAllContainersInSandbox(realSandboxID, error) != 0) {
        return;
    }

    if (ClearCniNetwork(realSandboxID, hostNetwork, ns, name, errlist, stdAnnos, error) != 0) {
        return;
    }

    StopContainerHelper(realSandboxID, error);
    if (error.NotEmpty()) {
        errlist.push_back(error.GetMessage());
    }
    error.SetAggregate(errlist);
}

auto PodSandboxManagerService::RemoveAllContainersInSandbox(const std::string &realSandboxID,
                                                            std::vector<std::string> &errors) -> int
{
    int ret = 0;
    container_list_request *list_request { nullptr };
    container_list_response *list_response { nullptr };

    if (m_cb == nullptr || m_cb->container.list == nullptr) {
        errors.push_back("Unimplemented callback");
        return -1;
    }

    // list all containers to stop
    list_request = (container_list_request *)util_common_calloc_s(sizeof(container_list_request));
    if (list_request == nullptr) {
        errors.push_back("Out of memory");
        return -1;
    }
    list_request->all = true;

    list_request->filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
    if (list_request->filters == nullptr) {
        errors.push_back("Out of memory");
        ret = -1;
        goto cleanup;
    }

    // Add sandbox label
    if (CRIHelpers::FiltersAddLabel(list_request->filters, CRIHelpers::Constants::SANDBOX_ID_LABEL_KEY,
                                    realSandboxID) != 0) {
        errors.push_back("Faild to add label");
        ret = -1;
        goto cleanup;
    }

    ret = m_cb->container.list(list_request, &list_response);
    if (ret != 0) {
        if (list_response != nullptr && list_response->errmsg != nullptr) {
            errors.push_back(list_response->errmsg);
        } else {
            errors.push_back("Failed to call list container callback");
        }
    }

    // Remove all containers in the sandbox.
    for (size_t i = 0; list_response != nullptr && i < list_response->containers_len; i++) {
        Errors rmError;
        CRIHelpers::RemoveContainer(m_cb, list_response->containers[i]->id, rmError);
        if (rmError.NotEmpty() && !CRIHelpers::IsContainerNotFoundError(rmError.GetMessage())) {
            ERROR("Error remove container: %s: %s", list_response->containers[i]->id, rmError.GetCMessage());
            errors.push_back(rmError.GetMessage());
        }
    }
cleanup:
    free_container_list_request(list_request);
    free_container_list_response(list_response);
    return ret;
}

void PodSandboxManagerService::ClearNetworkReady(const std::string &podSandboxID)
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    auto iter = m_networkReady.find(podSandboxID);
    if (iter != m_networkReady.end()) {
        m_networkReady.erase(iter);
    }
}

int PodSandboxManagerService::DoRemovePodSandbox(const std::string &realSandboxID, std::vector<std::string> &errors)
{
    int ret = 0;
    container_delete_request *remove_request { nullptr };
    container_delete_response *remove_response { nullptr };

    if (m_cb == nullptr || m_cb->container.remove == nullptr) {
        errors.push_back("Unimplemented callback");
        return -1;
    }

    remove_request = (container_delete_request *)util_common_calloc_s(sizeof(container_delete_request));
    if (remove_request == nullptr) {
        errors.push_back("Out of memory");
        return -1;
    }
    remove_request->id = util_strdup_s(realSandboxID.c_str());
    remove_request->force = true;

    ret = m_cb->container.remove(remove_request, &remove_response);
    if (ret == 0 || (remove_response != nullptr && remove_response->errmsg != nullptr &&
                     CRIHelpers::IsContainerNotFoundError(remove_response->errmsg))) {
        // Only clear network ready when the sandbox has actually been
        // removed from docker or doesn't exist
        ClearNetworkReady(realSandboxID);
    } else {
        if (remove_response != nullptr && (remove_response->errmsg != nullptr)) {
            errors.push_back(remove_response->errmsg);
        } else {
            errors.push_back("Failed to call remove container callback");
        }
    }
    free_container_delete_request(remove_request);
    free_container_delete_response(remove_response);
    return ret;
}

void PodSandboxManagerService::RemovePodSandbox(const std::string &podSandboxID, Errors &error)
{
    std::vector<std::string> errors;
    Errors localErr;
    std::string realSandboxID;

    if (podSandboxID.empty()) {
        errors.push_back("Invalid empty sandbox id.");
        goto cleanup;
    }
    realSandboxID = CRIHelpers::GetRealContainerOrSandboxID(m_cb, podSandboxID, true, error);
    if (error.NotEmpty()) {
        if (CRIHelpers::IsContainerNotFoundError(error.GetMessage())) {
            error.Clear();
            realSandboxID = podSandboxID;
        } else {
            ERROR("Failed to find sandbox id %s: %s", podSandboxID.c_str(), error.GetCMessage());
            errors.push_back("Failed to find sandbox id " + podSandboxID + ": " + error.GetMessage());
            goto cleanup;
        }
    }

    if (RemoveAllContainersInSandbox(realSandboxID, errors) != 0) {
        goto cleanup;
    }

    if (DoRemovePodSandbox(realSandboxID, errors) != 0) {
        goto cleanup;
    }

cleanup:
    error.SetAggregate(errors);
}

auto PodSandboxManagerService::SharesHostNetwork(const container_inspect *inspect) -> runtime::v1::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->network_mode != nullptr) &&
        std::string(inspect->host_config->network_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1::NamespaceMode::NODE;
    }
    return runtime::v1::NamespaceMode::POD;
}

auto PodSandboxManagerService::SharesHostPid(const container_inspect *inspect) -> runtime::v1::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->pid_mode != nullptr) &&
        std::string(inspect->host_config->pid_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1::NamespaceMode::NODE;
    }
    return runtime::v1::NamespaceMode::CONTAINER;
}

auto PodSandboxManagerService::SharesHostIpc(const container_inspect *inspect) -> runtime::v1::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->ipc_mode != nullptr) &&
        std::string(inspect->host_config->ipc_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1::NamespaceMode::NODE;
    }
    return runtime::v1::NamespaceMode::POD;
}

void PodSandboxManagerService::GetIPs(const std::string &podSandboxID, const container_inspect *inspect,
                                      const std::string &networkInterface, std::vector<std::string> &ips, Errors &error)
{
    if (inspect == nullptr) {
        return;
    }

    if (SharesHostNetwork(inspect) != 0) {
        // For sandboxes using host network, the shim is not responsible for reporting the IP.
        return;
    }

    bool ready = GetNetworkReady(podSandboxID, error);
    if (error.Empty() && !ready) {
        WARN("Network %s do not ready", podSandboxID.c_str());
        return;
    }

    error.Clear();
    if (inspect->network_settings == NULL || inspect->network_settings->networks == NULL) {
        WARN("inspect network is empty");
        return;
    }

    for (size_t i = 0; i < inspect->network_settings->networks->len; i++) {
        if (inspect->network_settings->networks->values[i] != nullptr &&
            inspect->network_settings->networks->values[i]->ip_address != nullptr) {
            WARN("Use container inspect ip: %s", inspect->network_settings->networks->values[i]->ip_address);
            ips.push_back(inspect->network_settings->networks->values[i]->ip_address);
        }
    }
}

void PodSandboxManagerService::SetSandboxStatusNetwork(const container_inspect *inspect,
                                                       const std::string &podSandboxID,
                                                       std::unique_ptr<runtime::v1::PodSandboxStatus> &podStatus,
                                                       Errors &error)
{
    std::vector<std::string> ips;
    size_t i;

    GetIPs(podSandboxID, inspect, Network::DEFAULT_NETWORK_INTERFACE_NAME, ips, error);
    if (ips.size() == 0) {
        return;
    }
    podStatus->mutable_network()->set_ip(ips[0]);

    for (i = 1; i < ips.size(); i++) {
        auto tPoint = podStatus->mutable_network()->add_additional_ips();
        tPoint->set_ip(ips[i]);
    }
}

void PodSandboxManagerService::PodSandboxStatusToGRPC(const container_inspect *inspect, const std::string &podSandboxID,
                                                      std::unique_ptr<runtime::v1::PodSandboxStatus> &podStatus,
                                                      Errors &error)
{
    int64_t createdAt {};
    runtime::v1::NamespaceOption *options { nullptr };

    if (inspect->id != nullptr) {
        podStatus->set_id(inspect->id);
    }

    CRIHelpers::GetContainerTimeStamps(inspect, &createdAt, nullptr, nullptr, error);
    if (error.NotEmpty()) {
        return;
    }
    podStatus->set_created_at(createdAt);

    if ((inspect->state != nullptr) && inspect->state->running) {
        podStatus->set_state(runtime::v1::SANDBOX_READY);
    } else {
        podStatus->set_state(runtime::v1::SANDBOX_NOTREADY);
    }

    if (inspect->config == nullptr) {
        ERROR("Invalid container information! Must include config info");
        return;
    }

    CRIHelpers::ExtractLabels(inspect->config->labels, *podStatus->mutable_labels());
    CRIHelpers::ExtractAnnotations(inspect->config->annotations, *podStatus->mutable_annotations());
    CRINamingV1::ParseSandboxName(podStatus->annotations(), *podStatus->mutable_metadata(), error);
    if (error.NotEmpty()) {
        return;
    }

    options = podStatus->mutable_linux()->mutable_namespaces()->mutable_options();
    options->set_network(SharesHostNetwork(inspect));
    options->set_pid(SharesHostPid(inspect));
    options->set_ipc(SharesHostIpc(inspect));

    // add networks
    // get default network status
    SetSandboxStatusNetwork(inspect, podSandboxID, podStatus, error);
    if (error.NotEmpty()) {
        ERROR("Set network status failed: %s", error.GetCMessage());
        return;
    }
}

std::unique_ptr<runtime::v1::PodSandboxStatus>
PodSandboxManagerService::PodSandboxStatus(const std::string &podSandboxID, Errors &error)
{
    container_inspect *inspect { nullptr };
    std::unique_ptr<runtime::v1::PodSandboxStatus> podStatus(new runtime::v1::PodSandboxStatus);

    if (podSandboxID.empty()) {
        error.SetError("Empty pod sandbox id");
        return nullptr;
    }
    std::string realSandboxID = CRIHelpers::GetRealContainerOrSandboxID(m_cb, podSandboxID, true, error);
    if (error.NotEmpty()) {
        ERROR("Failed to find sandbox id %s: %s", podSandboxID.c_str(), error.GetCMessage());
        error.Errorf("Failed to find sandbox id %s: %s", podSandboxID.c_str(), error.GetCMessage());
        return nullptr;
    }
    inspect = CRIHelpers::InspectContainer(realSandboxID, error, true);
    if (error.NotEmpty()) {
        ERROR("Inspect pod failed: %s", error.GetCMessage());
        return nullptr;
    }
    PodSandboxStatusToGRPC(inspect, realSandboxID, podStatus, error);
    free_container_inspect(inspect);
    return podStatus;
}

void PodSandboxManagerService::ListPodSandboxFromGRPC(const runtime::v1::PodSandboxFilter *filter,
                                                      container_list_request **request, bool *filterOutReadySandboxes,
                                                      Errors &error)
{
    *request = (container_list_request *)util_common_calloc_s(sizeof(container_list_request));
    if (*request == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    (*request)->filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
    if ((*request)->filters == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    (*request)->all = true;

    if (CRIHelpers::FiltersAddLabel((*request)->filters, CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY,
                                    CRIHelpers::Constants::CONTAINER_TYPE_LABEL_SANDBOX) != 0) {
        error.SetError("Failed to add label");
        return;
    }

    if (filter != nullptr) {
        if (!filter->id().empty()) {
            if (CRIHelpers::FiltersAdd((*request)->filters, "id", filter->id()) != 0) {
                error.SetError("Failed to add label");
                return;
            }
        }
        if (filter->has_state()) {
            if (filter->state().state() == runtime::v1::SANDBOX_READY) {
                (*request)->all = false;
            } else {
                *filterOutReadySandboxes = true;
            }
        }

        // Add some label
        for (auto &iter : filter->label_selector()) {
            if (CRIHelpers::FiltersAddLabel((*request)->filters, iter.first, iter.second) != 0) {
                error.SetError("Failed to add label");
                return;
            }
        }
    }
}

void PodSandboxManagerService::ListPodSandboxToGRPC(container_list_response *response,
                                                    std::vector<std::unique_ptr<runtime::v1::PodSandbox>> *pods,
                                                    bool filterOutReadySandboxes, Errors &error)
{
    for (size_t i = 0; i < response->containers_len; i++) {
        std::unique_ptr<runtime::v1::PodSandbox> pod(new runtime::v1::PodSandbox);

        if (response->containers[i]->id != nullptr) {
            pod->set_id(response->containers[i]->id);
        }
        if (response->containers[i]->status == CONTAINER_STATUS_RUNNING) {
            pod->set_state(runtime::v1::SANDBOX_READY);
        } else {
            pod->set_state(runtime::v1::SANDBOX_NOTREADY);
        }
        pod->set_created_at(response->containers[i]->created);

        CRIHelpers::ExtractLabels(response->containers[i]->labels, *pod->mutable_labels());

        CRIHelpers::ExtractAnnotations(response->containers[i]->annotations, *pod->mutable_annotations());

        CRINamingV1::ParseSandboxName(pod->annotations(), *pod->mutable_metadata(), error);

        if (filterOutReadySandboxes && pod->state() == runtime::v1::SANDBOX_READY) {
            continue;
        }

        pods->push_back(std::move(pod));
    }
}

void PodSandboxManagerService::ListPodSandbox(const runtime::v1::PodSandboxFilter *filter,
                                              std::vector<std::unique_ptr<runtime::v1::PodSandbox>> *pods,
                                              Errors &error)
{
    int ret = 0;
    container_list_request *request { nullptr };
    container_list_response *response { nullptr };
    bool filterOutReadySandboxes { false };

    if (m_cb == nullptr || m_cb->container.list == nullptr) {
        error.SetError("Unimplemented callback");
        return;
    }

    ListPodSandboxFromGRPC(filter, &request, &filterOutReadySandboxes, error);
    if (error.NotEmpty()) {
        goto cleanup;
    }

    ret = m_cb->container.list(request, &response);
    if (ret != 0) {
        if (response != nullptr && (response->errmsg != nullptr)) {
            error.SetError(response->errmsg);
        } else {
            error.SetError("Failed to call start container callback");
        }
        goto cleanup;
    }
    ListPodSandboxToGRPC(response, pods, filterOutReadySandboxes, error);

cleanup:
    free_container_list_request(request);
    free_container_list_response(response);
}

void PodSandboxManagerService::GetPodSandboxCgroupMetrics(const container_inspect *inspectData,
                                                          cgroup_metrics_t &cgroupMetrics, Errors &error)
{
    int nret { 0 };

    if (inspectData == nullptr || inspectData->host_config == nullptr) {
        error.SetError("Failed to retrieve inspect data");
        return;
    }

    auto cgroupParent = inspectData->host_config->cgroup_parent;
    if (cgroupParent == nullptr || strlen(cgroupParent) == 0) {
        error.Errorf("None cgroup parent");
        return;
    }

    auto cgroupVersion = common_get_cgroup_version();
    if (cgroupVersion < 0) {
        error.Errorf("Invalid cgroup version");
        return;
    }

    if (cgroupVersion == CGROUP_VERSION_1) {
        nret = common_get_cgroup_v1_metrics(cgroupParent, &cgroupMetrics);
    } else {
        // todo: get cgroup v2 metrics
    }

    if (nret != 0) {
        error.Errorf("Failed to get cgroup metrics");
    }
}

auto PodSandboxManagerService::GetNsenterPath(Errors &error) -> std::string
{
    char *err { nullptr };

    auto nsenterPath = look_path(std::string("nsenter").c_str(), &err);
    if (nsenterPath == nullptr) {
        error.SetError(err);
        free(err);
        return std::string();
    }

    auto path = std::string(nsenterPath);
    free(nsenterPath);
    return path;
}

void PodSandboxManagerService::GetPodSandboxNetworkMetrics(const container_inspect *inspectData,
                                                           std::map<std::string, std::string> &annotations,
                                                           std::vector<Network::NetworkInterfaceStats> &netMetrics,
                                                           Errors &error)
{
    Errors tmpErr;

    auto nsenterPath = GetNsenterPath(tmpErr);
    if (tmpErr.NotEmpty()) {
        error.Errorf("Failed to get nsenter: %s", tmpErr.GetCMessage());
        return;
    }

    std::string netnsPath = GetSandboxKey(inspectData);
    if (netnsPath.size() == 0) {
        error.SetError("Failed to get network namespace path");
        return;
    }

    Network::NetworkInterfaceStats netStats;
    Network::GetPodNetworkStats(nsenterPath, netnsPath, Network::DEFAULT_NETWORK_INTERFACE_NAME, netStats, tmpErr);
    if (tmpErr.NotEmpty()) {
        error.Errorf("Failed to get network stats: %s", tmpErr.GetCMessage());
        return;
    }
    netMetrics.push_back(netStats);

    auto networks = CRIHelpers::GetNetworkPlaneFromPodAnno(annotations, tmpErr);
    if (tmpErr.NotEmpty()) {
        WARN("Failed to get network plane: %s", tmpErr.GetCMessage());
        return;
    }
    if (networks == nullptr || networks->len == 0 || networks->items == nullptr) {
        // none extral networks
        return;
    }

    for (size_t i = 0; i < networks->len; i++) {
        if (networks->items[i] == nullptr || networks->items[i]->interface == nullptr) {
            continue;
        }

        Network::NetworkInterfaceStats netStats;
        Network::GetPodNetworkStats(nsenterPath, netnsPath, std::string(networks->items[i]->interface), netStats, tmpErr);
        if (tmpErr.NotEmpty()) {
            WARN("Failed to get network stats: %s", tmpErr.GetCMessage());
            tmpErr.Clear();
            continue;
        }
        netMetrics.push_back(netStats);
    }
}

void PodSandboxManagerService::PackagePodSandboxStatsAttributes(
    const std::string &id, std::unique_ptr<runtime::v1::PodSandboxStats> &podStatsPtr, Errors &error)
{
    auto status = PodSandboxStatus(id, error);
    if (error.NotEmpty()) {
        return;
    }

    podStatsPtr->mutable_attributes()->set_id(id);
    if (status->has_metadata()) {
        std::unique_ptr<runtime::v1::PodSandboxMetadata> metadata(
            new (std::nothrow) runtime::v1::PodSandboxMetadata(status->metadata()));
        if (metadata == nullptr) {
            error.SetError("Out of memory");
            return;
        }
        podStatsPtr->mutable_attributes()->set_allocated_metadata(metadata.release());
    }
    if (status->labels_size() > 0) {
        auto labels = podStatsPtr->mutable_attributes()->mutable_labels();
        *labels = status->labels();
    }
    if (status->annotations_size() > 0) {
        auto annotations = podStatsPtr->mutable_attributes()->mutable_annotations();
        *annotations = status->annotations();
    }
}

auto PodSandboxManagerService::GetAvailableBytes(const uint64_t &memoryLimit, const uint64_t &workingSetBytes)
-> uint64_t
{
    // maxMemorySize is define in
    // cadvisor/blob/2b6fbacac7598e0140b5bc8428e3bdd7d86cf5b9/metrics/prometheus.go#L1969-L1971
    const uint64_t maxMemorySize = 1UL << 62;

    if (memoryLimit < maxMemorySize && memoryLimit > workingSetBytes) {
        return memoryLimit - workingSetBytes;
    }

    return 0;
}

void PodSandboxManagerService::PackagePodSandboxContainerStats(
    const std::string &id,
    const std::unique_ptr<ContainerManagerService> &containerManager,
    std::unique_ptr<runtime::v1::PodSandboxStats> &podStatsPtr, Errors &error)
{
    std::vector<std::unique_ptr<runtime::v1::ContainerStats>> containerStats;
    runtime::v1::ContainerStatsFilter filter;

    filter.set_pod_sandbox_id(id);
    containerManager->ListContainerStats(&filter, &containerStats, error);
    if (error.NotEmpty()) {
        error.Errorf("Failed to list container stats: %s", error.GetCMessage());
        return;
    }

    for (auto &itor : containerStats) {
        auto container = podStatsPtr->mutable_linux()->add_containers();
        if (container == nullptr) {
            ERROR("Out of memory");
            error.SetError("Out of memory");
            return;
        }
        *container = *itor;
    }
}

void PodSandboxManagerService::PodSandboxStatsToGRPC(const std::string &id, const cgroup_metrics_t &cgroupMetrics,
                                                     const std::vector<Network::NetworkInterfaceStats> &netMetrics,
                                                     const std::unique_ptr<ContainerManagerService> &containerManager,
                                                     std::unique_ptr<runtime::v1::PodSandboxStats> &podStats,
                                                     Errors &error)
{
    std::unique_ptr<runtime::v1::PodSandboxStats> podStatsPtr(
        new (std::nothrow) runtime::v1::PodSandboxStats);
    if (podStatsPtr == nullptr) {
        ERROR("Out of memory");
        error.SetError("Out of memory");
        return;
    }

    PackagePodSandboxStatsAttributes(id, podStatsPtr, error);
    if (error.NotEmpty()) {
        return;
    }

    int64_t timestamp = util_get_now_time_nanos();
    // CPU
    auto cpu = podStatsPtr->mutable_linux()->mutable_cpu();
    cpu->set_timestamp(timestamp);
    cpu->mutable_usage_core_nano_seconds()->set_value(cgroupMetrics.cgcpu_metrics.cpu_use_nanos);
    // todo
    // cpu->mutable_usage_nano_cores()->set_value(getNanoCores());

    // Memory
    auto memory = podStatsPtr->mutable_linux()->mutable_memory();
    uint64_t workingSetBytes = 0;
    if (cgroupMetrics.cgmem_metrics.mem_used > cgroupMetrics.cgmem_metrics.total_inactive_file) {
        workingSetBytes = cgroupMetrics.cgmem_metrics.mem_used - cgroupMetrics.cgmem_metrics.total_inactive_file;
    }
    uint64_t availableBytes = GetAvailableBytes(cgroupMetrics.cgmem_metrics.mem_limit, workingSetBytes);

    memory->set_timestamp(timestamp);
    memory->mutable_working_set_bytes()->set_value(workingSetBytes);
    memory->mutable_available_bytes()->set_value(availableBytes);
    memory->mutable_usage_bytes()->set_value(cgroupMetrics.cgmem_metrics.mem_used);
    memory->mutable_rss_bytes()->set_value(cgroupMetrics.cgmem_metrics.total_rss);
    memory->mutable_page_faults()->set_value(cgroupMetrics.cgmem_metrics.total_pgfault);
    memory->mutable_major_page_faults()->set_value(cgroupMetrics.cgmem_metrics.total_pgmajfault);

    // Network
    if (netMetrics.size() > 0) {
        auto network = podStatsPtr->mutable_linux()->mutable_network();
        network->set_timestamp(timestamp);
        network->mutable_default_interface()->set_name(netMetrics[0].name);
        network->mutable_default_interface()->mutable_rx_bytes()->set_value(netMetrics[0].rxBytes);
        network->mutable_default_interface()->mutable_rx_errors()->set_value(netMetrics[0].rxErrors);
        network->mutable_default_interface()->mutable_tx_bytes()->set_value(netMetrics[0].txBytes);
        network->mutable_default_interface()->mutable_tx_errors()->set_value(netMetrics[0].txErrors);

        for (size_t i = 1; i < netMetrics.size(); i++) {
            auto extra = network->add_interfaces();
            extra->set_name(netMetrics[i].name);
            extra->mutable_rx_bytes()->set_value(netMetrics[i].rxBytes);
            extra->mutable_rx_errors()->set_value(netMetrics[i].rxErrors);
            extra->mutable_tx_bytes()->set_value(netMetrics[i].txBytes);
            extra->mutable_tx_errors()->set_value(netMetrics[i].txErrors);
        }
    }

    // Process
    auto process = podStatsPtr->mutable_linux()->mutable_process();
    process->set_timestamp(timestamp);
    process->mutable_process_count()->set_value(cgroupMetrics.cgpids_metrics.pid_current);

    PackagePodSandboxContainerStats(id, containerManager, podStatsPtr, error);
    if (error.NotEmpty()) {
        return;
    }

    podStats = move(podStatsPtr);
    return;
}

auto PodSandboxManagerService::PodSandboxStats(const std::string &podSandboxID,
                                               const std::unique_ptr<ContainerManagerService> &containerManager,
                                               Errors &error) -> std::unique_ptr<runtime::v1::PodSandboxStats>
{
    Errors tmpErr;
    container_inspect *inspectData { nullptr };
    cgroup_metrics_t cgroupMetrics { 0 };
    std::vector<Network::NetworkInterfaceStats> netMetrics;
    std::map<std::string, std::string> annotations;
    std::unique_ptr<runtime::v1::PodSandboxStats> podStats { nullptr };

    // get sandbox id
    if (podSandboxID.empty()) {
        ERROR("Empty pod sandbox id");
        error.SetError("Empty pod sandbox id");
        return nullptr;
    }
    std::string realSandboxID = CRIHelpers::GetRealContainerOrSandboxID(m_cb, podSandboxID, true, tmpErr);
    if (tmpErr.NotEmpty()) {
        ERROR("Failed to find sandbox id %s: %s", podSandboxID.c_str(), tmpErr.GetCMessage());
        error.Errorf("Failed to find sandbox id %s", podSandboxID.c_str());
        return nullptr;
    }

    inspectData = CRIHelpers::InspectContainer(realSandboxID, tmpErr, true);
    if (tmpErr.NotEmpty()) {
        ERROR("Failed to inspect container %s: %s", realSandboxID.c_str(), tmpErr.GetCMessage());
        error.Errorf("Failed to inspect container %s", realSandboxID.c_str());
        return nullptr;
    }

    auto status = PodSandboxStatus(realSandboxID, tmpErr);
    if (tmpErr.NotEmpty()) {
        ERROR("Failed to get podsandbox %s status: %s", realSandboxID.c_str(), tmpErr.GetCMessage());
        error.Errorf("Failed to get podsandbox %s status", realSandboxID.c_str());
        goto out;
    }
    CRIHelpers::ProtobufAnnoMapToStd(status->annotations(), annotations);

    GetPodSandboxCgroupMetrics(inspectData, cgroupMetrics, tmpErr);
    if (tmpErr.NotEmpty()) {
        ERROR("Failed to get cgroup metrics of sandbox id %s: %s", podSandboxID.c_str(), tmpErr.GetCMessage());
        error.Errorf("Failed to get cgroup metrics of sandbox id %s", podSandboxID.c_str());
        goto out;
    }

    GetPodSandboxNetworkMetrics(inspectData, annotations, netMetrics, tmpErr);
    if (tmpErr.NotEmpty()) {
        WARN("Failed to get network metrics of sandbox id %s: %s", podSandboxID.c_str(), tmpErr.GetCMessage());
        tmpErr.Clear();
    }

    PodSandboxStatsToGRPC(realSandboxID, cgroupMetrics, netMetrics, containerManager, podStats, tmpErr);
    if (tmpErr.NotEmpty()) {
        ERROR("Failed to set PodSandboxStats: %s", tmpErr.GetCMessage());
        error.Errorf("Failed to set PodSandboxStats");
        goto out;
    }

out:
    free_container_inspect(inspectData);
    return podStats;
}

void PodSandboxManagerService::GetFilterPodSandbox(const runtime::v1::PodSandboxStatsFilter *filter,
                                                   std::vector<std::string> &podSandboxIDs, Errors &error)
{
    int ret = 0;
    container_list_request *request { nullptr };
    container_list_response *response { nullptr };

    if (m_cb == nullptr || m_cb->container.list == nullptr) {
        error.SetError("Unimplemented callback list");
        return;
    }

    request = (container_list_request *)util_common_calloc_s(sizeof(container_list_request));
    if (request == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    request->filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
    if (request->filters == nullptr) {
        error.SetError("Out of memory");
        goto cleanup;
    }
    // only get running pod metrics
    request->all = false;

    // add filter
    if (CRIHelpers::FiltersAddLabel(request->filters, CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY,
                                    CRIHelpers::Constants::CONTAINER_TYPE_LABEL_SANDBOX) != 0) {
        error.Errorf("Failed to add label %s", CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY.c_str());
        goto cleanup;
    }
    if (filter != nullptr) {
        if (!filter->id().empty()) {
            if (CRIHelpers::FiltersAdd(request->filters, "id", filter->id()) != 0) {
                error.SetError("Failed to add label id");
                goto cleanup;
            }
        }
        for (auto &iter : filter->label_selector()) {
            if (CRIHelpers::FiltersAddLabel(request->filters, iter.first, iter.second) != 0) {
                error.Errorf("Failed to add label %s", iter.first.c_str());
                goto cleanup;
            }
        }
    }

    ret = m_cb->container.list(request, &response);
    if (ret != 0) {
        if (response != nullptr && (response->errmsg != nullptr)) {
            error.SetError(response->errmsg);
        } else {
            error.SetError("Failed to call list container callback");
        }
        goto cleanup;
    }

    for (size_t i = 0; i < response->containers_len; i++) {
        podSandboxIDs.push_back(response->containers[i]->id);
    }

cleanup:
    free_container_list_request(request);
    free_container_list_response(response);
}

void PodSandboxManagerService::ListPodSandboxStats(const runtime::v1::PodSandboxStatsFilter *filter,
                                                   const std::unique_ptr<ContainerManagerService> &containerManager,
                                                   std::vector<std::unique_ptr<runtime::v1::PodSandboxStats>> *podsStats,
                                                   Errors &error)
{
    std::vector<std::string> podSandboxIDs;

    GetFilterPodSandbox(filter, podSandboxIDs, error);
    if (error.NotEmpty()) {
        ERROR("Failed to get podsandbox filter: %s", error.GetCMessage());
        error.SetError("Failed to get podsandbox filter");
        return;
    }

    if (podSandboxIDs.size() == 0) {
        // none ready pods
        return;
    }

    for (auto &id : podSandboxIDs) {
        Errors tmpErr;
        auto podStats = PodSandboxStats(id, containerManager, tmpErr);
        if (podStats == nullptr) {
            WARN("Failed to get podSandbox %s stats: %s", id.c_str(), tmpErr.GetCMessage());
            continue;
        }

        podsStats->push_back(move(podStats));
    }
}

void PodSandboxManagerService::PortForward(const runtime::v1::PortForwardRequest &req,
                                           runtime::v1::PortForwardResponse *resp, Errors &error)
{
    // This feature is temporarily not supported
}

} // namespace CRI
