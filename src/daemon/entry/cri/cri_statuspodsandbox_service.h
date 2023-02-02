#ifndef DAEMON_ENTRY_CRI_STATUSPODSANDBOX_SERVICE_H
#define DAEMON_ENTRY_CRI_STATUSPODSANDBOX_SERVICE_H
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
class StatusPodSandboxService {
public:
    StatusPodSandboxService(std::mutex &networkReadyLock, std::map<std::string, bool> &networkReady,
                            service_executor_t * cb)
            : m_networkReadyLock(networkReadyLock)
            , m_networkReady(networkReady)
            , m_cb(cb)
    {
    }
    StatusPodSandboxService(const StatusPodSandboxService &) = delete;
    auto operator=(const StatusPodSandboxService &) -> StatusPodSandboxService & = delete;
    virtual ~StatusPodSandboxService() = default;
    std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> PodSandboxStatus(const std::string &podSandboxID,
                                                                          Errors &error);

private:
    void PodSandboxStatusToGRPC(const container_inspect *inspect, const std::string &podSandboxID,
                                std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus, Errors &error);
    auto SharesHostNetwork(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    auto SharesHostPid(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    auto SharesHostIpc(const container_inspect *inspect) -> runtime::v1alpha2::NamespaceMode;
    auto GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool;
    void GetIPs(const std::string &podSandboxID, const container_inspect *inspect, const std::string &networkInterface,
                std::vector<std::string> &ips, Errors &error);
    void SetSandboxStatusNetwork(const container_inspect *inspect, const std::string &podSandboxID,
                                 std::unique_ptr<runtime::v1alpha2::PodSandboxStatus> &podStatus, Errors &error);

private:
    std::mutex &m_networkReadyLock;
    std::map<std::string, bool> &m_networkReady;
    service_executor_t *m_cb { nullptr };
};

}


#endif
