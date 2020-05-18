/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
* iSulad licensed under the Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*     http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
* PURPOSE.
* See the Mulan PSL v2 for more details.
* Author: gaohuatao
* Create: 2020-05-14
* Description: isula load operator implement
*******************************************************************************/
#include "oci_load.h"

#include "utils.h"
#include "log.h"
#include "util_archive.h"
#include "storage.h"
#include "sha256.h"
#include "mediatype.h"


#define MANIFEST_BIG_DATA_KEY "manifest"
#define OCI_SCHEMA_VERSION 2

static image_manifest_items_element **load_manifest(const char *fname, size_t *length)
{
    image_manifest_items_element **manifest = NULL;
    parser_error err = NULL;
    size_t len;

    if (fname == NULL || length == NULL) {
        return NULL;
    }

    manifest = image_manifest_items_parse_file(fname, NULL, &err, &len);
    if (manifest == NULL) {
        len = 0;
        ERROR("Parse manifest %s err:%s", fname, err);
    }

    *length = len;
    free(err);
    return manifest;
}

static oci_image_spec *load_image_config(const char *fname)
{
    oci_image_spec *config = NULL;
    parser_error err = NULL;

    if (fname == NULL) {
        return NULL;
    }

    config = oci_image_spec_parse_file(fname, NULL, &err);
    if (config == NULL) {
        ERROR("Parse image config file %s err:%s", fname, err);
    }

    free(err);
    return config;
}

static ssize_t image_archive_io_read(void *context, void *buf, size_t buf_len)
{
    int *read_fd = (int *)context;

    return util_read_nointr(*read_fd, buf, buf_len);
}

static int image_archive_io_close(void *context, char **err)
{
    int *read_fd = (int *)context;

    close(*read_fd);
    free(read_fd);
    return 0;
}

static int file_read_wrapper(const char *image_data_path, struct io_read_wrapper *reader)
{
    int ret = 0;
    int *fd_ptr = NULL;

    fd_ptr = util_common_calloc_s(sizeof(int));
    if (fd_ptr == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto out;
    }

    *fd_ptr = util_open(image_data_path, O_RDONLY, 0);
    if (*fd_ptr == -1) {
        ERROR("Failed to open layer data %s", image_data_path);
        ret = -1;
        goto err_out;
    }

    reader->context = fd_ptr;
    reader->read = image_archive_io_read;
    reader->close = image_archive_io_close;

    goto out;

err_out:
    free(fd_ptr);
out:
    return ret;
}

static void free_blob_layer(layer_blob *l)
{
    if (l == NULL) {
        return;
    }

    if (l->chain_id != NULL) {
        free(l->chain_id);
        l->chain_id = NULL;
    }

    if (l->diff_id != NULL) {
        free(l->diff_id);
        l->diff_id = NULL;
    }

    if (l->fpath != NULL) {
        free(l->fpath);
        l->fpath = NULL;
    }
    free(l);
}

static void free_image(image_t *im)
{
    int i = 0;

    if (im == NULL) {
        return;
    }

    if (im->config_fpath != NULL) {
        free(im->config_fpath);
        im->config_fpath = NULL;
    }

    if (im->im_id != NULL) {
        free(im->im_id);
        im->im_id = NULL;
    }

    if (im->im_digest != NULL) {
        free(im->im_digest);
        im->im_digest = NULL;
    }

    if (im->manifest_fpath != NULL) {
        free(im->manifest_fpath);
        im->manifest_fpath = NULL;
    }

    if (im->manifest_digest != NULL) {
        free(im->manifest_digest);
        im->manifest_digest = NULL;
    }

    util_free_array_by_len(im->repo_tags, im->repo_tags_len);

    for (; i < im->layers_len; i++) {
        free_blob_layer(im->layers[i]);
    }
    free(im->layers);
    im->layers = NULL;
    im->layers_len = 0;

    free_oci_image_manifest(im->manifest);

    free(im);
}

static char **str_array_copy(char **arr, size_t len)
{
    char **str_arr = NULL;
    size_t i = 0;

    str_arr = util_common_calloc_s(sizeof(char *) * len);
    if (str_arr == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    for (; i < len; i++) {
        str_arr[i] = util_strdup_s(arr[i]);
    }

    return str_arr;
}

static types_timestamp_t created_to_timestamp(char *created)
{
    int64_t nanos = 0;
    types_timestamp_t timestamp = {0};

    if (to_unix_nanos_from_str(created, &nanos) != 0) {
        ERROR("Failed to get created time from image config");
        goto out;
    }

    timestamp.has_seconds = true;
    timestamp.seconds = nanos / Time_Second;
    timestamp.has_nanos = true;
    timestamp.nanos = nanos % Time_Second;

out:

    return timestamp;
}

static char *calc_diffid(char *file)
{
    int ret = 0;
    char *diff_id = NULL;
    bool gzip = false;

    if (file == NULL) {
        ERROR("Invalid NULL param");
        return NULL;
    }

    ret = util_gzip_compressed(file, &gzip);
    if (ret != 0) {
        gzip = true;
    }

    if (gzip) {
        diff_id = util_full_gzip_digest(file);
    } else {
        diff_id = util_full_file_digest(file);
    }
    if (diff_id == NULL) {
        ERROR("calculate digest failed for file %s", file);
        ret = -1;
        goto out;
    }

out:
    if (ret != 0) {
        free(diff_id);
        diff_id = NULL;
    }

    return diff_id;
}

static char *calc_chain_id(char *parent_chain_id, char *diff_id)
{
    int sret = 0;
    char tmp_buffer[256] = {0};
    char *digest = NULL;
    char *full_digest = NULL;

    if (parent_chain_id == NULL || diff_id == NULL) {
        ERROR("Invalid NULL param");
        return NULL;
    }

    if (strlen(diff_id) <= strlen(SHA256_PREFIX)) {
        ERROR("Invalid diff id %s found when calc chain id", diff_id);
        return NULL;
    }

    if (strlen(parent_chain_id) == 0) {
        return util_strdup_s(diff_id);
    }

    if (strlen(parent_chain_id) <= strlen(SHA256_PREFIX)) {
        ERROR("Invalid parent chain id %s found when calc chain id", parent_chain_id);
        return NULL;
    }

    sret = snprintf(tmp_buffer, sizeof(tmp_buffer), "%s+%s", parent_chain_id + strlen(SHA256_PREFIX),
                    diff_id + strlen(SHA256_PREFIX));
    if (sret < 0 || (size_t)sret >= sizeof(tmp_buffer)) {
        ERROR("Failed to sprintf chain id original string");
        return NULL;
    }

    digest = sha256_digest_str(tmp_buffer);
    if (digest == NULL) {
        ERROR("Failed to calculate chain id");
        goto out;
    }

    full_digest = util_full_digest(digest);

out:
    free(digest);
    return full_digest;
}
static char *without_sha256_prefix(char *digest)
{
    if (digest == NULL) {
        ERROR("Invalid digest NULL when strip sha256 prefix");
        return NULL;
    }

    return digest + strlen(SHA256_PREFIX);
}

static int set_chain_id_to_image(image_t *image)
{
    char *parent_chain_id = "";
    size_t i = 0;

    // caculate chain-id
    for (; i < image->layers_len; i++) {
        image->layers[i]->chain_id = calc_chain_id(parent_chain_id, image->layers[i]->diff_id);
        if (image->layers[i]->chain_id == NULL) {
            ERROR("calc chain id failed, diff id %s, parent chain id %s", image->layers[i]->diff_id, parent_chain_id);
            return -1;
        }
        parent_chain_id = image->layers[i]->chain_id;
    }
    return 0;
}

static int register_layers(image_t *desc)
{
    int ret = 0;
    size_t i = 0;
    struct layer *l = NULL;
    char *id = NULL;
    char *parent = NULL;

    if (desc == NULL) {
        ERROR("Invalid NULL pointer");
        return -1;
    }

    if (desc->layers_len == 0) {
        ERROR("No layer found failed");
        return -1;
    }

    for (; i < desc->layers_len; i++) {
        id = without_sha256_prefix(desc->layers[i]->chain_id);
        if (id == NULL) {
            ERROR("layer %zu have NULL digest for image %s", i, desc->im_id);
            ret = -1;
            goto out;
        }

        l = storage_layer_get(id);
        if (l != NULL) {
            free_layer(l);
            l = NULL;
            ret = storage_layer_try_repair_lowers(id, parent);
            if (ret != 0) {
                ERROR("try to repair lowers for layer %s failed", id);
            }
        } else {
            ret = storage_layer_create(id, parent, desc->layers[i]->diff_id, false, desc->layers[i]->fpath);
        }

        if (ret != 0) {
            ERROR("create layer %s failed, parent %s, file %s", id, parent, desc->layers[i]->fpath);
            goto out;
        }

        parent = id;
    }

out:
    return ret;
}

static int create_image(image_t *desc)
{
    int ret = 0;
    size_t i = 0;
    size_t top_layer_index = 0;
    struct storage_img_create_options opts = {0};
    char *top_layer_id = NULL;
    char *pre_top_layer = NULL;
    oci_image_spec *conf = NULL;
    types_timestamp_t timestamp = {0};

    if (desc == NULL || desc->im_id == NULL) {
        ERROR("Invalid NULL pointer");
        return -1;
    }

    conf = load_image_config(desc->config_fpath);
    if (conf == NULL || conf->created == NULL) {
        ERROR("Get image created time failed");
        ret = -1;
        goto out;
    }

    timestamp = created_to_timestamp(conf->created);
    top_layer_index = desc->layers_len - 1;
    opts.create_time = &timestamp;
    opts.digest = desc->manifest_digest;
    top_layer_id = without_sha256_prefix(desc->layers[top_layer_index]->chain_id);
    if (top_layer_id == NULL) {
        ERROR("NULL top layer id found for image %s", desc->im_id);
        ret = -1;
        goto out;
    }

    ret = storage_img_create(desc->im_id, top_layer_id, NULL, &opts);
    if (ret != 0) {
        pre_top_layer = storage_get_img_top_layer(desc->im_id);
        if (pre_top_layer == NULL) {
            ERROR("create image %s for %s failed", desc->im_id);
            ret = -1;
            goto out;
        }

        if (strcmp(pre_top_layer, top_layer_id) != 0) {
            ERROR("error load image, image id %s exist, but top layer doesn't match. local %s, load %s",
                  desc->im_id, pre_top_layer, top_layer_id);
            ret = -1;
            goto out;
        }
        ret = 0;
    }

    ERROR("start go add image name len:%d", desc->repo_tags_len); // 0

    for (; i < desc->repo_tags_len; i++) {
        ERROR("image name is %s", desc->repo_tags[i]);
        ret = storage_img_add_name(desc->im_id, desc->repo_tags[i]);
        if (ret != 0) {
            ERROR("add image name failed");
            goto out;
        }
    }

out:
    free_oci_image_spec(conf);
    free(pre_top_layer);
    return ret;
}


static int set_manifest(const oci_image_manifest *m, char *image_id)
{
    int ret = 0;
    char *manifest_str = NULL;
    parser_error err = NULL;

    if (m == NULL || image_id == NULL) {
        ERROR("Invalid NULL pointer");
        return -1;
    }

    manifest_str = oci_image_manifest_generate_json(m, NULL, &err);
    if (manifest_str == NULL) {
        ERROR("Generate image %s manifest json err:%s", image_id, err);
        ret = -1;
        goto out;
    }

    ret = storage_img_set_big_data(image_id, MANIFEST_BIG_DATA_KEY, manifest_str);
    if (ret != 0) {
        ERROR("set big data failed");
        goto out;
    }

out:
    free(err);
    free(manifest_str);
    return ret;
}

static int set_config(image_t *desc)
{
    int ret = 0;
    char *config_str = NULL;

    if (desc == NULL) {
        ERROR("Invalid NULL pointer");
        return -1;
    }

    config_str = util_read_text_file(desc->config_fpath);
    if (config_str == NULL) {
        ERROR("read file %s content failed", desc->config_fpath);
        ret = -1;
        goto out;
    }

    ret = storage_img_set_big_data(desc->im_id, desc->im_digest, config_str);
    if (ret != 0) {
        ERROR("set big data failed");
        goto out;
    }

out:

    free(config_str);
    config_str = NULL;

    return ret;
}

static int set_loaded_time(char *image_id)
{
    int ret = 0;
    types_timestamp_t now = {0};

    if (!get_now_time_stamp(&now)) {
        ret = -1;
        ERROR("get now time stamp failed");
        goto out;
    }

    ret = storage_img_set_loaded_time(image_id, &now);
    if (ret != 0) {
        ERROR("set loaded time failed");
        goto out;
    }

out:

    return ret;
}

static int register_image(image_t *desc)
{
    int ret = 0;
    bool image_created = false;


    if (desc == NULL || desc->im_id == NULL) {
        ERROR("Invalid NULL pointer");
        return -1;
    }

    ret = register_layers(desc);
    if (ret != 0) {
        ERROR("registry layers failed");
        goto out;
    }

    ret = create_image(desc);
    if (ret != 0) {
        ERROR("create image failed");
        goto out;
    }
    image_created = true;

    ret = set_config(desc);
    if (ret != 0) {
        ERROR("set image config failed");
        goto out;
    }

    ret = set_manifest(desc->manifest, desc->im_id);
    if (ret != 0) {
        ERROR("set manifest failed");
        goto out;
    }

    ret = set_loaded_time(desc->im_id);
    if (ret != 0) {
        ERROR("set loaded time failed");
        goto out;
    }

    ret = storage_img_set_image_size(desc->im_id);
    if (ret != 0) {
        ERROR("set image size failed for %s failed", desc->im_id);
        goto out;
    }

out:

    if (ret != 0 && image_created) {
        if (storage_img_delete(desc->im_id, true)) {
            ERROR("delete image %d failed", desc->im_id);
        }
    }

    return ret;
}

static image_t *process_manifest(const image_manifest_items_element *manifest, const char *dstdir)
{
    int ret = 0;
    size_t i = 0;
    char *layer_fpath = NULL;
    char *config_fpath = NULL;
    char *image_digest = NULL;
    char *image_id = NULL;
    image_t *im = NULL;

    im = util_common_calloc_s(sizeof(image_t));
    if (im == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }

    config_fpath = util_path_join(dstdir, manifest->config);
    if (config_fpath == NULL) {
        ret = -1;
        ERROR("Path:%s join failed", manifest->config);
        goto out;
    }

    image_digest = util_full_file_digest(config_fpath);
    if (image_digest == NULL) {
        ret = -1;
        ERROR("Calc image config file %s digest err", manifest->config);
        goto out;
    }

    image_id = without_sha256_prefix(image_digest);
    if (image_id == NULL) {
        ret = -1;
        ERROR("Remove sha256 prefix error from image digest %s", image_digest);
        goto out;
    }

    im->im_id = util_strdup_s(image_id);
    im->im_digest = util_strdup_s(image_digest);
    im->config_fpath = util_strdup_s(config_fpath);
    im->repo_tags_len = manifest->repo_tags_len;
    im->repo_tags = str_array_copy(manifest->repo_tags, manifest->repo_tags_len);
    im->layers_len = manifest->layers_len;
    im->layers = util_common_calloc_s(sizeof(layer_blob*) * manifest->layers_len);
    if (im->layers == NULL) {
        ret = -1;
        ERROR("Calloc memory failed");
        goto out;
    }

    for (; i < im->layers_len; i++) {
        im->layers[i] = util_common_calloc_s(sizeof(layer_blob));
        if (im->layers[i] == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }

        layer_fpath = util_path_join(dstdir, manifest->layers[i]);
        if (layer_fpath == NULL) {
            ret = -1;
            ERROR("Path join failed");
            goto out;
        }

        im->layers[i]->diff_id = calc_diffid(layer_fpath);
        if (im->layers[i]->diff_id == NULL) {
            ret = -1;
            ERROR("Calc layer %s diff id failed", manifest->layers[i]);
            goto out;
        }
        im->layers[i]->fpath = util_strdup_s(layer_fpath);

        UTIL_FREE_AND_SET_NULL(layer_fpath);
    }

    ret = set_chain_id_to_image(im);
    if (ret != 0) {
        ERROR("Calc image chain id failed");
    }

out:
    free(config_fpath);
    free(image_digest);
    if (layer_fpath != NULL) {
        free(layer_fpath);
    }
    if (ret != 0) {
        free_image(im);
        return NULL;
    }

    return im;
}

static int set_manifest_info(image_t *im)
{
    int ret = 0;
    size_t i = 0;
    int64_t size = 0;

    if (im == NULL) {
        ERROR("Invalid input image ptr");
        return -1;
    }

    im->manifest = util_common_calloc_s(sizeof(oci_image_manifest));
    if (im->manifest == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }

    im->manifest->schema_version = OCI_SCHEMA_VERSION;
    im->manifest->layers = util_common_calloc_s(sizeof(oci_image_content_descriptor*) * im->layers_len);
    if (im->manifest->layers == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    im->manifest->layers_len = im->layers_len;
    im->manifest->config = util_common_calloc_s(sizeof(oci_image_content_descriptor));
    if (im->manifest->config == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    im->manifest->config->media_type = util_strdup_s(MediaTypeDockerSchema2Config);
    im->manifest->config->digest = util_strdup_s(im->im_digest);

    size = util_file_size(im->config_fpath);
    if (size < 0) {
        ERROR("Calc image config file %s size err", im->config_fpath);
        ret = -1;
        goto out;
    }

    im->manifest->config->size = size;

    for (; i < im->layers_len; i++) {
        im->manifest->layers[i] = util_common_calloc_s(sizeof(oci_image_content_descriptor));
        if (im->manifest->layers[i] == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        im->manifest->layers[i]->media_type = util_strdup_s(MediaTypeDockerSchema2LayerGzip);
        im->manifest->layers[i]->digest = util_strdup_s(im->layers[i]->diff_id);

        size = util_file_size(im->layers[i]->fpath);
        if (size < 0) {
            ERROR("Calc image layer %s size error", im->layers[i]->fpath);
            ret = -1;
            goto out;
        }
        im->manifest->layers[i]->size = size;
    }

out:
    if (ret != 0) {
        free_oci_image_manifest(im->manifest);
        im->manifest = NULL;
    }
    return ret;
}

static int check_image_layers(image_t *im)
{
    int ret;
    size_t i = 0;
    oci_image_spec *conf = NULL;

    conf = load_image_config(im->config_fpath);
    if (conf == NULL || conf->rootfs == NULL) {
        ERROR("Load image config file %s failed", im->config_fpath);
        ret = -1;
        goto out;
    }

    if (conf->rootfs->diff_ids_len != im->layers_len) {
        ret = -1;
        ERROR("Config file layer numbers are not equal to with image layer numbers");
        goto out;
    }

    for (; i < im->layers_len; i++) {
        if (strcmp(im->layers[i]->diff_id, conf->rootfs->diff_ids[i]) != 0) {
            ERROR("Layer diff id %s check err", im->layers[i]->diff_id);
            ret = -1;
            goto out;
        }
    }
    ret = 0;

out:
    free_oci_image_spec(conf);
    return ret;
}

int oci_do_load(const im_load_request *request)
{
    int ret;
    size_t i = 0;
    struct archive_options options = { 0 };
    struct io_read_wrapper reader = { 0 };
    char *manifest_fpath = NULL;
    image_manifest_items_element **manifest = NULL;
    size_t manifest_len = 0;
    image_t *im = NULL;
    char *digest = NULL;
    char dstdir[] = "/var/tmp/isulad-load-XXXXXX";

    if (mkdtemp(dstdir) == NULL) {
        ERROR("make temporary direcory failed: %s", strerror(errno));
        ret = -1;
        goto out;
    }

    if (file_read_wrapper(request->file, &reader) != 0) {
        ERROR("Failed to fill layer read wrapper");
        ret = -1;
        goto out;
    }

    options.whiteout_format = NONE_WHITEOUT_FORMATE;
    ret = archive_unpack(&reader, dstdir, &options);
    if (ret != 0) {
        ERROR("Failed to unpack to :%s", dstdir);
        goto out;
    }

    manifest_fpath = util_path_join(dstdir, "manifest.json");
    if (manifest_fpath == NULL) {
        ERROR("Failed to join manifest.json path:%s", dstdir);
        ret = -1;
        goto out;
    }

    manifest = load_manifest(manifest_fpath, &manifest_len);
    if (manifest == NULL) {
        ERROR("Failed to load manifest.json file from path:%s", manifest_fpath);
        ret = -1;
        goto out;
    }

    digest = util_full_file_digest(manifest_fpath);
    if (digest == NULL) {
        ret = -1;
        ERROR("calculate digest failed for manifest file %s", manifest_fpath);
        goto out;
    }

    for (; i < manifest_len; i++) {
        im = process_manifest(manifest[i], dstdir);
        if (im == NULL) {
            ret = -1;
            goto out;
        }

        ret = set_manifest_info(im);
        if (ret != 0) {
            ERROR("Image %s set manifest info err", im->im_id);
            goto out;
        }

        ret = check_image_layers(im);
        if (ret != 0) {
            ERROR("Image %s check err", im->im_id);
            goto out;
        }

        im->manifest_digest = util_strdup_s(digest);

        ret = register_image(im);
        if (ret != 0) {
            ERROR("error register image %s to store", im->im_id);
            goto out;
        }
        free_image(im);
        im = NULL;
    }

    ret = 0;

out:
    free(manifest_fpath);
    free(digest);
    if (im != NULL) {
        free_image(im);
    }
    for (i = 0; i < manifest_len; i++) {
        free_image_manifest_items_element(manifest[i]);
    }

    if (reader.close != NULL) {
        reader.close(reader.context, NULL);
    }
    return ret;
}