
#ifndef DAEMON_ENTRY_CRI_STOPPODSANDBOX_SERVICE_H
#define DAEMON_ENTRY_CRI_STOPPODSANDBOX_SERVICE_H
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>

#include "api.pb.h"
#include "errors.h"
#include "callback.h"
#include "pthread.h"
#include "network_plugin.h"
#include "isula_libutils/host_config.h"
#include "isula_libutils/container_config.h"
#include "isula_libutils/container_inspect.h"
#include "checkpoint_handler.h"
namespace CRI
{
class StopPodSandboxService {
public:
    StopPodSandboxService(std::mutex &networkReadyLock, std::map<std::string, bool> &networkReady, service_executor_t *cb,
                          std::shared_ptr<Network::PluginManager> pluginManager)
            : m_networkReadyLock(networkReadyLock)
            , m_networkReady(networkReady)
            , m_cb(cb)
            , m_pluginManager(pluginManager)
    {
    }
    StopPodSandboxService(const StopPodSandboxService &) = delete;
    auto operator=(const StopPodSandboxService &) -> StopPodSandboxService & = delete;
    virtual ~StopPodSandboxService() = default;
    void StopPodSandbox(const std::string &podSandboxID, Errors &error);
    
private:
    auto GetRealSandboxIDToStop(const std::string &podSandboxID, bool &hostNetwork, std::string &name, std::string &ns,
                                std::string &realSandboxID, std::map<std::string, std::string> &stdAnnos, Errors &error)
            -> int;
    std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> PodSandboxStatus(const std::string &podSandboxID,
                                                                          Errors &error);
    void PodSandboxStatusToGRPC(const container_inspect *inspect, const std::string &podSandboxID,
                                std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus, Errors &error);
    auto SharesHostNetwork(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    auto SharesHostPid(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    auto SharesHostIpc(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    void ShareHostUserNamespace(const container_inspect *inspect, runtime::v1alpha2::UserNamespace &userns);
    void GetIPs(const std::string &podSandboxID, const container_inspect *inspect, const std::string &networkInterface,
                std::vector<std::string> &ips, Errors &error);
    void SetSandboxStatusNetwork(const container_inspect *inspect, const std::string &podSandboxID,
                                 std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus, Errors &error);
    std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> GetPodSandboxStatus(const std::string &podSandboxID,
                                                                          Errors &error);

    auto StopAllContainersInSandbox(const std::string &realSandboxID, Errors &error) -> int;
    auto GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool;
    void SetNetworkReady(const std::string &podSandboxID, bool ready, Errors &error);
    auto GetSandboxKey(const container_inspect *inspect_data) -> std::string;
    auto ClearCniNetwork(const std::string &realSandboxID, bool hostNetwork, const std::string &ns,
                         const std::string &name, std::vector<std::string> &errlist,
                         std::map<std::string, std::string> &stdAnnos, Errors &
                         /*error*/) -> int;
    void StopContainerHelper(const std::string &containerID, Errors &error);

private:
    std::mutex &m_networkReadyLock;
    std::map<std::string, bool> &m_networkReady;
    service_executor_t *m_cb { nullptr };
    std::shared_ptr<Network::PluginManager> m_pluginManager { nullptr };
};
}
#endif