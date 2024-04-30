/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Description: PortForward streaming service implementation.
 * Author: zhongtao
 * Create: 2023-09-15
 ******************************************************************************/

#include "portforward_serve.h"

#include <isula_libutils/log.h>
#include "io_wrapper.h"
#include "session.h"
#include "utils.h"
#include "callback.h"
#include "cri_helpers.h"

void PortForwardServe::SetServeThreadName()
{
    prctl(PR_SET_NAME, "PortForwardServe");
}

void *PortForwardServe::SetContainerStreamRequest(StreamRequest *grequest, const std::string &suffix)
{
    if (grequest->containerID.empty()) {
        ERROR("Empty container id");
        return nullptr;
    }

    auto *m_request = static_cast<container_portforward_request *>(util_common_calloc_s(sizeof(container_portforward_request)));
    if (m_request == nullptr) {
        ERROR("Out of memory");
        return nullptr;
    }

    if (grequest->ports.size() == 0) {
        return m_request;
    }

    m_request->ports = (int32_t *)util_smart_calloc_s(sizeof(int), grequest->ports.size());
    if (m_request->ports == nullptr) {
        free_container_portforward_request(m_request);
        ERROR("Out of memory");
        return nullptr;
    }

    size_t i;
    for (i = 0; i < grequest->ports.size(); i++) {
        m_request->ports[i] = grequest->ports.at(i);
        m_request->ports_len++;
    }

    m_request->pod_sandbox_id = util_strdup_s(grequest->containerID.c_str());

    return m_request;
}

int PortForwardServe::ExecuteStreamCommand(SessionData *lwsCtx, void *request)
{
    int ret = 0;
    auto *cb = get_service_executor();
    if (cb == nullptr || cb->container.portforward == nullptr) {
        ERROR("Failed to get portforward service executor");
        return -1;
    }

    auto *m_request = static_cast<container_portforward_request *>(request);

    DEBUG("ports_len: %ld", m_request->ports_len);

    if (m_request->ports_len == 0) {
        ERROR("Empty ports_len");
        return -1;
    }

    lwsCtx->portPipes.resize(m_request->ports_len, std::vector<int>(MAX_ARRAY_LEN, -1));
    // store portPipes[0]
    int pipes[m_request->ports_len] = { 0 };
    size_t i;
    // Create a read client-specific fifo and write the data received from the client.
    // then forward the contents in fifo to container ports through io copy
    for (i = 0; i < m_request->ports_len; i++) {
        if ((pipe2(lwsCtx -> portPipes[i].data(), O_NONBLOCK | O_CLOEXEC)) < 0) {
            ERROR("Failed to create pipe(websocket server to container pipe)!");
            return -1;
        }
        pipes[i] = lwsCtx -> portPipes[i][0];
    }

    struct io_write_wrapper stringWriter = { 0 };
    struct io_read_wrapper stringReader = { 0 };

    stringWriter.context = (void *)lwsCtx;
    stringWriter.write_func = WsWriteDataToClient;
    // the close function of stringWriter is preferred unless stringWriter is nullptr
    stringWriter.close_func = nullptr;

    stringReader.context = (void *)pipes;
    stringReader.read = nullptr;
    stringWriter.close_func = nullptr;

    container_portforward_response *m_response { nullptr };
    auto *c_serr = static_cast<cri_status_error *>(util_common_calloc_s(sizeof(cri_status_error)));
    if (c_serr == nullptr) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    ret = cb->container.portforward(m_request, &stringWriter, &stringReader, &m_response);
    if (ret != 0) {
        std::string message;
        if (m_response != nullptr && m_response->errmsg != nullptr) {
            message = m_response->errmsg;
        } else {
            message = "Failed to call portforward container callback. ";
        }
        c_serr->details = static_cast<cri_status_error_details *>(util_common_calloc_s(sizeof(cri_status_error_details)));
        if (c_serr->details == nullptr) {
            WARN("Out of memory, skip this error");
        } else {
            c_serr->details->causes = static_cast<cri_status_error_details_causes_element **>(util_smart_calloc_s(sizeof(
                                                                                                                      cri_status_error_details_causes_element *), 1));
            if (c_serr->details->causes == nullptr) {
                WARN("Out of memory, skip this error");
            } else {
                c_serr->details->causes[0] = static_cast<cri_status_error_details_causes_element *>(util_common_calloc_s(sizeof(
                                                                                                                             cri_status_error_details_causes_element)));
                if (c_serr->details->causes[0] == nullptr) {
                    WARN("Out of memory, skip this error");
                } else {
                    c_serr->details->causes_len = 1;
                }
            }
        }
        c_serr->status = util_strdup_s("Failure");
        if (m_response != nullptr) {
            // 500 represent is a internal server error
            c_serr->code = 500;
            c_serr->reason = util_strdup_s("InternalError");
            if (c_serr->details != nullptr && c_serr->details->causes_len == 1) {
                c_serr->details->causes[0]->message = util_strdup_s(message.c_str());
            }
            std::string exit_info = "Internal error occurred:" + message;
            c_serr->message = util_strdup_s(exit_info.c_str());
        }
    } else {
        c_serr->status = util_strdup_s("Success");
    }
    WsWriteStatusErrToClient(lwsCtx, c_serr);

out:
    for (size_t i = 0; i < m_request->ports_len; i++) {
        close(lwsCtx -> portPipes[i][0]);
        close(lwsCtx -> portPipes[i][1]);
    }

    free_cri_status_error(c_serr);

    free_container_portforward_response(m_response);
    return ret;
}

void PortForwardServe::CloseConnect(SessionData *lwsCtx)
{
    closeWsConnect((void *)lwsCtx, nullptr);
}

void PortForwardServe::FreeRequest(void *m_request)
{
    free_container_portforward_request(static_cast<container_portforward_request *>(m_request));
}
