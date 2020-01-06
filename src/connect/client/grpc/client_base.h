/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: lifeng
 * Create: 2018-11-08
 * Description: provide container client base functions
 ******************************************************************************/
#ifndef __CLIENT_BASH_H
#define __CLIENT_BASH_H
#include <iostream>
#include <string>
#include <memory>
#include <grpcpp/grpcpp.h>
#include <sstream>
#include <fstream>

#include "error.h"
#include "log.h"
#include "connect.h"
#include "utils.h"
#include "certificate.h"

using grpc::Channel;
using grpc::ChannelArguments;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

namespace ClientBaseConstants {
const size_t COMMON_NAME_LEN { 50 };
const std::string TLS_OFF { "0" };
const std::string TLS_ON { "1" };
} // namespace ClientBaseConstants

template <class SV, class sTB, class RQ, class gRQ, class RP, class gRP>
class ClientBase {
public:
    explicit ClientBase(void *args)
    {
        client_connect_config_t *arguments = reinterpret_cast<client_connect_config_t *>(args);

        std::string socket_address = arguments->socket;
        const std::string tcp_prefix = "tcp://";
        deadline = arguments->deadline;

        if (socket_address.compare(0, tcp_prefix.length(), tcp_prefix) == 0) {
            socket_address.erase(0, tcp_prefix.length());
        }

        ChannelArguments channelArgs;
        // Set the load balancing policy for the channel.
        channelArgs.SetLoadBalancingPolicyName("round_robin");

        if (arguments->tls) {
            m_tlsMode = ClientBaseConstants::TLS_ON;
            m_certFile = arguments->cert_file != nullptr ?
                         std::string(arguments->cert_file, std::string(arguments->cert_file).length()) : "";
            std::string pem_root_certs = ReadTextFile(arguments->ca_file);
            std::string pem_private_key = ReadTextFile(arguments->key_file);
            std::string pem_cert_chain = ReadTextFile(arguments->cert_file);
            // Client modes
            // mode1/tls: Authenticate server based on public/default CA pool(not support)
            // mode2/tlsverify, tlscacert: Authenticate server based on given CA
            // mode3/tls, tlscert, tlskey: Authenticate with client certificate,
            // do not authenticate server based on given CA
            // mode4/tlsverify, tlscacert, tlscert, tlskey: Authenticate with client certificate
            // and authenticate server based on given CA
            grpc::SslCredentialsOptions ssl_opts = {
                arguments->tls_verify ? pem_root_certs : "", pem_private_key, pem_cert_chain
            };
            // Create a default SSL ChannelCredentials object.
            auto channel_creds = grpc::SslCredentials(ssl_opts);
            // Create a channel using the credentials created in the previous step.
            auto channel = grpc::CreateCustomChannel(socket_address, channel_creds, channelArgs);
            // Connect to gRPC server with ssl/tls authentication mechanism.
            stub_ = SV::NewStub(channel);
        } else {
            // Connect to gRPC server without ssl/tls authentication mechanism.
            stub_ = SV::NewStub(grpc::CreateCustomChannel(socket_address,
                                                          grpc::InsecureChannelCredentials(),
                                                          channelArgs));
        }
    }
    virtual ~ClientBase() = default;

    virtual void unpackStatus(Status &status, RP *response)
    {
        if (!status.error_message().empty() && \
            (status.error_code() == grpc::StatusCode::UNKNOWN ||
             status.error_code() == grpc::StatusCode::PERMISSION_DENIED ||
             status.error_code() == grpc::StatusCode::INTERNAL)) {
            response->errmsg = util_strdup_s(status.error_message().c_str());
        } else {
            response->errmsg = util_strdup_s(errno_to_error_message(LCRD_ERR_CONNECT));
        }

        response->cc = LCRD_ERR_EXEC;
    }

    virtual int run(const RQ *request, RP *response)
    {
        int ret;
        gRQ req;
        gRP reply;
        ClientContext context;
        Status status;

        // Set deadline for GRPC client
        if (deadline > 0) {
            auto tDeadline = std::chrono::system_clock::now() + std::chrono::seconds(deadline);
            context.set_deadline(tDeadline);
        }

        // Set common name from cert.perm
        char common_name_value[ClientBaseConstants::COMMON_NAME_LEN] = { 0 };
        ret = get_common_name_from_tls_cert(m_certFile.c_str(), common_name_value,
                                            ClientBaseConstants::COMMON_NAME_LEN);
        if (ret != 0) {
            ERROR("Failed to get common name in: %s", m_certFile.c_str());
            return -1;
        }
        context.AddMetadata("username", std::string(common_name_value, strlen(common_name_value)));
        context.AddMetadata("tls_mode", m_tlsMode);

        ret = request_to_grpc(request, &req);
        if (ret != 0) {
            ERROR("Failed to translate request to grpc");
            response->cc = LCRD_ERR_INPUT;
            return -1;
        }

        if (check_parameter(req)) {
            response->cc = LCRD_ERR_INPUT;
            return -1;
        }

        status = grpc_call(&context, req, &reply);
        if (!status.ok()) {
            ERROR("error_code: %d: %s", status.error_code(), status.error_message().c_str());
            unpackStatus(status, response);
            return -1;
        }

        ret = response_from_grpc(&reply, response);
        if (ret != 0) {
            ERROR("Failed to transform grpc response");
            response->cc = LCRD_ERR_EXEC;
            return -1;
        }

        if (response->server_errono != LCRD_SUCCESS) {
            response->cc = LCRD_ERR_EXEC;
        }

        return (response->cc == LCRD_SUCCESS) ? 0 : -1;
    }

protected:
    virtual int request_to_grpc(const RQ *rq, gRQ *grq)
    {
        return 0;
    };
    virtual int response_from_grpc(gRP *reply, RP *response)
    {
        return 0;
    };
    virtual int check_parameter(const gRQ &grq)
    {
        return 0;
    };
    virtual Status grpc_call(ClientContext *context, const gRQ &req, gRP *reply)
    {
        return Status::OK;
    };

    static std::string ReadTextFile(const char *file)
    {
        char *real_file = verify_file_and_get_real_path(file);
        if (real_file == nullptr) {
            return "";
        }
        std::ifstream context(real_file, std::ios::in);
        if (!context) {
            free(real_file);
            return "";
        }
        std::stringstream ss;
        if (context.is_open()) {
            ss << context.rdbuf();
            context.close();
        }
        free(real_file);
        return ss.str();
    }

    std::unique_ptr<sTB> stub_;
    std::string m_tlsMode { ClientBaseConstants::TLS_OFF };
    std::string m_certFile { "" };

    unsigned int deadline;
};

template <class REQUEST, class RESPONSE, class FUNC>
int container_func(const REQUEST *request, RESPONSE *response, void *arg) noexcept
{
    if (request == nullptr || response == nullptr || arg == nullptr) {
        ERROR("Receive NULL args");
        return -1;
    }

    std::unique_ptr<FUNC> client(new (std::nothrow) FUNC(arg));
    if (client == nullptr) {
        ERROR("Out of memory");
        return -1;
    }

    return client->run(request, response);
}

#endif /* __CLIENT_BASH_H */

