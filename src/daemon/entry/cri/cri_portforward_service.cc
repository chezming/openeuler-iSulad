#include "cri_portforward_service.h"

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
void PortForwardService::PortForward(const runtime::v1alpha2::PortForwardRequest &req,
                                           runtime::v1alpha2::PortForwardResponse *resp, Errors &error)
{
    // This feature is temporarily not supported
}
}