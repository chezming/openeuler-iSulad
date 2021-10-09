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
 * Create: 2020-09-03
 * Description: provide checkpoint functions
 *********************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <isula_libutils/defs.h>
#include <isula_libutils/json_common.h>

#include "callback.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "checkpoint_cb.h"
#include "utils.h"
#include "error.h"
#include "err_msg.h"
#include "isula_libutils/log.h"
#include "checkpoint_api.h"
#include "container_api.h"


/* checkpoint create cb */
static int checkpoint_create_cb(const checkpoint_create_checkpoint_request *request, checkpoint_create_checkpoint_response **response)
{
    uint32_t cc = ISULAD_SUCCESS;

    container_t *cont = NULL;
    DAEMON_CLEAR_ERRMSG();

    if (request == NULL || request->container==NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return EINVALIDARGS;
    }

    *response = util_common_calloc_s(sizeof(checkpoint_create_checkpoint_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto out;
    }

    EVENT("Checkpoint Event: {Object: create checkpoint, Type: Create}");

    cont = containers_store_get(request->container);
    if (cont == NULL) {
        cc = ISULAD_ERR_EXEC;
        ERROR("No such container:%s", request->container);
        isulad_set_error_message("No such container:%s", request->container);
        goto out;
    }


    char* id = cont->common_config->id;

    
    if (checkpoint_create(id,request->dir)!=0) {
        cc = ISULAD_ERR_EXEC;
        goto out;
    }
   

    

    EVENT("Checkpoint Event: {Object: create checkpoints, Type: Created");

out:
    if (*response != NULL) {
        (*response)->cc = cc;
        if (g_isulad_errmsg != NULL) {
            (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
            DAEMON_CLEAR_ERRMSG();
        }
    }


    return (cc != ISULAD_SUCCESS) ? ECOMMON : 0;
}

static int checkpoint_restore_cb(const checkpoint_restore_checkpoint_request *request, checkpoint_restore_checkpoint_response **response)
{
    uint32_t cc = ISULAD_SUCCESS;
container_t *cont = NULL;

    DAEMON_CLEAR_ERRMSG();

    if (request == NULL || request->container==NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return EINVALIDARGS;
    }

    *response = util_common_calloc_s(sizeof(checkpoint_restore_checkpoint_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto out;
    }

    EVENT("Checkpoint Event: {Object: restore checkpoint, Type: Create}");

    cont = containers_store_get(request->container);
    if (cont == NULL) {
        cc = ISULAD_ERR_EXEC;
        ERROR("No such container:%s", request->container);
        isulad_set_error_message("No such container:%s", request->container);
        goto out;
    }


    char* id = cont->common_config->id;

  
    if (checkpoint_restore(id,request->dir)!=0) {
        cc = ISULAD_ERR_EXEC;
        goto out;
    }
    

    

    EVENT("Checkpoint Event: {Object: restore checkpoints, Type: Created");

out:
    if (*response != NULL) {
        (*response)->cc = cc;
        if (g_isulad_errmsg != NULL) {
            (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
            DAEMON_CLEAR_ERRMSG();
        }
    }


    return (cc != ISULAD_SUCCESS) ? ECOMMON : 0;
}

static int checkpoint_remove_cb(const checkpoint_remove_checkpoint_request *request, checkpoint_remove_checkpoint_response **response)
{
    uint32_t cc = ISULAD_SUCCESS;

    container_t *cont = NULL;
    DAEMON_CLEAR_ERRMSG();

    if (request == NULL || request->container==NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return EINVALIDARGS;
    }

    


    *response = util_common_calloc_s(sizeof(checkpoint_remove_checkpoint_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto out;
    }

    cont = containers_store_get(request->container);
    if (cont == NULL) {
        cc = ISULAD_ERR_EXEC;
        isulad_set_error_message("No such container:%s", request->container);
        ERROR("No such container:%s", request->container);
        goto out;
    }

    EVENT("Checkpoint Event: {Object: remove checkpoint, Type: Removing}");


    char* id = cont->common_config->id;

    if(checkpoint_remove(id,request->dir)!=0){
        cc = ISULAD_ERR_EXEC;
        goto out;
    }
    

    EVENT("Checkpoint Event: {Object: remove checkpoints, Type: Deleted");

out:
    if (*response != NULL) {
        (*response)->cc = cc;
        if (g_isulad_errmsg != NULL) {
            (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
            DAEMON_CLEAR_ERRMSG();
        }
    }

    return (cc != ISULAD_SUCCESS) ? ECOMMON : 0;
}

static int checkpoint_list_cb(const checkpoint_list_checkpoint_request *request, checkpoint_list_checkpoint_response **response)
{
    uint32_t cc = ISULAD_SUCCESS;
    struct checkpoints *list = NULL;
    size_t i=0;
    checkpoint_checkpoint *che = NULL;

    DAEMON_CLEAR_ERRMSG();

    if (request == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return EINVALIDARGS;
    }

    *response = util_common_calloc_s(sizeof(checkpoint_list_checkpoint_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto out;
    }

    EVENT("Checkpoint Event: {Object: list checkpoint, Type: listing}");
    printf("request->dir:%s\n",request->dir);
    list = checkpoint_list(request->dir);
    if (list == NULL) {
        cc = ISULAD_ERR_EXEC;
        goto out;
    }
     if (list->ches_len == 0) {
        goto out;
    }

     (*response)->checkpoints = util_smart_calloc_s(sizeof(checkpoint_checkpoint *), list->ches_len);
    if ((*response)->checkpoints == NULL) {
        ERROR("out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto err_out;
    }
    for (i = 0; i < list->ches_len; i++) {
        che = util_common_calloc_s(sizeof(checkpoint_checkpoint));
        if (che == NULL) {
            ERROR("out of memory");
            cc = ISULAD_ERR_MEMOUT;
            goto err_out;
        }
        che->dir = util_strdup_s(list->ches[i]->dir);
        che->name = util_strdup_s(list->ches[i]->name);
        (*response)->checkpoints[i] = che;
        (*response)->checkpoints_len++;
    }

    
out:
    EVENT("Checkpoint Event: {Object: list checkpoints, Type: listed");

err_out:
    if (*response != NULL) {
        (*response)->cc = cc;
        if (g_isulad_errmsg != NULL) {
            (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
            DAEMON_CLEAR_ERRMSG();
        }
    }


    return (cc != ISULAD_SUCCESS) ? ECOMMON : 0;
}


/* checkpoint callback init */
void checkpoint_callback_init(service_checkpoint_callback_t *cb)
{
    if (cb == NULL) {
        ERROR("Invalid input arguments");
        return;
    }

    cb->create = checkpoint_create_cb;
    cb->remove = checkpoint_remove_cb;
    cb->list   = checkpoint_list_cb;
    cb->restore= checkpoint_restore_cb;
}
