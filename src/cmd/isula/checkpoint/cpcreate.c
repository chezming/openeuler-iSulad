/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wangfengtu
 * Create: 2020-09-04
 * Description: provide checkpoint remove functions
 ******************************************************************************/
#include "cpcreate.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <lxc/lxccontainer.h>

#include "utils.h"
#include "client_arguments.h"
#include "isula_connect.h"
#include "isula_libutils/log.h"
#include "connect.h"
#include "protocol_type.h"
#include <syslog.h>

const char g_cmd_checkpoint_create_desc[] =
    "Create a checkpoint from a running container";
const char g_cmd_checkpoint_create_usage[] = "create [OPTIONS] CONTAINER";

struct client_arguments g_cmd_checkpoint_create_args;

#define CREATE_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_STRING, false, "checkpoint-dir", 0, &(cmdargs).checkpoint_dir, "Use a custom checkpoint storage directory", NULL },


static int client_checkpoint_create(const struct client_arguments *args, char ***checkpoints, size_t *checkpoints_len){
   
    isula_connect_ops *ops =NULL;
    struct isula_create_checkpoint_request request ={0};
    struct isula_create_checkpoint_response *response =NULL;
    client_connect_config_t config ={0};
    int ret = 0;

    response = util_common_calloc_s(sizeof(struct isula_create_checkpoint_response));
    if (response==NULL){
        ERROR("Out of memory");
        return -1;
    }
    request.container = args->name;
    request.dir=args->checkpoint_dir;
  

    ops = get_connect_client_ops();
    
    if(ops==NULL || ops->checkpoint.create==NULL){
        ERROR("Unimplemented ops");
        ret=-1;
        goto out;
    }
    //把参数放到了config里
    config = get_connect_config(args);
   
    ret=ops->checkpoint.create(&request,response,&config);
   
    if(ret!=0){
        client_print_error(response->cc,response->server_errono,response->errmsg);
        if(response->server_errono){
            ret=ESERVERERROR;
        }

        goto out;
    }
    printf("%s\n",args->name);

out:
    //isula_create_checkpoint_response_free(response);
    
    return ret;
}

int cmd_checkpoint_create_main(int argc, const char **argv)
{
    //int i = 0;
    struct isula_libutils_log_config lconf = { 0 };
    int exit_code = 1;
    command_t cmd;
    char **checkpoints = NULL;
    size_t checkpoints_len = 0;
   // char ch = 'n';
    struct command_option options[] = { 
        CREATE_OPTIONS(g_cmd_checkpoint_create_args)
    };
    //printf("%s",options[0])

    if (client_arguments_init(&g_cmd_checkpoint_create_args)) {
        COMMAND_ERROR("client arguments init failed");
        exit(ECOMMON);
    }
    g_cmd_checkpoint_create_args.progname = util_string_join(" ", argv, 2);
    isula_libutils_default_log_config(argv[0], &lconf);
    subcommand_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_checkpoint_create_desc,
                    g_cmd_checkpoint_create_usage);

    if (command_parse_args(&cmd, &g_cmd_checkpoint_create_args.argc, &g_cmd_checkpoint_create_args.argv)) {
        exit(exit_code);
    }
  
    //printf("%s\n",g_cmd_checkpoint_create_args.checkpoint_dir);
    if (isula_libutils_log_enable(&lconf)) {
        COMMAND_ERROR("checkpoint create: log init failed");
        exit(exit_code);
    }


    g_cmd_checkpoint_create_args.name=g_cmd_checkpoint_create_args.argv[0];
    
    if(g_cmd_checkpoint_create_args.argc>1){
        g_cmd_checkpoint_create_args.checkpoint_dir=g_cmd_checkpoint_create_args.argv[1];
    }
    
    //printf("%s\n",g_cmd_checkpoint_create_args.name);
    if (client_checkpoint_create(&g_cmd_checkpoint_create_args, &checkpoints, &checkpoints_len) != 0) {
        ERROR("Create checkpoints failed");
        exit(exit_code);
    }


    exit(EXIT_SUCCESS);
    
}
