/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
* iSulad licensed under the Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*     http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
* PURPOSE.
* See the Mulan PSL v2 for more details.
* Author: huangsong
* Create: 2022-10-08
* Description: isula history operator implement
*******************************************************************************/
#include "oci_history.h"

#include <stdbool.h>
#include <string.h>
#include <isula_libutils/log.h>

#include "utils.h"
#include "utils_array.h"
#include "utils_string.h"
#include "utils_verify.h"
#include "layer_store.h"
#include "storage.h"
#include "err_msg.h"
#include "oci_image.h"
#include "sha256.h"


static int get_layer_size(imagetool_image *image_info, size_t *layer_size, size_t layer_counter)
{
    struct layer *layer_info = NULL;
    char *digest = NULL;
    char *layer_id = NULL;
    if (image_info->spec->rootfs->diff_ids_len <= layer_counter){
        ERROR("Too many non-empty layers in History section");
        return -1;
    }
    digest = image_info->spec->rootfs->diff_ids[layer_counter];
    layer_id = util_without_sha256_prefix(digest);
    layer_info = layer_store_lookup(layer_id);
    if (layer_info == NULL) {
        ERROR("Failed to get layer info for layer %s", layer_id);
        return -1;
    }

    if (layer_info->uncompress_size < 0 || layer_info->uncompressed_digest == NULL) {
        ERROR("size for layer %s unknown", layer_id);
        return -1;
    }
    *layer_size = layer_info->uncompress_size;
    return 0;
}

int oci_do_history(const im_history_request *request, im_history_response *response)
{
    int ret = 0;
    char *real_image_name = NULL;
    imagetool_image *image_info = NULL;
    int i;
    size_t layer_counter = 0;
    struct layer *layer_info = NULL;
    image_history_info_t *res = NULL;
    
    if (request == NULL || request->image.image == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    if (!util_valid_image_name(request->image.image)) {
        ERROR("Invalid image name: %s", request->image.image);
        isulad_try_set_error_message("Invalid image name: %s", request->image.image);
        return -1;
    }

    real_image_name = oci_resolve_image_name(request->image.image);
    if (real_image_name == NULL) {
        ERROR("Failed to resolve image name");
        return -1;
    }

   image_info = storage_img_get(real_image_name);
    if (image_info == NULL) {
        ERROR("No such image:%s", real_image_name);
        isulad_set_error_message("No such image:%s", real_image_name);
        ret = -1;
        goto out;
    }
    
    response->history_info = util_smart_calloc_s(sizeof(image_history_info_t *), image_info->spec->history_len + 1);
    if (response->history_info == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    for (i = image_info->spec->history_len -1; i >= 0 ; i--) {
        size_t layer_size = 0;
        if (!image_info->spec->history[i]->empty_layer) {
            ret = get_layer_size(image_info, &layer_size, layer_counter);
            if (ret == -1) {
                goto out;
            }
            layer_counter++;
        }

        res = util_common_calloc_s(sizeof(image_history_info_t));
        if (res == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }

        res->comment = util_strdup_s(image_info->spec->history[i]->comment);
        res->created = util_strdup_s(image_info->spec->history[i]->created);
        res->created_by = util_strdup_s(image_info->spec->history[i]->created_by);
        res->id = util_strdup_s("<missing>");
        res->size = layer_size;
        response->history_info[response->history_info_len] = res;
        response->history_info_len += 1;
    }

    response->history_info[0]->id = util_strdup_s(image_info->id);
    

out: 
    free(real_image_name);
    free_layer(layer_info);
    free_imagetool_image(image_info);
    return ret;
}
