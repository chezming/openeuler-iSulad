#ifndef DAEMON_ENTRY_CRI_RUNPODSANDBOX_SERVICE_H
#define DAEMON_ENTRY_CRI_RUNPODSANDBOX_SERVICE_H
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
class RunPodSandboxService {
public:
    RunPodSandboxService(const std::string &podSandboxImage, std::mutex &networkReadyLock,
                         std::map<std::string, bool> &networkReady, service_executor_t *cb,
                         std::shared_ptr<Network::PluginManager> pluginManager) 
            : m_podSandboxImage(podSandboxImage)
            , m_networkReadyLock(networkReadyLock)
            , m_networkReady(networkReady)
            , m_cb(cb)
            , m_pluginManager(pluginManager)
    {
    }
    RunPodSandboxService(const RunPodSandboxService &) = delete;
    auto operator=(const RunPodSandboxService &) -> RunPodSandboxService & = delete;
    virtual ~RunPodSandboxService() = default;
    auto RunPodSandbox(const runtime::v1alpha2::PodSandboxConfig &config, const std::string &runtimeHandler,
                       Errors &error) -> std::string;

private:
    auto EnsureSandboxImageExists(const std::string &image, Errors &error) -> bool;
    auto CreateSandboxContainer(const runtime::v1alpha2::PodSandboxConfig &config, const std::string &image,
                                std::string &jsonCheckpoint, const std::string &runtimeHandler, Errors &error)
            -> std::string;
    container_create_request *GenerateSandboxCreateContainerRequest(const runtime::v1alpha2::PodSandboxConfig &config,
                                                                    const std::string &image,
                                                                    std::string &jsonCheckpoint,
                                                                    const std::string &runtimeHandler, Errors &error);
    void MakeSandboxIsuladConfig(const runtime::v1alpha2::PodSandboxConfig &c, host_config *hc,
                                 container_config *custom_config, Errors &error);
    void SetHostConfigDefaultValue(host_config *hc);
    void ApplySandboxResources(const runtime::v1alpha2::LinuxPodSandboxConfig * /*lc*/, host_config *hc,
                               Errors & /*error*/);
    void ApplySandboxLinuxOptions(const runtime::v1alpha2::LinuxPodSandboxConfig &lc, host_config *hc,
                                  container_config *custom_config, Errors &error);
    void ConstructPodSandboxCheckpoint(const runtime::v1alpha2::PodSandboxConfig &config,
                                       CRI::PodSandboxCheckpoint &checkpoint);
    auto ParseCheckpointProtocol(runtime::v1alpha2::Protocol protocol) -> std::string;
    container_create_request *PackCreateContainerRequest(const runtime::v1alpha2::PodSandboxConfig &config,
                                                         const std::string &image, host_config *hostconfig,
                                                         container_config *custom_config,
                                                         const std::string &runtimeHandler, Errors &error);
    void SetNetworkReady(const std::string &podSandboxID, bool ready, Errors &error);
    void StartSandboxContainer(const std::string &response_id, Errors &error);
    void GetSandboxNetworkInfo(const runtime::v1alpha2::PodSandboxConfig &config, const std::string &jsonCheckpoint,
                               const container_inspect *inspect_data, std::string &sandboxKey,
                               std::map<std::string, std::string> &networkOptions,
                               std::map<std::string, std::string> &stdAnnos, Errors &error);
    auto GetSandboxKey(const container_inspect *inspect_data) -> std::string;
    void SetupSandboxNetwork(const runtime::v1alpha2::PodSandboxConfig &config, const std::string &response_id,
                             const container_inspect *inspect_data,
                             const std::map<std::string, std::string> &networkOptions,
                             const std::map<std::string, std::string> &stdAnnos, std::string &network_settings_json,
                             Errors &error);
    void SetupSandboxFiles(const std::string &resolvPath, const runtime::v1alpha2::PodSandboxConfig &config,
                           Errors &error);
    auto GenerateUpdateNetworkSettingsReqest(const std::string &id, const std::string &json, Errors &error)
            -> container_update_network_settings_request *;
    auto ClearCniNetwork(const std::string &realSandboxID, bool hostNetwork, const std::string &ns,
                         const std::string &name, std::vector<std::string> &errlist,
                         std::map<std::string, std::string> &stdAnnos, Errors &
                         /*error*/) -> int;
    auto GetNetworkReady(const std::string &podSandboxID, Errors &error) -> bool;


private:
    std::string m_podSandboxImage;
    std::mutex &m_networkReadyLock;
    std::map<std::string, bool> &m_networkReady;
    service_executor_t *m_cb { nullptr };
    std::shared_ptr<Network::PluginManager> m_pluginManager { nullptr };
};

}

#endif // !DAEMON_ENTRY_CRI_RUNPODSANDBOX_SERVICE_H
