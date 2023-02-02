#include "cri_statuspodsandbox_service.h"

#include <sys/mount.h>
#include "isula_libutils/log.h"
#include "isula_libutils/host_config.h"
#include "isula_libutils/container_config.h"
#include "checkpoint_handler.h"
#include "utils.h"
#include "cri_helpers.h"
#include "cri_security_context.h"
#include "cri_constants.h"
#include "naming.h"
#include "service_container_api.h"
#include "cxxutils.h"
#include "network_namespace.h"
#include "cri_image_manager_service_impl.h"
#include "namespace.h"

namespace CRI
{
auto StatusPodSandboxService::SharesHostNetwork(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->network_mode != nullptr) &&
        std::string(inspect->host_config->network_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::POD;
}

auto StatusPodSandboxService::SharesHostPid(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->pid_mode != nullptr) &&
        std::string(inspect->host_config->pid_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::CONTAINER;
}

auto StatusPodSandboxService::SharesHostIpc(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->ipc_mode != nullptr) &&
        std::string(inspect->host_config->ipc_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::POD;
}

auto StatusPodSandboxService::GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool
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
void StatusPodSandboxService::GetIPs(const std::string &podSandboxID, const container_inspect *inspect,
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

void StatusPodSandboxService::SetSandboxStatusNetwork(const container_inspect *inspect,
                                                       const std::string &podSandboxID,
                                                       std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus,
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
void StatusPodSandboxService::PodSandboxStatusToGRPC(const container_inspect *inspect, const std::string &podSandboxID,
                                                      std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus,
                                                      Errors &error)
{
    int64_t createdAt {};
    runtime::v1alpha2::NamespaceOption *options { nullptr };

    if (inspect->id != nullptr) {
        podStatus->set_id(inspect->id);
    }

    CRIHelpers::GetContainerTimeStamps(inspect, &createdAt, nullptr, nullptr, error);
    if (error.NotEmpty()) {
        return;
    }
    podStatus->set_created_at(createdAt);

    if ((inspect->state != nullptr) && inspect->state->running) {
        podStatus->set_state(runtime::v1alpha2::SANDBOX_READY);
    } else {
        podStatus->set_state(runtime::v1alpha2::SANDBOX_NOTREADY);
    }

    if (inspect->config == nullptr) {
        ERROR("Invalid container information! Must include config info");
        return;
    }

    CRIHelpers::ExtractLabels(inspect->config->labels, *podStatus->mutable_labels());
    CRIHelpers::ExtractAnnotations(inspect->config->annotations, *podStatus->mutable_annotations());
    CRINaming::ParseSandboxName(podStatus->annotations(), *podStatus->mutable_metadata(), error);
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

std::unique_ptr<runtime::v1alpha2::PodSandboxStatus>
StatusPodSandboxService::PodSandboxStatus(const std::string &podSandboxID, Errors &error)
{
    container_inspect *inspect { nullptr };
    std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> podStatus(new runtime::v1alpha2::PodSandboxStatus);

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

}
