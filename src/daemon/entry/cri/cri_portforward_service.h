#ifndef DAEMON_ENTRY_CRI_PORTFORWARD_SERVICE_H
#define DAEMON_ENTRY_CRI_PORTFORWARD_SERVICE_H
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
class PortForwardService {
public:
    PortForwardService(service_executor_t *cb)
            : m_cb(cb)
    {
    }
    PortForwardService(const PortForwardService &) = delete;
    auto operator=(const PortForwardService &) -> PortForwardService & = delete;
    virtual ~PortForwardService() = default;
    void PortForward(const runtime::v1alpha2::PortForwardRequest &req, runtime::v1alpha2::PortForwardResponse *resp,
                     Errors &error);

private:
    service_executor_t *m_cb { nullptr };

};

}


#endif