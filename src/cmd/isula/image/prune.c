/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
 *
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Author: Chen Wei
 * Create: 2023-05-11
 * Description: command image prune
 ******************************************************************************/
#include "prune.h"
#include <isula_libutils/log.h>
#include "client_arguments.h"
#include "isula_connect.h"
#include "isula_commands.h"
#include "connect.h"
#include "protocol_type.h"
#include "utils.h"

const char g_cmd_image_prune_desc[] = "Remove all unused images";
const char g_cmd_image_prune_usage[] = "prune [OPTIONS]";

struct client_arguments g_cmd_image_prune_args;

// Add filter "dangling" true or false
static void add_filter_dangling(struct isula_image_prune_request request, bool if_all)
{
    request.filters->keys[request.filters->len] = util_strdup_s("dangling");
    if (!if_all) {
        request.filters->values[request.filters->len] = util_strdup_s("true");
    } else {
        request.filters->values[request.filters->len] = util_strdup_s("false");
    }
    request.filters->len++;
}

static int client_image_prune(const struct client_arguments *args, char ***deleted_images, size_t *deleted_cnt,
                              size_t *space_reclaimed)
{
    isula_connect_ops *ops = NULL;
    struct isula_image_prune_request request = { 0 };
    struct isula_image_prune_response *response = NULL;
    client_connect_config_t config = { 0 };
    int ret = 0;

    if (args->filters != NULL) {
        int len = util_array_len((const char **)(args->filters));

        request.filters =
            isula_filters_parse_args((const char **)args->filters, len);
        if (request.filters == NULL) {
            ERROR("Failed to parse filters args");
            ret = -1;
            goto out;
        }
        if (util_mem_realloc((void **)&(request.filters->keys), sizeof(char *) * (len + 1), (void *)request.filters->keys, sizeof(char *) * len) != 0) {
            ERROR("out of memory");
            ret = -1;
            goto out;
        }
        if (util_mem_realloc((void **)&(request.filters->values), sizeof(char *) * (len + 1), (void *)request.filters->values, sizeof(char *) * len) != 0) {
            ERROR("out of memory");
            ret = -1;
            goto out;
        }
    } else {
        request.filters = util_common_calloc_s(sizeof(struct isula_filters));;
        if (request.filters == NULL) {
            ERROR("out of memory");
            ret = -1;
            goto out;
        }
        request.filters->len = 0;
        request.filters->keys = util_smart_calloc_s(sizeof(char *), 1);
        if (request.filters->keys == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }
        request.filters->values = util_smart_calloc_s(sizeof(char *), 1);
        if (request.filters->values == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }
    }
    add_filter_dangling(request, args->all);

    response = util_common_calloc_s(sizeof(struct isula_image_prune_response));
    if (response == NULL) {
        COMMAND_ERROR("Out of memory");
        ret = -1;
		goto out;
    }

    ops = get_connect_client_ops();
    if (ops == NULL || ops->image.prune == NULL) {
        COMMAND_ERROR("Unimplemented ops");
        ret = -1;
        goto out;
    }
    config = get_connect_config(args);
    ret = ops->image.prune(&request, response, &config);
    if (ret != 0) {
        client_print_error(response->cc, response->server_errono, response->errmsg);
        if (response->server_errono) {
            ret = ESERVERERROR;
        }
        goto out;
    }
    ret = util_dup_array_of_strings((const char **)response->images, response->images_len, deleted_images, deleted_cnt);
    if (ret != 0) {
        COMMAND_ERROR("dup images failed");
        goto out;
    }
    *space_reclaimed = response->space_reclaimed;

out:
    isula_filters_free(request.filters);
    isula_image_prune_response_free(response);
    return ret;
}

int cmd_image_prune_main(int argc, const char **argv)
{
    int i = 0;
    struct isula_libutils_log_config lconf = { 0 };
    command_t cmd;
    char **deleted_images = NULL;
    size_t deleted_cnt = 0;
    size_t space_reclaimed = 0;
    struct command_option options[] = { LOG_OPTIONS(lconf) COMMON_OPTIONS(g_cmd_image_prune_args)
        PRUNE_OPTIONS(g_cmd_image_prune_args)
    };

    if (client_arguments_init(&g_cmd_image_prune_args)) {
        COMMAND_ERROR("client arguments init failed");
        exit(ECOMMON);
    }

    g_cmd_image_prune_args.progname = util_string_join(" ", argv, 2);
    isula_libutils_default_log_config(argv[0], &lconf);
    subcommand_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_image_prune_desc,
                    g_cmd_image_prune_usage);

    if (command_parse_args(&cmd, &g_cmd_image_prune_args.argc, &g_cmd_image_prune_args.argv)) {
        exit(ECOMMON);
    }
    if (isula_libutils_log_enable(&lconf)) {
        COMMAND_ERROR("image prune: log init failed");
        exit(ECOMMON);
    }

    if (g_cmd_image_prune_args.argc != 0) {
        COMMAND_ERROR("%s: \"image prune\" requires exactly 0 arguments.", g_cmd_image_prune_args.progname);
        exit(ECOMMON);
    }

    if (!g_cmd_image_prune_args.force) {
        char *prompt_msg;
        if (g_cmd_image_prune_args.all) {
            prompt_msg = "WARNING! This will remove all unused images.";
        } else {
            prompt_msg = "WARNING! This will remove all dangling images.";
        }

        if (!user_prompt(prompt_msg, "Are you sure you want to continue? [y/n]", check_input_yes)) {
            exit(EXIT_SUCCESS);
        }
    }

    if (client_image_prune(&g_cmd_image_prune_args, &deleted_images, &deleted_cnt, &space_reclaimed) != 0) {
        ERROR("Prune images failed");
        COMMAND_ERROR("Prune images failed");
        exit(ECOMMON);
    }

    for (i = 0; i < deleted_cnt; i++) {
        printf("%s\n", deleted_images[i]);
    }
    printf("Total reclaimed space: %zu Bytes\n", space_reclaimed);

	util_free_array(deleted_images);
    exit(EXIT_SUCCESS);
}
