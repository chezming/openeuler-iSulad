#include "cri_removepodsandbox_service.h"

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
auto RemovePodSandboxService::RemoveAllContainersInSandbox(const std::string &realSandboxID,
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
void RemovePodSandboxService::ClearNetworkReady(const std::string &podSandboxID)
{
    std::lock_guard<std::mutex> lockGuard(m_networkReadyLock);

    auto iter = m_networkReady.find(podSandboxID);
    if (iter != m_networkReady.end()) {
        m_networkReady.erase(iter);
    }
}
int RemovePodSandboxService::DoRemovePodSandbox(const std::string &realSandboxID, std::vector<std::string> &errors)
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


void RemovePodSandboxService::RemovePodSandbox(const std::string &podSandboxID, Errors &error)
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

}