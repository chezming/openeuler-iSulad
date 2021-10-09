
#ifndef CMD_ISULA_CHECKPOINT_CREATE_H
#define CMD_ISULA_CHECKPOINT_CREATE_H

#include <stdbool.h>
#include <stddef.h>

#include "client_arguments.h"
#include "command_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char g_cmd_checkpoint_create_desc[];
extern const char g_cmd_checkpoint_create_usage[];
extern struct client_arguments g_cmd_checkpoint_create_args;
int cmd_checkpoint_create_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif // CMD_ISULA_CHECKPOINT_CREATE_H
