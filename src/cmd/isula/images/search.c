/******************************************************************************
 * Author: xiangli
 * Create: 2022-6-11
 * Description: provide search image
 ********************************************************************************/
#include "search.h"

#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "utils_cjson.h"
#include "client_arguments.h"
#include "isula_connect.h"
#include "isula_libutils/log.h"
#include "command_parser.h"
#include "connect.h"

const char g_cmd_search_desc[] = "Search an image or a repository from a registry";
const char g_cmd_search_usage[] = "search [OPTIONS] NAME";

struct client_arguments g_cmd_search_args = {};

void search_parse_print_json(const char *json)
{
    utils_cjson *utils_cjson_json = NULL;
    utils_cjson *utils_cjson_name = NULL;
    utils_cjson *utils_cjson_tags = NULL;
    utils_cjson *utils_cjson_tags_item = NULL;
    int tags_len = 0;
    lengths max_len = {
        .name_length = 30,
        .tag_length = 10,
    };

    utils_cjson_json = utils_cjson_Parse(json);
    utils_cjson_name = utils_cjson_GetObjectItem(utils_cjson_json, "name");
    utils_cjson_tags = utils_cjson_GetObjectItem(utils_cjson_json, "tags");
    tags_len = utils_cjson_GetArraySize(utils_cjson_tags);

    if (tags_len == 0) {
        COMMAND_ERROR("Search error: No such object:%s", utils_cjson_name->valuestring);
        return;
    }

    /* print header */
    printf("%-*s ", (int)max_len.name_length, "NAME");
    printf("%-*s ", (int)max_len.tag_length, "TAG");
    printf("\n");

    for (int j = 0; j < tags_len; j++) {
        utils_cjson_tags_item = utils_cjson_GetArrayItem(utils_cjson_tags, j);
        printf("%-*s ", (int)max_len.name_length, utils_cjson_name->valuestring);
        printf("%-*s ", (int)max_len.tag_length, utils_cjson_tags_item->valuestring);
        printf("\n");
    }

    free(utils_cjson_json);
    free(utils_cjson_name);
    free(utils_cjson_tags);
    free(utils_cjson_tags_item);
}


/*
 * Search an image or a repository from a registry
 */
int client_search(const struct client_arguments *args)
{
    isula_connect_ops *ops = NULL;
    struct isula_search_request request = { 0 };
    struct isula_search_response *response = NULL;
    client_connect_config_t config = { 0 };
    int ret = 0;

    response = util_common_calloc_s(sizeof(struct isula_search_response));
    if (response == NULL) {
        ERROR("Out of memory");
        return ECOMMON;
    }

    request.image_name = args->image_name;

    ops = get_connect_client_ops();
    if (ops == NULL || ops->image.search == NULL) {
        ERROR("Unimplemented ops");
        ret = ECOMMON;
        goto out;
    }
    COMMAND_ERROR("Image \"%s\" searching", request.image_name);

    config = get_connect_config(args);
    ret = ops->image.search(&request, response, &config);
    if (ret != 0) {
        client_print_error(response->cc, response->server_errono, response->errmsg);
        ret = ESERVERERROR;
        goto out;
    }
    search_parse_print_json(response->image_tags_json);
    COMMAND_ERROR("Image \"%s\" searched", args->image_name);

out:
    isula_search_response_free(response);
    return ret;
}

int cmd_search_main(int argc, const char **argv)
{
    int ret = 0;
    struct isula_libutils_log_config lconf = { 0 };
    int exit_code = 1; /* exit 1 if failed to search */
    command_t cmd;
    struct command_option options[] = { COMMON_OPTIONS(g_cmd_search_args) };

    if (client_arguments_init(&g_cmd_search_args)) {
        COMMAND_ERROR("client arguments init failed");
        exit(ECOMMON);
    }
    g_cmd_search_args.progname = argv[0];

    isula_libutils_default_log_config(argv[0], &lconf);
    command_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_search_desc,
                 g_cmd_search_usage);
    if (command_parse_args(&cmd, &g_cmd_search_args.argc, &g_cmd_search_args.argv)) {
        exit(exit_code);
    }

    if (isula_libutils_log_enable(&lconf)) {
        COMMAND_ERROR("Search: log init failed");
        exit(exit_code);
    }

    if (g_cmd_search_args.argc != 1) {
        COMMAND_ERROR("search requires 1 argument.");
        exit(exit_code);
    }

    g_cmd_search_args.image_name = g_cmd_search_args.argv[0];
    ret = client_search(&g_cmd_search_args);
    if (ret != 0) {
        exit(exit_code);
    }

    exit(EXIT_SUCCESS);
}
