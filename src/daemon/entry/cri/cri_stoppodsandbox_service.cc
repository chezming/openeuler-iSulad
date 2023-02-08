#include "cri_stoppodsandbox_service.h"

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
auto StopPodSandboxService::SharesHostNetwork(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->network_mode != nullptr) &&
        std::string(inspect->host_config->network_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::POD;
}

auto StopPodSandboxService::SharesHostPid(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->pid_mode != nullptr) &&
        std::string(inspect->host_config->pid_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::CONTAINER;
}

auto StopPodSandboxService::SharesHostIpc(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->ipc_mode != nullptr) &&
        std::string(inspect->host_config->ipc_mode) == CRI::Constants::namespaceModeHost) {
        return runtime::v1alpha2::NamespaceMode::NODE;
    }
    return runtime::v1alpha2::NamespaceMode::POD;
}
void StopPodSandboxService::ShareHostUserNamespace(const container_inspect *inspect,
                                                   runtime::v1alpha2::UserNamespace &userns)
{
    if (inspect != nullptr && inspect->host_config != nullptr && (inspect->host_config->userns_mode != nullptr) &&
        std::string(inspect->host_config->userns_mode) == CRI::Constants::namespaceModeHost) {
        userns.set_mode(runtime::v1alpha2::NamespaceMode::NODE);
    }
    userns.set_mode(runtime::v1alpha2::NamespaceMode::POD);
    if (inspect->host_config->user_remap != nullptr) {
        char **items = NULL;
        size_t args_len = 0;
        int ret = 0;
        unsigned int host_uid = 0;
        unsigned int host_gid = 0;
        unsigned int size = 0;
        items = util_string_split(inspect->host_config->user_remap, ':');
        if (items == NULL) {
            return;
        }
        args_len = util_array_len((const char **)items);
        runtime::v1alpha2::IDMapping *uidmapping = userns.add_uids();
        runtime::v1alpha2::IDMapping *gidmapping = userns.add_gids();
        switch (args_len) {
            case 3:
                ret = util_safe_uint(items[0], &host_uid);
                if (ret) {
                    break;
                }
                ret = util_safe_uint(items[1], &host_gid);
                if (ret) {
                    break;
                }
                ret = util_safe_uint(items[2], &size);
                if (ret) {
                    break;
                }
                if (size > MAX_ID_OFFSET || size == 0) {            
                    break;
                }
                break;
            default:
                break;
            
        }
        uidmapping->set_host_id(host_uid);
        uidmapping->set_container_id(0);
        uidmapping->set_length(size);
        gidmapping->set_host_id(host_gid);
        gidmapping->set_container_id(0);
        gidmapping->set_length(size);

    }
    
}

void StopPodSandboxService::GetIPs(const std::string &podSandboxID, const container_inspect *inspect,
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

void StopPodSandboxService::SetSandboxStatusNetwork(const container_inspect *inspect,
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

void StopPodSandboxService::PodSandboxStatusToGRPC(const container_inspect *inspect, const std::string &podSandboxID,
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
    ShareHostUserNamespace(inspect, *options->mutable_userns_options());
    if (inspect->host_config->runtime != nullptr) {
        podStatus->set_runtime_handler(inspect->host_config->runtime);
    }
    // add networks
    // get default network status
    SetSandboxStatusNetwork(inspect, podSandboxID, podStatus, error);
    if (error.NotEmpty()) {
        ERROR("Set network status failed: %s", error.GetCMessage());
        return;
    }
}
std::unique_ptr<runtime::v1alpha2::PodSandboxStatus>
StopPodSandboxService::GetPodSandboxStatus(const std::string &podSandboxID, Errors &error)
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
auto StopPodSandboxService::GetRealSandboxIDToStop(const std::string &podSandboxID, bool &hostNetwork,
                                                      std::string &name, std::string &ns, std::string &realSandboxID,
                                                      std::map<std::string, std::string> &stdAnnos, Errors &error)
        -> int
{
    Errors statusErr;

    auto status = GetPodSandboxStatus(podSandboxID, statusErr);
    if (statusErr.Empty()) {
        if (status->linux().namespaces().has_options()) {
            hostNetwork = (status->linux().namespaces().options().network() == runtime::v1alpha2::NamespaceMode::NODE);
        }
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

auto StopPodSandboxService::StopAllContainersInSandbox(const std::string &realSandboxID, Errors &error) -> int
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

auto StopPodSandboxService::GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool
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
void StopPodSandboxService::SetNetworkReady(const std::string &podSandboxID, bool ready, Errors &error)
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    m_networkReady[podSandboxID] = ready;
}
auto StopPodSandboxService::GetSandboxKey(const container_inspect *inspect_data) -> std::string
{
    if (inspect_data == nullptr || inspect_data->network_settings == nullptr ||
        inspect_data->network_settings->sandbox_key == nullptr) {
        ERROR("Inspect data does not have network settings");
        return std::string("");
    }

    return std::string(inspect_data->network_settings->sandbox_key);
}
auto StopPodSandboxService::ClearCniNetwork(const std::string &realSandboxID, bool hostNetwork,
                                               const std::string &ns, const std::string &name,
                                               std::vector<std::string> &errlist,
                                               std::map<std::string, std::string> &stdAnnos, Errors &
                                               /*error*/) -> int
{
    Errors networkErr;
    container_inspect *inspect_data = nullptr;

    bool ready = GetNetworkReady(realSandboxID, networkErr);
    if (!hostNetwork && (ready || networkErr.NotEmpty())) {
        Errors pluginErr;

        // hostNetwork has indicated network mode which render host config unnecessary
        // so that with_host_config is set to be false.
        inspect_data = CRIHelpers::InspectContainer(realSandboxID, pluginErr, false);
        if (pluginErr.NotEmpty()) {
            ERROR("Failed to inspect container");
        }

        std::string netnsPath = GetSandboxKey(inspect_data);
        if (netnsPath.size() == 0) {
            ERROR("Failed to get network namespace path");
            return 0;
        }

        stdAnnos.insert(std::pair<std::string, std::string>(CRIHelpers::Constants::POD_SANDBOX_KEY, netnsPath));
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
    }
    free_container_inspect(inspect_data);
    return 0;
}
void StopPodSandboxService::StopContainerHelper(const std::string &containerID, Errors &error)
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

void StopPodSandboxService::StopPodSandbox(const std::string &podSandboxID, Errors &error)
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

}