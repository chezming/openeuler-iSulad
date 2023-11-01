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
* Create: 2020-05-07
* Description: isula image pull operator implement
*******************************************************************************/
#include "oci_pull.h"

#include <isula_libutils/image_progress.h>
#include <isula_libutils/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "err_msg.h"
#include "map.h"
#include "oci_image.h"
#include "progress.h"
#include "registry.h"
#include "storage.h"
#include "utils.h"
#include "utils_array.h"
#include "utils_base64.h"
#include "utils_images.h"
#include "utils_string.h"

static int decode_auth(const char *auth, char **username, char **password)
{
    int nret = 0;
    int ret = 0;
    unsigned char *decoded = NULL;
    size_t decoded_len = 0;
    char **auth_parts = NULL;

    if (auth == NULL || username == NULL || password == NULL) {
        ERROR("invalid NULL pointer");
        return -1;
    }

    nret = util_base64_decode(auth, strlen(auth), &decoded, &decoded_len);
    if (nret < 0) {
        ERROR("decode auth from base64 failed");
        ret = -1;
        goto out;
    }

    auth_parts = util_string_split((char *)decoded, ':');
    if (auth_parts == NULL || util_array_len((const char **)auth_parts) != 2) {
        ERROR("Invalid auth format");
        ret = -1;
        goto out;
    }

    *username = util_strdup_s(auth_parts[0]);
    *password = util_strdup_s(auth_parts[1]);
    (void)memset(auth_parts[0], 0, strlen(auth_parts[0]));
    (void)memset(auth_parts[1], 0, strlen(auth_parts[1]));

out:
    util_free_sensitive_string((char *)decoded);
    decoded = NULL;
    util_free_array(auth_parts);
    auth_parts = NULL;

    return ret;
}

static void update_option_insecure_registry(registry_pull_options *options, char **insecure_registries, const char *host)
{
    char **registry = NULL;

    if (insecure_registries == NULL || options == NULL || host == NULL) {
        return;
    }

    for (registry = insecure_registries; (registry != NULL) && (*registry != NULL); registry++) {
        if (!strcmp(*registry, host)) {
            options->insecure_registry = true;
        }
    }
}

static int pull_image(const im_pull_request *request, progress_status_map *progress_status_store, char **name)
{
    int ret = -1;
    registry_pull_options *options = NULL;
    char **insecure_registries = NULL;
    char **registry_mirrors = NULL;
    char **mirror = NULL;
    char *host = NULL;
    char *with_tag = NULL;
    struct oci_image_module_data *oci_image_data = NULL;

    options = (registry_pull_options *)util_common_calloc_s(sizeof(registry_pull_options));
    if (options == NULL) {
        ERROR("Out of memory");
        goto out;
    }

    if (request->auth != NULL) {
        ret = decode_auth(request->auth, &options->auth.username, &options->auth.password);
        if (ret != 0) {
            ERROR("Decode auth failed");
            goto out;
        }
    } else {
        options->auth.username = util_strdup_s(request->username);
        options->auth.password = util_strdup_s(request->password);
    }
    options->progress_status_store = progress_status_store;

    oci_image_data = get_oci_image_data();
    options->skip_tls_verify = oci_image_data->insecure_skip_verify_enforce;
    insecure_registries = oci_image_data->insecure_registries;

    // key of image which save in image-store
    options->dest_image_name = oci_normalize_image_name(request->image);

    // add default tag if required
    with_tag = oci_default_tag(request->image);

    host = oci_get_host(request->image);
    if (host != NULL) {
        // 1. image_name use for split host/tag/name
        // 2. user for tag of log
        options->image_name = with_tag;
        with_tag = NULL;

        update_option_insecure_registry(options, insecure_registries, host);
        ret = registry_pull(options);
        if (ret != 0) {
            ERROR("pull image failed");
            goto out;
        }
    } else {
        registry_mirrors = oci_image_data->registry_mirrors;
        if (registry_mirrors == NULL) {
            ERROR("Invalid image name %s, no host found", request->image);
            isulad_try_set_error_message("Invalid image name, no host found");
            goto out;
        }

        for (mirror = registry_mirrors; (mirror != NULL) && (*mirror != NULL); mirror++) {
            if (util_has_prefix(*mirror, HTTP_PREFIX)) {
                options->insecure_registry = true;
            }
            host = oci_host_from_mirror(*mirror);
            update_option_insecure_registry(options, insecure_registries, host);
            // add current mirror to image name
            free(options->image_name);
            options->image_name = oci_add_host(host, with_tag);
            free(host);
            host = NULL;

            ret = registry_pull(options);
            if (ret != 0) {
                continue;
            }
            break;
        }
    }

    *name = util_strdup_s(options->dest_image_name);

out:
    free(with_tag);
    free(host);
    free_registry_pull_options(options);

    return ret;
}

typedef struct status_arg {
    progress_status_map *status_store;
    bool should_terminal;
    imagetool_image_summary *image;
    stream_func_wrapper *stream;
} status_arg;

void *get_progress_status(void *arg)
{
    status_arg *status = (status_arg *)arg;
    const int delay = 100; // Sleep for 100 milliseconds
    bool write_ok = false;

    if (status == NULL || status->status_store == NULL || status->stream == NULL) {
        ERROR("Get progress status condition error");
        return NULL;
    }

    for (;;) {
        int i = 0;
        
        usleep(delay * 1000);  // Sleep for 100 milliseconds

        if (status->should_terminal && status->image == NULL) {
            break;
        }
        
        image_progress *progresses;
        size_t progress_size = progress_status_map_size(status->status_store);

        progresses = util_common_calloc_s(sizeof(image_progress));
        if (progresses == NULL) {
            ERROR("Out of memory. Skip progress show.");
            break;   
        }

        progresses->progresses = util_smart_calloc_s(sizeof(image_progress_progresses_element *), progress_size);
        if (progresses->progresses == NULL) {
            ERROR("Out of memory. Skip progress show.");
            goto error;
        }
        if (status->image != NULL) {
            progresses->image = util_strdup_s(status->image->id);
            status->image = NULL;
        }

        if (!progress_status_map_lock(status->status_store)) {
            ERROR("Cannot itorate progress status map for locking failed");
            goto error;
        }
        map_itor *itor = map_itor_new(status->status_store->map); 
        for (i = 0; map_itor_valid(itor) && i < progress_size; map_itor_next(itor), i++) {
            void *id = map_itor_key(itor);
            const progress *value = (progress *)map_itor_value(itor);
            const int ID_LEN = 12; // The last 12 charactos of image digest.

            progresses->progresses[i] = util_common_calloc_s(sizeof(image_progress_progresses_element));
            if (progresses->progresses[i] == NULL) {
                WARN("Out of memory. Skip progress show.");
                map_itor_free(itor);
                progress_status_map_unlock(status->status_store);
                goto error;
            }
            progresses->progresses[i]->id = util_strdup_s((char *)id + strlen((char *)id) - ID_LEN);
            progresses->progresses[i]->total = value->dltotal;
            progresses->progresses[i]->current = value->dlnow;
            progresses->progresses_len++;
        }
        map_itor_free(itor);
        progress_status_map_unlock(status->status_store);
    
        /* send to client */
        write_ok = status->stream->write_func(status->stream->writer, progresses);
        if (write_ok) {
            continue;
        } 
        if(status->stream->is_cancelled(status->stream->context)) {
            ERROR("pull stream is cancelled");
            goto error;
        }    
        ERROR("Send progress data to client failed");
error:
        free_image_progress(progresses);
    }
    return NULL;
}

int oci_do_pull_image(const im_pull_request *request, stream_func_wrapper *stream, im_pull_response *response)
{
    int ret = 0;
    imagetool_image_summary *image = NULL;
    imagetool_image_summary *image2 = NULL;
    char *dest_image_name = NULL;
    progress_status_map *progress_status_store = NULL;

    if (request == NULL || request->image == NULL || response == NULL) {
        ERROR("Invalid NULL param");
        return -1;
    }

    pthread_t tid = 0;
    status_arg arg = {0};
    if (request->is_progress_visible) {
        progress_status_store = progress_status_map_new();
        if (progress_status_store == NULL) {
            ERROR("Out of memory and will not show the pull progress");
            isulad_set_error_message("Failed to pull image %s with error: out of memory", request->image);
            ret = -1;
            goto out;
        }
        arg.should_terminal = false;
        arg.status_store = progress_status_store;
        arg.stream = stream;
        if (pthread_create(&tid, NULL, get_progress_status, (void *)&arg) != 0) {
            ERROR("Failed to start thread to get progress status");
            isulad_set_error_message("Failed to pull image %s with error: start progress thread error", request->image);
            ret = -1;
            goto out;
        }
    }

    ret = pull_image(request, progress_status_store, &dest_image_name);
    if (ret != 0) {
        ERROR("Pull image %s failed", request->image);
        isulad_set_error_message("Failed to pull image %s with error: %s", request->image, g_isulad_errmsg);
        ret = -1;
        goto out;
    }

    image = storage_img_get_summary(dest_image_name);
    image2 = storage_img_get_summary(request->image);
    if (image == NULL || image2 == NULL) {
        ERROR("Get image %s failed after pulling", request->image);
        isulad_set_error_message("Failed to pull image %s with error: image not found after pulling", request->image);
        ret = -1;
        goto out;
    }
    arg.image = image;

out:
    arg.should_terminal = true;
    if (tid != 0 && pthread_join(tid, NULL) != 0) {
        ERROR("Wait child pthread error");
    }
    free_imagetool_image_summary(image);
    free_imagetool_image_summary(image2);
    free(dest_image_name);
    progress_status_map_free(progress_status_store);
    return ret;
}
