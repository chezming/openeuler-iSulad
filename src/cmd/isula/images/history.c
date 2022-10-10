/******************************************************************************
 * Copyright (c) KylinSoft  Co., Ltd. 2022. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: huangsong
 * Create: 2022-10-04
 * Description: provide image history definition
 ********************************************************************************/
#include "history.h"

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "client_arguments.h"
#include "isula_connect.h"
#include "isula_libutils/log.h"
#include "command_parser.h"
#include "connect.h"
#include "utils_timestamp.h"



const char g_cmd_history_desc[] = "Show the history of an image";
const char g_cmd_history_usage[] = "history [OPTIONS] IMAGE";

struct client_arguments g_cmd_history_args = {};

struct lengths {
    unsigned int image_length;
    unsigned int created_length;
    unsigned int created_by_length;
    unsigned int size_length;
    unsigned int comment_length;
    unsigned int created_by_description_length;
};

#define SHORT_DIGEST_LEN 12

/* list print table */
static void list_print_table(struct isula_history_info *history_list, const size_t size, const struct lengths *length)
{
    char *digest = NULL;
    char *image_size = NULL;
    char *created = NULL;
    int space_length = 0;
    size_t i = 0;
    /* print header */
    char tmpbuffer[PATH_MAX];
    int len = 0;
    len = snprintf(tmpbuffer, PATH_MAX, "%-*s %-*s%-*s %-*s %-*s", 
        (int)length->image_length, "IMAGE",
        (int)length->created_length, "CREATED",
        (int)length->created_by_length, "CREATED BY",
        (int)length->size_length, "SIZE",
        (int)length->comment_length, "COMMENT");
    if (len < 0 || len >= PATH_MAX) {
        ERROR("Failed to print string");
        return;
    }
    printf("%s",tmpbuffer);
    printf("\n");

    for (i = 0; i < size; i++)
    {
        digest = util_sub_string(history_list[i].id, 0, SHORT_DIGEST_LEN);
        printf("%-*s ", (int)length->image_length, digest ? digest : "-");
        free(digest);

        created = util_format_time(history_list[i].created);
        printf("%-*s", (int)length->created_length, created ? created : "-");
        free(created);

        printf("%-*.*s", (int)length->created_by_description_length, (int)length->created_by_description_length,
             history_list[i].create_by ? history_list[i].create_by : "-");
        space_length = (int)length->created_by_length - (int)length->created_by_description_length;
        printf("%-*.*s ", space_length, space_length, strlen(history_list[i].create_by) > (int)length->created_by_description_length ? "..." : "   ");
        image_size = util_human_size_decimal(history_list[i].size);
        printf("%-*s ", (int)length->size_length, image_size ? image_size : "-");
        free(image_size);

        printf("%-*s ", (int)length->comment_length, history_list[i].comment ? history_list[i].comment : "-");
        printf("\n");
    }
    
}

/*
 * list all history info from image
 */
static void history_info_print(const struct isula_history_response *response)
{
    const unsigned int base_len = 20;
    const unsigned int created_length = 25;
    const unsigned int created_by_len = 50;
    const unsigned int created_by_description_length = 44;
    struct lengths max_len = {
        .image_length = base_len, 
        .created_length = created_length,
        .created_by_length = created_by_len, 
        .created_by_description_length = created_by_description_length,
        .size_length = base_len, 
        .comment_length = base_len, 
    };

    list_print_table(response->history_list, (size_t)response->history_num, &max_len);
}

/*
 * Show the history of an image
 */
int client_history(const struct client_arguments *args)
{
    isula_connect_ops *ops = NULL;
    struct isula_history_request request = { 0 };
    struct isula_history_response *response = NULL;
    client_connect_config_t config = { 0 };
    int ret = 0;

    response = util_common_calloc_s(sizeof(struct isula_history_response));
    if (response == NULL) {
        ERROR("Out of memory");
        return ECOMMON;
    }

    request.image_name = args->image_name;
    // Support type oci only currently.
    request.type = "oci";
    
    ops = get_connect_client_ops();
    if (ops == NULL || ops->image.history == NULL) {
        ERROR("Unimplemented ops");
        ret = ECOMMON;
        goto out;
    }

    config = get_connect_config(args);
    ret = ops->image.history(&request, response, &config);
    if (ret != 0) {
        client_print_error(response->cc, response->server_errono, response->errmsg);
        ret = ESERVERERROR;
        goto out;
    }
    
    history_info_print(response);

out:
    isula_history_response_free(response);
    return ret;
}

int cmd_history_main(int argc, const char **argv)
{
    int ret = 0;
    struct isula_libutils_log_config lconf = { 0 };
    int exit_code = ECOMMON; 
    command_t cmd;
    struct command_option options[] = { LOG_OPTIONS(lconf) HISTORY_OPTIONS(g_cmd_history_args),
               COMMON_OPTIONS(g_cmd_history_args)
    };
    if (client_arguments_init(&g_cmd_history_args)) {
        COMMAND_ERROR("client arguments init failed");
        exit(ECOMMON);
    }
    g_cmd_history_args.progname = argv[0];

    isula_libutils_default_log_config(argv[0], &lconf);
    command_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_history_desc,
                 g_cmd_history_usage);
    if (command_parse_args(&cmd, &g_cmd_history_args.argc, &g_cmd_history_args.argv)) {
        exit(exit_code);
    }

    if (isula_libutils_log_enable(&lconf)) {
        COMMAND_ERROR("history: log init failed");
        exit(exit_code);
    }

    if (g_cmd_history_args.argc != 1) {
        COMMAND_ERROR("history requires 1 argument.");
        exit(exit_code);
    }

    g_cmd_history_args.image_name = g_cmd_history_args.argv[0];
    ret = client_history(&g_cmd_history_args);
    exit(ret != 0 ? exit_code : EXIT_SUCCESS); 
}