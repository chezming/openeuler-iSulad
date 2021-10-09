#include "grpc_checkpoints_service.h"

#include <unistd.h>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <syslog.h>

#include "isula_libutils/log.h"
#include "utils.h"
#include "grpc_server_tls_auth.h"



//获取请求
int CheckpointServiceImpl::checkpoint_create_request_from_grpc(const CreateCheckpointRequest *grequest,
                                                      checkpoint_create_checkpoint_request **request)
{
   
    printf("checkpoint_create_request_from_grpc\n");
    printf("dir:%s\n",grequest->dir().c_str());
    checkpoint_create_checkpoint_request *tmpreq =
        static_cast<checkpoint_create_checkpoint_request *>(util_common_calloc_s(sizeof(checkpoint_create_checkpoint_request)));
    if (tmpreq == nullptr) {
        printf("tmpreq == nullptr?:%d\n",tmpreq == nullptr);
        ERROR("Out of memory");
        return -1;
    }
   

    if (!grequest->container().empty()) {
        tmpreq->container = util_strdup_s(grequest->container().c_str());
    }
     if (!grequest->dir().empty()) {
        tmpreq->dir = util_strdup_s(grequest->dir().c_str());
    }
    *request = tmpreq;

    return 0;
}

//发送响应
int CheckpointServiceImpl::checkpoint_create_response_to_grpc(checkpoint_create_checkpoint_response *response,
                                                     CreateCheckpointResponse *gresponse)
{
    printf("发送响应\n");
    if (response == nullptr) {
        gresponse->set_cc(ISULAD_ERR_MEMOUT);
        return 0;
    }

    gresponse->set_cc(response->cc);
    if (response->errmsg != nullptr) {
        gresponse->set_errmsg(response->errmsg);
    }


    return 0;
}


//处理逻辑
Status CheckpointServiceImpl::Create(ServerContext *context, const CreateCheckpointRequest *request, CreateCheckpointResponse *reply)
{
    printf("server contianer is null:%d\n",request->container().empty());
    //tls认证
    auto status = GrpcServerTlsAuth::auth(context, "checkpoint_create");
    if (!status.ok()) {
        openlog("isula",LOG_CONS | LOG_PID,LOG_LOCAL2);
	    syslog(LOG_DEBUG,"tls认证失败\n");
	    closelog();
        return status;
    }
    //获取服务执行器
    service_executor_t *cb = get_service_executor();
    //获取为空
    if (cb == nullptr || cb->checkpoint.create == nullptr) {
        return Status(StatusCode::UNIMPLEMENTED, "Unimplemented callback");
    }
    //声明请求
    checkpoint_create_checkpoint_request *checkpoint_req = nullptr;
    //填充请求
    int tret = checkpoint_create_request_from_grpc(request, &checkpoint_req);
    if (tret != 0) {
        ERROR("Failed to transform grpc request");
        reply->set_cc(ISULAD_ERR_INPUT);
        return Status::OK;
    }

    //声明响应
    checkpoint_create_checkpoint_response *checkpoint_res = nullptr;
    //填充响应
    int ret = cb->checkpoint.create(checkpoint_req, &checkpoint_res);
    //发送响应
    tret = checkpoint_create_response_to_grpc(checkpoint_res, reply);
    free(checkpoint_req);
    free(checkpoint_res);
    if (tret != 0) {
        openlog("isula",LOG_CONS | LOG_PID,LOG_LOCAL2);
	    syslog(LOG_DEBUG,"发送响应失败\n");
	    closelog();
        reply->set_errmsg(util_strdup_s(errno_to_error_message(ISULAD_ERR_INTERNAL)));
        reply->set_cc(ISULAD_ERR_INPUT);
        ERROR("Failed to translate response to grpc, operation is %s", ret ? "failed" : "success");
    }

    return Status::OK;
}

//获取请求
int CheckpointServiceImpl::checkpoint_restore_request_from_grpc(const RestoreCheckpointRequest *grequest,
                                                      checkpoint_restore_checkpoint_request **request)
{
   
    printf("checkpoint_restore_request_from_grpc\n");
    checkpoint_restore_checkpoint_request *tmpreq =
        static_cast<checkpoint_restore_checkpoint_request *>(util_common_calloc_s(sizeof(checkpoint_restore_checkpoint_request)));
    if (tmpreq == nullptr) {
        printf("tmpreq == nullptr?:%d\n",tmpreq == nullptr);
        ERROR("Out of memory");
        return -1;
    }
    printf("container is empty:%d\n",grequest->container().empty());

    if (!grequest->container().empty()) {
        tmpreq->container = util_strdup_s(grequest->container().c_str());
    }
     if (!grequest->dir().empty()) {
        tmpreq->dir = util_strdup_s(grequest->dir().c_str());
    }
    *request = tmpreq;

    return 0;
}

//发送响应
int CheckpointServiceImpl::checkpoint_restore_response_to_grpc(checkpoint_restore_checkpoint_response *response,
                                                     RestoreCheckpointResponse *gresponse)
{
    printf("发送响应\n");
    if (response == nullptr) {
        gresponse->set_cc(ISULAD_ERR_MEMOUT);
        return 0;
    }

    gresponse->set_cc(response->cc);
    if (response->errmsg != nullptr) {
        gresponse->set_errmsg(response->errmsg);
    }

   // for (size_t i {}; i < response->checkpoints_len; i++) {
     //   gresponse->add_checkpoints(response->checkpoints[i]);
    //}

    return 0;
}


//处理逻辑
Status CheckpointServiceImpl::Restore(ServerContext *context, const RestoreCheckpointRequest *request, RestoreCheckpointResponse *reply)
{
    printf("server contianer is null:%d\n",request->container().empty());
    //tls认证
    auto status = GrpcServerTlsAuth::auth(context, "checkpoint_restore");
    if (!status.ok()) {
        openlog("isula",LOG_CONS | LOG_PID,LOG_LOCAL2);
	    syslog(LOG_DEBUG,"tls认证失败\n");
	    closelog();
        return status;
    }
    //获取服务执行器
    service_executor_t *cb = get_service_executor();
    //获取为空
    if (cb == nullptr || cb->checkpoint.restore == nullptr) {
        return Status(StatusCode::UNIMPLEMENTED, "Unimplemented callback");
    }
    //声明请求
    checkpoint_restore_checkpoint_request *checkpoint_req = nullptr;
    //填充请求
    int tret = checkpoint_restore_request_from_grpc(request, &checkpoint_req);
    if (tret != 0) {
        ERROR("Failed to transform grpc request");
        reply->set_cc(ISULAD_ERR_INPUT);
        return Status::OK;
    }

    //声明响应
    checkpoint_restore_checkpoint_response *checkpoint_res = nullptr;
    //填充响应
    int ret = cb->checkpoint.restore(checkpoint_req, &checkpoint_res);
    //发送响应
    tret = checkpoint_restore_response_to_grpc(checkpoint_res, reply);
    free(checkpoint_req);
    free(checkpoint_res);
    if (tret != 0) {
        openlog("isula",LOG_CONS | LOG_PID,LOG_LOCAL2);
	    syslog(LOG_DEBUG,"发送响应失败\n");
	    closelog();
        reply->set_errmsg(util_strdup_s(errno_to_error_message(ISULAD_ERR_INTERNAL)));
        reply->set_cc(ISULAD_ERR_INPUT);
        ERROR("Failed to translate response to grpc, operation is %s", ret ? "failed" : "success");
    }

    return Status::OK;
}

//获取请求
int CheckpointServiceImpl::checkpoint_remove_request_from_grpc(const RemoveCheckpointRequest *grequest,
                                                      checkpoint_remove_checkpoint_request **request)
{
   
    checkpoint_remove_checkpoint_request *tmpreq =
        static_cast<checkpoint_remove_checkpoint_request *>(util_common_calloc_s(sizeof(checkpoint_remove_checkpoint_request)));
    if (tmpreq == nullptr) {
        ERROR("Out of memory");
        return -1;
    }
   

    if (!grequest->container().empty()) {
        tmpreq->container = util_strdup_s(grequest->container().c_str());
    }
    if (!grequest->dir().empty()) {
       tmpreq->dir = util_strdup_s(grequest->dir().c_str());
    }
    
    *request = tmpreq;

    return 0;
}


//处理逻辑
Status CheckpointServiceImpl::Remove(ServerContext *context, const RemoveCheckpointRequest *request, RemoveCheckpointResponse *reply)
{
   
    //tls认证
    auto status = GrpcServerTlsAuth::auth(context, "checkpoint_create");
    if (!status.ok()) {
        return status;
    }
    //获取服务执行器
    service_executor_t *cb = get_service_executor();
    //获取为空
    if (cb == nullptr || cb->checkpoint.remove == nullptr) {
        return Status(StatusCode::UNIMPLEMENTED, "Unimplemented callback");
    }
    //声明请求
    checkpoint_remove_checkpoint_request *checkpoint_req = nullptr;
    
    //填充请求
    int tret = checkpoint_remove_request_from_grpc(request, &checkpoint_req);
    if (tret != 0) {
        ERROR("Failed to transform grpc request");
        reply->set_cc(ISULAD_ERR_INPUT);
        return Status::OK;
    }

    //声明响应
    checkpoint_remove_checkpoint_response *checkpoint_res = nullptr;
    //填充响应
    int ret = cb->checkpoint.remove(checkpoint_req, &checkpoint_res);
    
    //发送响应
    tret = response_to_grpc(checkpoint_res, reply);
    free(checkpoint_req);
    free(checkpoint_res);
    if (tret != 0) {
        
        reply->set_errmsg(util_strdup_s(errno_to_error_message(ISULAD_ERR_INTERNAL)));
        reply->set_cc(ISULAD_ERR_INPUT);
        ERROR("Failed to translate response to grpc, operation is %s", ret ? "failed" : "success");
    }

    return Status::OK;
}

//获取请求
int CheckpointServiceImpl::checkpoint_list_request_from_grpc(const ListCheckpointRequest *grequest,
                                                      checkpoint_list_checkpoint_request **request)
{
    checkpoint_list_checkpoint_request *tmpreq =
        static_cast<checkpoint_list_checkpoint_request *>(util_common_calloc_s(sizeof(checkpoint_list_checkpoint_request)));
    if (tmpreq == nullptr) {
        ERROR("Out of memory");
        return -1;
    }

     if (!grequest->dir().empty()) {
        tmpreq->dir = util_strdup_s(grequest->dir().c_str());
    }

   
    *request = tmpreq;

    return 0;
}

//发送响应
int CheckpointServiceImpl::checkpoint_list_response_to_grpc(checkpoint_list_checkpoint_response *response,
                                                     ListCheckpointResponse *gresponse)
{
    if (response == nullptr) {
        gresponse->set_cc(ISULAD_ERR_MEMOUT);
        return 0;
    }

    gresponse->set_cc(response->cc);
    if (response->errmsg != nullptr) {
        gresponse->set_errmsg(response->errmsg);
    }
    for (size_t i {}; i < response->checkpoints_len; i++) {
        auto checkpoint = gresponse->add_checkpoints();
        if (response->checkpoints[i]->dir != nullptr) {
            checkpoint->set_dir(response->checkpoints[i]->dir);
        }
        if (response->checkpoints[i]->name != nullptr) {
            checkpoint->set_name(response->checkpoints[i]->name);
        }
    }

    return 0;
}


//处理逻辑
Status CheckpointServiceImpl::List(ServerContext *context, const ListCheckpointRequest *request, ListCheckpointResponse *reply)
{
    //tls认证
    auto status = GrpcServerTlsAuth::auth(context, "checkpoint_list");
    if (!status.ok()) {
        return status;
    }
    //获取服务执行器
    service_executor_t *cb = get_service_executor();
    
    //获取为空
    if (cb == nullptr || cb->checkpoint.list == nullptr) {
        return Status(StatusCode::UNIMPLEMENTED, "Unimplemented callback");
    }
    //声明请求
    checkpoint_list_checkpoint_request *checkpoint_req = nullptr;
    //填充请求
    int tret = checkpoint_list_request_from_grpc(request, &checkpoint_req);
    if (tret != 0) {
        ERROR("Failed to transform grpc request");
        reply->set_cc(ISULAD_ERR_INPUT);
        return Status::OK;
    }

    //声明响应
    checkpoint_list_checkpoint_response *checkpoint_res = nullptr;
    //填充响应
    int ret = cb->checkpoint.list(checkpoint_req, &checkpoint_res);
    //发送响应
    tret = checkpoint_list_response_to_grpc(checkpoint_res, reply);
    free(checkpoint_req);
    free(checkpoint_res);
    if (tret != 0) {
        reply->set_errmsg(util_strdup_s(errno_to_error_message(ISULAD_ERR_INTERNAL)));
        reply->set_cc(ISULAD_ERR_INPUT);
        ERROR("Failed to translate response to grpc, operation is %s", ret ? "failed" : "success");
    }

    return Status::OK;
}

