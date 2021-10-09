#ifndef DAEMON_EXECUTOR_CHECKPOINT_CB_CHECKPOINT_CB_H
#define DAEMON_EXECUTOR_CHECKPOINT_CB_CHECKPOINT_CB_H

#include "callback.h"

#ifdef __cplusplus
extern "C" {
#endif

void checkpoint_callback_init(service_checkpoint_callback_t *cb);

#ifdef __cplusplus
}
#endif

#endif