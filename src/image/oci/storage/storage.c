/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: lifeng
 * Create: 2020-04-01
 * Description: provide storage functions
 ******************************************************************************/
#include "storage.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "log.h"

static ssize_t layer_archive_io_read(void *context, void *buf, size_t buf_len)
{
    int *read_fd = (int *)context;

    return util_read_nointr(*read_fd, buf, buf_len);
}

static int layer_archive_io_close(void *context, char **err)
{
    int *read_fd = (int *)context;

    close(*read_fd);

    free(read_fd);

    return 0;
}

static int fill_read_wrapper(const char *layer_data_path, struct io_read_wrapper *reader)
{
    int ret = 0;
    int *fd_ptr = NULL;

    fd_ptr = util_common_calloc_s(sizeof(int));
    if (fd_ptr == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto out;
    }

    *fd_ptr = util_open(layer_data_path, O_RDONLY, 0);
    if (*fd_ptr == -1) {
        ERROR("Failed to open layer data %s", layer_data_path);
        ret = -1;
        goto err_out;
    }

    reader->context = fd_ptr;
    reader->read = layer_archive_io_read;
    reader->close = layer_archive_io_close;

    goto out;

err_out:
    free(fd_ptr);
out:
    return ret;
}

static struct layer_opts *fill_create_layer_opts(const char *parent_id, bool writeable)
{
    struct layer_opts *opts = NULL;

    opts = util_common_calloc_s(sizeof(struct layer_opts));
    if (opts == NULL) {
        ERROR("Memory out");
        goto out;
    }

    opts->parent = util_strdup_s(parent_id);
    opts->writable = writeable;

out:
    return opts;
}

int storage_layer_create(const char *layer_id, const char *parent_id, bool writeable, const char *layer_data_path)
{
    int ret = 0;
    struct io_read_wrapper reader = { 0 };
    struct layer_opts *opts = NULL;

    if (layer_id == NULL || parent_id == NULL || layer_data_path == NULL) {
        ERROR("Invalid arguments for put ro layer");
        ret = -1;
        goto out;
    }

    if (fill_read_wrapper(layer_data_path, &reader) != 0) {
        ERROR("Failed to fill layer read wrapper");
        ret = -1;
        goto out;
    }

    opts = fill_create_layer_opts(parent_id, writeable);
    if (opts == NULL) {
        ERROR("Failed to fill create ro layer options");
        ret = -1;
        goto out;
    }

    ret = layer_store_create(layer_id, opts, &reader, NULL);
    if (ret != 0) {
        ERROR("Failed to call layer store create");
        ret = -1;
        goto out;
    }

out:
    if (reader.close != NULL) {
        reader.close(reader.context, NULL);
    }
    free_layer_opts(opts);
    return ret;
}

struct layer *storage_layer_get(const char *id)
{
    // TODO call layer_store functions to get layer info
    return NULL;
}

int storage_layer_try_repair_lowers(const char *id, const char *last_layer_id)
{
    // TODO layer_store_try_repair_lowers
    int ret = 0;

    return ret;
}

int storage_img_create(const char *id, const char *parent_id, const char *metadata,
                       struct storage_img_create_options *opts)
{
    int ret = 0;

    return ret;
}

storage_image *storage_img_get(const char *img_id)
{
    if (img_id == NULL) {
        ERROR("Invalid arguments for image get");
        return NULL;
    }

    return image_store_lookup(img_id);
}

int storage_img_set_big_data(const char *img_id, const char *key, const char *val)
{
    int ret = 0;

    if (img_id == NULL || key == NULL || val == NULL) {
        ERROR("Invalid arguments");
        ret = -1;
        goto out;
    }

    if (image_store_set_big_data(img_id, key, val) != 0) {
        ERROR("Failed to set img %s big data %s=%s", img_id, key, val);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

int storage_img_add_name(const char *img_id, const char *img_name)
{
    int ret = 0;

    if (img_id == NULL || img_name == NULL) {
        ERROR("Invalid arguments");
        ret = -1;
        goto out;
    }

    if (image_store_add_name(img_id, img_name) != 0) {
        ERROR("Failed to add img %s name %s", img_id, img_name);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

int storage_img_delete(const char *img_id, bool commit)
{
    int ret = 0;

    if (image_store_delete(img_id) != 0) {
        ERROR("Failed to delete img %s", img_id);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

int storage_img_set_meta_data(const char *img_id, const char *meta)
{
    int ret = 0;

    if (img_id == NULL || meta == NULL) {
        ERROR("Invalid arguments");
        ret = -1;
        goto out;
    }

    if (image_store_set_metadata(img_id, meta) != 0) {
        ERROR("Failed to set img %s meta data", img_id);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

int storage_img_set_loaded_time(const char *img_id, types_timestamp_t *loaded_time)
{
    int ret = 0;

    if (img_id == NULL || loaded_time == NULL) {
        ERROR("Invalid arguments");
        ret = -1;
        goto out;
    }

    if (image_store_set_load_time(img_id, loaded_time) != 0) {
        ERROR("Failed to set img %s loaded time", img_id);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

