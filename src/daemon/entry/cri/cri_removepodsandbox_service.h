#ifndef DAEMON_ENTRY_CRI_REMOVEPODSANDBOX_SERVICE_H
#define DAEMON_ENTRY_CRI_REMOVEPODSANDBOX_SERVICE_H
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
class RemovePodSandboxService {
public:
    RemovePodSandboxService(std::mutex &networkReadyLock, std::map<std::string, bool> & networkReady,
                            service_executor_t * cb)
            : m_networkReadyLock(networkReadyLock)
            , m_networkReady(networkReady)
            , m_cb(cb)  
    {
    }
    RemovePodSandboxService(const RemovePodSandboxService &) = delete;
    auto operator=(const RemovePodSandboxService &) -> RemovePodSandboxService & = delete;
    virtual ~RemovePodSandboxService() = default;
    void RemovePodSandbox(const std::string &podSandboxID, Errors &error);

private:
    auto RemoveAllContainersInSandbox(const std::string &realSandboxID, std::vector<std::string> &errors) -> int;
    void ClearNetworkReady(const std::string &podSandboxID);
    int DoRemovePodSandbox(const std::string &realSandboxID, std::vector<std::string> &errors);

private:
    std::mutex &m_networkReadyLock;
    std::map<std::string, bool> &m_networkReady;
    service_executor_t *m_cb { nullptr };

};
}




#endif