#ifndef DAEMON_ENTRY_CONNECT_GRPC_GRPC_CHECKPOINTS_SERVICE_H
#define DAEMON_ENTRY_CONNECT_GRPC_GRPC_CHECKPOINTS_SERVICE_H

#include <grpc++/grpc++.h>


#include "callback.h"
#include "error.h"
#include "checkpoints.grpc.pb.h"

using grpc::ServerContext;
using grpc::Status;

using namespace checkpoint;



// Implement of checkpoint service
class CheckpointServiceImpl final : public CheckpointService::Service {
public:
    CheckpointServiceImpl() = default;
    CheckpointServiceImpl(const CheckpointServiceImpl &) = delete;
    CheckpointServiceImpl &operator=(const CheckpointServiceImpl &) = delete;
    virtual ~CheckpointServiceImpl() = default;

    Status Create(ServerContext *context, const CreateCheckpointRequest *request, CreateCheckpointResponse *reply) override;
    Status Remove(ServerContext *context, const RemoveCheckpointRequest *request, RemoveCheckpointResponse *reply) override;
    Status List(ServerContext *context, const ListCheckpointRequest *request, ListCheckpointResponse *reply) override;
    Status Restore(ServerContext *context, const RestoreCheckpointRequest *request, RestoreCheckpointResponse *reply) override;

private:
    template <class T1, class T2>
    int response_to_grpc(const T1 *response, T2 *gresponse)
    {
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

    int checkpoint_create_request_from_grpc(const CreateCheckpointRequest *grequest, checkpoint_create_checkpoint_request **request);

    int checkpoint_create_response_to_grpc(checkpoint_create_checkpoint_response *response, CreateCheckpointResponse *gresponse);

    int checkpoint_remove_request_from_grpc(const RemoveCheckpointRequest *grequest, checkpoint_remove_checkpoint_request **request);

    int checkpoint_remove_response_to_grpc(checkpoint_remove_checkpoint_response *response, RemoveCheckpointResponse *gresponse);

    int checkpoint_list_request_from_grpc(const ListCheckpointRequest *grequest, checkpoint_list_checkpoint_request **request);

    int checkpoint_list_response_to_grpc(checkpoint_list_checkpoint_response *response, ListCheckpointResponse *gresponse);

    int checkpoint_restore_request_from_grpc(const RestoreCheckpointRequest *grequest, checkpoint_restore_checkpoint_request **request);

    int checkpoint_restore_response_to_grpc(checkpoint_restore_checkpoint_response *response, RestoreCheckpointResponse *gresponse);
};

#endif // DAEMON_ENTRY_CONNECT_GRPC_GRPC_CHECKPOINTS_SERVICE_H
