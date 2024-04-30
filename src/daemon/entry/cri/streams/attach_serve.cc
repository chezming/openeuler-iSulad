/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wujing
 * Create: 2018-11-08
 * Description: provide container attach functions
 ******************************************************************************/
#include "attach_serve.h"
#include <isula_libutils/log.h>
#include "session.h"
#include "callback.h"
#include "utils.h"

typedef ssize_t (*AttachWriter)(void *context, const void *data, size_t len);
struct AttachContext {
    SessionData *lwsCtx;
    sem_t *sem;
    AttachWriter attachWriter;
};

ssize_t AttachWriteToClient(void *context, const void *data, size_t len)
{
    if (context == nullptr) {
        ERROR("attach context empty");
        return -1;
    }

    auto *attachCtx = static_cast<AttachContext *>(context);
    if (attachCtx->attachWriter == nullptr) {
        ERROR("attach context writer empty");
        return -1;
    }

    return attachCtx->attachWriter(static_cast<void *>(attachCtx->lwsCtx), data, len);
}

int AttachConnectClosed(void *context, char **err)
{
    (void)err;

    if (context == nullptr) {
        ERROR("attach context empty");
        return -1;
    }

    auto *attachCtx = static_cast<AttachContext *>(context);
    if (attachCtx->sem != nullptr) {
        (void)sem_post(attachCtx->sem);
    }

    return 0;
}

void AttachServe::SetServeThreadName()
{
    prctl(PR_SET_NAME, "AttachServe");
}

void *AttachServe::SetContainerStreamRequest(StreamRequest *grequest, const std::string &suffix)
{
    auto *m_request = static_cast<container_attach_request *>(util_common_calloc_s(sizeof(container_attach_request)));
    if (m_request == nullptr) {
        ERROR("Out of memory");
        return nullptr;
    }

    if (!grequest->containerID.empty()) {
        m_request->container_id = util_strdup_s(grequest->containerID.c_str());
    }
    m_request->attach_stdin = grequest->streamStdin;
    m_request->attach_stdout = grequest->streamStdout;
    m_request->attach_stderr = grequest->streamStderr;

    return m_request;
}

int AttachServe::ExecuteStreamCommand(SessionData *lwsCtx, void *request)
{
    auto *cb = get_service_executor();
    if (cb == nullptr || cb->container.attach == nullptr) {
        ERROR("Failed to get attach service executor");
        return -1;
    }

    auto *m_request = static_cast<container_attach_request *>(request);

    sem_t attachSem;
    if (sem_init(&attachSem, 0, 0) != 0) {
        ERROR("Semaphore initialization failed");
        return -1;
    }

    // stdout
    struct AttachContext stdoutContext = { 0 };
    stdoutContext.lwsCtx = lwsCtx;
    stdoutContext.sem = &attachSem;
    // write stdout to client if attach stdout is true
    stdoutContext.attachWriter = m_request->attach_stdout ? WsWriteStdoutToClient : WsDoNotWriteStdoutToClient;

    struct io_write_wrapper stdoutstringWriter = { 0 };
    stdoutstringWriter.context = static_cast<void *>(&stdoutContext);
    stdoutstringWriter.write_func = AttachWriteToClient;
    stdoutstringWriter.close_func = AttachConnectClosed;

    // stderr
    struct AttachContext stderrContext = { 0 };
    stderrContext.lwsCtx = lwsCtx;
    stderrContext.sem = nullptr;
    // write stderr to client if attach stderr is true
    stderrContext.attachWriter = m_request->attach_stderr ? WsWriteStderrToClient : WsDoNotWriteStderrToClient;

    struct io_write_wrapper stderrstringWriter = { 0 };
    stderrstringWriter.context = static_cast<void *>(&stderrContext);
    stderrstringWriter.write_func = AttachWriteToClient;
    stderrstringWriter.close_func = nullptr;

    // Maybe attach stdout and stderr are both false.
    // To make sure the close func sem_post, set attach stdout and stderr true.
    bool record_attach_stdout = m_request->attach_stdout;
    bool record_attach_stderr = m_request->attach_stderr;
    m_request->attach_stdout = true;
    m_request->attach_stderr = true;

    container_attach_response *m_response { nullptr };
    auto *c_serr = static_cast<cri_status_error *>(util_common_calloc_s(sizeof(cri_status_error)));
    if (c_serr == nullptr) {
        ERROR("Out of memory");
        return -1;
    }
    int ret = cb->container.attach(m_request, &m_response, m_request->attach_stdin ? lwsCtx->pipes.at(0) : -1,
                                   &stdoutstringWriter, &stderrstringWriter);

    if (ret != 0) {
        // join io copy thread in attach callback
        ERROR("Failed to attach container: %s", m_request->container_id);

        std::string message;
        if (m_response != nullptr && m_response->errmsg != nullptr) {
            message = m_response->errmsg;
        } else {
            message = "Failed to call attach container callback. ";
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

    if (ret == 0) {
        // wait io copy thread complete
        // cause to critest attach failure, because this wait will blocked websocket connection
        (void)sem_wait(&attachSem);
    }
    (void)sem_destroy(&attachSem);
    free_container_attach_response(m_response);
    m_request->attach_stdout = record_attach_stdout;
    m_request->attach_stderr = record_attach_stderr;
    return ret;
}

void AttachServe::CloseConnect(SessionData *lwsCtx)
{
    closeWsConnect(static_cast<void*>(lwsCtx), nullptr);
}

void AttachServe::FreeRequest(void *m_request)
{
    free_container_attach_request(static_cast<container_attach_request *>(m_request));
}
