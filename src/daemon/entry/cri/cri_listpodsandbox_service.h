#ifndef DAEMON_ENTRY_CRI_LISTPODSANDBOX_SERVICE_H
#define DAEMON_ENTRY_CRI_LISTPODSANDBOX_SERVICE_H
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
namespace CRI{
class ListPodSandboxService {
public:
    ListPodSandboxService(service_executor_t *cb)
            : m_cb(cb)
    {
    }
    ListPodSandboxService(const ListPodSandboxService &) = delete;
    auto operator=(const ListPodSandboxService &) -> ListPodSandboxService & = delete;
    virtual ~ListPodSandboxService() = default;
    void ListPodSandbox(const runtime::v1alpha2::PodSandboxFilter *filter,
                        std::vector<std::unique_ptr<runtime::v1alpha2::PodSandbox>> *pods, Errors &error);

private:
    void ListPodSandboxFromGRPC(const runtime::v1alpha2::PodSandboxFilter *filter, container_list_request **request,
                                bool *filterOutReadySandboxes, Errors &error);
    void ListPodSandboxToGRPC(container_list_response *response,
                              std::vector<std::unique_ptr<runtime::v1alpha2::PodSandbox>> *pods,
                              bool filterOutReadySandboxes, Errors &error);

private:
    service_executor_t *m_cb { nullptr };
};

}


#endif