#include "cri_listpodsandbox_service.h"

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
void ListPodSandboxService::ListPodSandboxFromGRPC(const runtime::v1alpha2::PodSandboxFilter *filter,
                                                      container_list_request **request, bool *filterOutReadySandboxes,
                                                      Errors &error)
{
    *request = (container_list_request *)util_common_calloc_s(sizeof(container_list_request));
    if (*request == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    (*request)->filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
    if ((*request)->filters == nullptr) {
        error.SetError("Out of memory");
        return;
    }
    (*request)->all = true;

    if (CRIHelpers::FiltersAddLabel((*request)->filters, CRIHelpers::Constants::CONTAINER_TYPE_LABEL_KEY,
                                    CRIHelpers::Constants::CONTAINER_TYPE_LABEL_SANDBOX) != 0) {
        error.SetError("Failed to add label");
        return;
    }

    if (filter != nullptr) {
        if (!filter->id().empty()) {
            if (CRIHelpers::FiltersAdd((*request)->filters, "id", filter->id()) != 0) {
                error.SetError("Failed to add label");
                return;
            }
        }
        if (filter->has_state()) {
            if (filter->state().state() == runtime::v1alpha2::SANDBOX_READY) {
                (*request)->all = false;
            } else {
                *filterOutReadySandboxes = true;
            }
        }

        // Add some label
        for (auto &iter : filter->label_selector()) {
            if (CRIHelpers::FiltersAddLabel((*request)->filters, iter.first, iter.second) != 0) {
                error.SetError("Failed to add label");
                return;
            }
        }
    }
}

void ListPodSandboxService::ListPodSandboxToGRPC(container_list_response *response,
                                                    std::vector<std::unique_ptr<runtime::v1alpha2::PodSandbox>> *pods,
                                                    bool filterOutReadySandboxes, Errors &error)
{
    for (size_t i = 0; i < response->containers_len; i++) {
        std::unique_ptr<runtime::v1alpha2::PodSandbox> pod(new runtime::v1alpha2::PodSandbox);

        if (response->containers[i]->id != nullptr) {
            pod->set_id(response->containers[i]->id);
        }
        if (response->containers[i]->status == CONTAINER_STATUS_RUNNING) {
            pod->set_state(runtime::v1alpha2::SANDBOX_READY);
        } else {
            pod->set_state(runtime::v1alpha2::SANDBOX_NOTREADY);
        }
        pod->set_created_at(response->containers[i]->created);

        CRIHelpers::ExtractLabels(response->containers[i]->labels, *pod->mutable_labels());

        CRIHelpers::ExtractAnnotations(response->containers[i]->annotations, *pod->mutable_annotations());

        CRINaming::ParseSandboxName(pod->annotations(), *pod->mutable_metadata(), error);

        if (filterOutReadySandboxes && pod->state() == runtime::v1alpha2::SANDBOX_READY) {
            continue;
        }
        //Add runtime_handler
        if (response->containers[i]->runtime != nullptr) {
            pod->set_runtime_handler(response->containers[i]->runtime);
        }

        pods->push_back(std::move(pod));
    }
}

void ListPodSandboxService::ListPodSandbox(const runtime::v1alpha2::PodSandboxFilter *filter,
                                              std::vector<std::unique_ptr<runtime::v1alpha2::PodSandbox>> *pods,
                                              Errors &error)
{
    int ret = 0;
    container_list_request *request { nullptr };
    container_list_response *response { nullptr };
    bool filterOutReadySandboxes { false };

    if (m_cb == nullptr || m_cb->container.list == nullptr) {
        error.SetError("Unimplemented callback");
        return;
    }

    ListPodSandboxFromGRPC(filter, &request, &filterOutReadySandboxes, error);
    if (error.NotEmpty()) {
        goto cleanup;
    }

    ret = m_cb->container.list(request, &response);
    if (ret != 0) {
        if (response != nullptr && (response->errmsg != nullptr)) {
            error.SetError(response->errmsg);
        } else {
            error.SetError("Failed to call start container callback");
        }
        goto cleanup;
    }
    ListPodSandboxToGRPC(response, pods, filterOutReadySandboxes, error);

cleanup:
    free_container_list_request(request);
    free_container_list_response(response);
}
}