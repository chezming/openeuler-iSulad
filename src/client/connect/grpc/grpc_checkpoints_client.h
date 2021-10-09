#ifndef CLIENT_CONNECT_GRPC_GRPC_CHECKPOINTS_CLIENT_H
#define CLIENT_CONNECT_GRPC_GRPC_CHECKPOINTS_CLIENT_H

#include "isula_connect.h"

#ifdef __cplusplus
extern "C" {
#endif

int grpc_checkpoints_client_ops_init(isula_connect_ops *ops);

#ifdef __cplusplus
}
#endif

#endif