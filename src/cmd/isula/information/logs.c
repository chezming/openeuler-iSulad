/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: lifeng
 * Create: 2018-11-08
 * Description: provide container logs functions
 ******************************************************************************/
#define _GNU_SOURCE /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "logs.h"
#include "client_arguments.h"
#include "isula_libutils/log.h"
#include "isula_connect.h"
#include "connect.h"
#include "libisula.h"
#include "utils.h"
#include "utils_convert.h"
#include "utils_regex.h"
#include "utils_timestamp.h"
#include "utils_string.h"

const char g_cmd_logs_desc[] = "Fetch the logs of a container";
const char g_cmd_logs_usage[] = "logs [OPTIONS] CONTAINER";
// The maximum size of time
const size_t tb_maxsize = 64;
// support 32-bits system
// The maximum value of seconds 
const int64_t secsmax = 9223372036;
// The maximum value of minutes 
const int64_t minsmax = 153722867;
// The maximum value of hours 
const int64_t hoursmax = 2562047;

struct client_arguments g_cmd_logs_args = {
    .follow = false,
    .tail = -1,
    .timestamps = false,
};

static int do_logs(const struct client_arguments *args)
{
#define DISABLE_ERR_MESSAGE "disable console log"
    isula_connect_ops *ops = NULL;
    struct isula_logs_request *request = NULL;
    struct isula_logs_response *response = NULL;
    client_connect_config_t config = { 0 };
    int ret = 0;

    response = util_common_calloc_s(sizeof(struct isula_logs_response));
    if (response == NULL) {
        ERROR("Log: Out of memory");
        return -1;
    }
    request = util_common_calloc_s(sizeof(struct isula_logs_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    ops = get_connect_client_ops();
    if (ops == NULL || ops->container.logs == NULL) {
        ERROR("Unimplemented logs op");
        ret = -1;
        goto out;
    }

    request->id = util_strdup_s(args->name);
    request->runtime = util_strdup_s(args->runtime);
    request->follow = args->follow;
    request->tail = (int64_t)args->tail;
    request->timestamps = args->timestamps;
    request->since = args->since;
    config = get_connect_config(args);
    ret = ops->container.logs(request, response, &config);
    if (ret != 0) {
        if (strncmp(response->errmsg, DISABLE_ERR_MESSAGE, strlen(DISABLE_ERR_MESSAGE)) == 0) {
            fprintf(stdout, "[WARNING]: Container %s disable console log!\n", args->name);
            ret = 0;
            goto out;
        }
        client_print_error(response->cc, response->server_errono, response->errmsg);
        ret = -1;
        goto out;
    }

out:
    isula_logs_response_free(response);
    isula_logs_request_free(request);
    return ret;
}

int callback_tail(command_option_t *option, const char *arg)
{
    if (util_safe_llong(arg, option->data)) {
        *(long long *)option->data = -1;
    }
    return 0;
}

static char *secs2utc(time_t seconds, int32_t nanos)
{
    char tb[tb_maxsize + 1];
    struct tm tm_utc = { 0 };
    int nret = 0;

    if (gmtime_r(&seconds, &tm_utc) == NULL) {
        ERROR("fail to get UTC time");
        return NULL;
    }
    if (strftime(tb, tb_maxsize, "%Y-%m-%dT%H:%M:%S", &tm_utc) == 0) {
        ERROR("failt to convert timestamps format");
        return NULL;
    }

    // add nano seconds to the end
    nret = snprintf(tb + strlen(tb), tb_maxsize - strlen(tb), ".%09dZ", nanos);
    if (nret < 0 || nret >= tb_maxsize - strlen(tb)) {
        ERROR("sprintf timebuffer failed");
        return NULL;
    }
    return util_strdup_s(tb);
}

static bool compute_time(char ch, int64_t v, int64_t *secs)
{
    switch (ch) {
        case 'h':
            // The total number of backtracking hours cannot exceed 2562047 seconds
            if (v > hoursmax) {
                return false;
            }
            *secs += v * 3600;
            break;
        case 'm':
            // The total number of backtracking minutes cannot exceed 153722867 seconds
            if (v > minsmax) {
                return false;
            }
            *secs += v * 60;
            break;
        case 's':
            // The total number of backtracking seconds cannot exceed 9223372036 seconds
            if (v > secsmax) {
                return false;
            }
            *secs += v;
            break;
        default:
            return false;
    }
    // The total number of backtracking seconds cannot exceed 9223372036 seconds
    if (*secs > secsmax) {
        return false;
    }
    return true;
}

static int check_relative_timestamps_is_valid(char **since, int len)
{
    // The format can only be (digits)[hms], e.g. 1h, 2m, 3h2m, 4s3m5h, 4s5s
    int64_t secs = 0;
    int64_t tmp = 0;
    char *utc = NULL;
    int i = 0;

    // Check and calculate the time
    for (; i < len; ++i) {
        if (!isdigit((*since)[i])) {
            // check value and alpha
            if (!compute_time((*since)[i], tmp, &secs)) {
                return -1;
            } else {
                tmp = 0;
            }
        } else {
            tmp *= 10;
            // The total number of backtracking seconds cannot exceed 9223372036 seconds
            if (tmp > secsmax - ((*since)[i] - '0')) {
                return -1;
            }
            tmp += (*since)[i] - '0';
        }
    }
    // the relative mode must end with three letters h, m, s
    if (tmp != 0) {
        return -1;
    }

    // Convert seconds to UTC time
    utc = secs2utc(time(NULL) - (time_t)secs, 0);
    if (utc == NULL) {
        ERROR("Log since : The relative mode cannot convert the time to UTC format");
        return -1;
    }
    free((*since));
    *since = util_strdup_s(utc);
    free(utc);
    return 0;
}

static int check_timestamps_is_valid(char **since, const char *str)
{
    int ret = 0;
    types_timestamp_t *timestamp = NULL;
    char *timebuffer = NULL;

    timestamp = util_common_calloc_s(sizeof(types_timestamp_t));
    if (timestamp == NULL) {
        ERROR("Out of memory");
        ret = -1;
        return ret;
    }

    // convert local time to seconds
    if (!get_timestamp(str, timestamp)) {
        COMMAND_ERROR("Failed to get since timestamp");
        ret = -1;
        goto out;
    }
    // convert seconds to utc time with nanos (default nanos = 0)
    if (timestamp->has_nanos) {
        timebuffer = secs2utc(timestamp->seconds, timestamp->nanos);
    } else {
        timebuffer = secs2utc(timestamp->seconds, 0);
    }
    if (timebuffer == NULL) {
        COMMAND_ERROR("Failed to get since timestamp");
    }

    // pass value to since
    free((*since));
    (*since) = util_strdup_s(timebuffer);

out:
    free(timestamp);
    return ret;
}

int callback_since(command_option_t *option, const char *arg)
{
    bool relative = true;
    int ret = 0;

    // relative mode or timestamps mode
    if (strings_contains_any(arg, "T")) {
        relative = false;
    }

    // different ways to check relative time and timestamp
    if (relative) {
        ret = check_relative_timestamps_is_valid((char **)(option->data), strlen(arg));
        if (ret == -1) {
            COMMAND_ERROR("invalid value for \"since\": faild to parse value as time or duration: \"%s\"", arg);
        }
    } else {
        ret = check_timestamps_is_valid((char **)(option->data), arg);
    }
    return ret;
}

static int cmd_logs_init(int argc, const char **argv)
{
    struct isula_libutils_log_config lconf = { 0 };
    command_t cmd;

    if (client_arguments_init(&g_cmd_logs_args)) {
        COMMAND_ERROR("client arguments init failed\n");
        return ECOMMON;
    }
    g_cmd_logs_args.progname = argv[0];
    struct command_option options[] = { LOG_OPTIONS(lconf) LOGS_OPTIONS(g_cmd_logs_args)
                                                COMMON_OPTIONS(g_cmd_logs_args) };

    command_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_logs_desc,
                 g_cmd_logs_usage);
    if (command_parse_args(&cmd, &g_cmd_logs_args.argc, &g_cmd_logs_args.argv)) {
        return EINVALIDARGS;
    }
    isula_libutils_default_log_config(argv[0], &lconf);
    if (isula_libutils_log_enable(&lconf)) {
        COMMAND_ERROR("log init failed\n");
        g_cmd_logs_args.name = g_cmd_logs_args.argv[0];
        return ECOMMON;
    }

    if (g_cmd_logs_args.argc != 1) {
        COMMAND_ERROR("Logs needs one container name");
        return ECOMMON;
    }

    return 0;
}

int cmd_logs_main(int argc, const char **argv)
{
    int ret = 0;

    ret = cmd_logs_init(argc, argv);
    if (ret != 0) {
        exit(ret);
    }

    g_cmd_logs_args.name = g_cmd_logs_args.argv[0];
    ret = do_logs(&g_cmd_logs_args);
    if (ret != 0) {
        exit(ECOMMON);
    }
    return 0;
}
