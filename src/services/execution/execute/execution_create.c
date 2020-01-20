/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: tanyifeng
 * Create: 2017-11-22
 * Description: provide container create callback function definition
 ********************************************************************************/
#include "execution_create.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <lcr/lcrcontainer.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <malloc.h>

#include "log.h"
#include "engine.h"
#include "console.h"
#include "isulad_config.h"
#include "config.h"
#include "specs.h"
#include "verify.h"
#include "containers_store.h"
#include "execution_network.h"
#include "runtime.h"
#include "plugin.h"
#include "image.h"
#include "utils.h"
#include "error.h"
#include "constants.h"

static int create_request_check(const container_create_request *request)
{
    int ret = 0;
    parser_error err = NULL;

    if (request == NULL) {
        ERROR("Receive NULL container id");
        ret = -1;
        goto out;
    }

    if ((request->rootfs == NULL && request->image == NULL)) {
        ERROR("Container image or rootfs error");
        ret = -1;
        goto out;
    }

    if (request->image != NULL && !util_valid_image_name(request->image)) {
        ERROR("invalid image name %s", request->image);
        isulad_set_error_message("Invalid image name '%s'", request->image);
        ret = -1;
        goto out;
    }

    if (request->runtime == NULL) {
        ERROR("Receive NULL Request runtime");
        ret = -1;
        goto out;
    }

    if (!util_valid_runtime_name(request->runtime)) {
        ERROR("Invalid runtime name:%s", request->runtime);
        isulad_set_error_message("Invalid runtime name (%s), only \"lcr\" supported.",
                                 request->runtime);
        ret = -1;
        goto out;
    }

    if (request->hostconfig == NULL) {
        ERROR("Receive NULL Request hostconfig");
        ret = -1;
        goto out;
    }

    if (request->customconfig == NULL) {
        ERROR("Receive NULL Request customconfig");
        ret = -1;
        goto out;
    }

out:
    free(err);
    return ret;
}

static host_config *get_host_spec_from_request(const container_create_request *request)
{
    parser_error err = NULL;
    host_config *host_spec = NULL;

    host_spec = host_config_parse_data(request->hostconfig, NULL, &err);
    if (host_spec == NULL) {
        ERROR("Failed to parse host config data:%s", err);
    }

    free(err);
    return host_spec;
}

static int merge_external_rootfs_to_host_config(host_config *host_spec, const char *external_rootfs)
{
    if (host_spec == NULL) {
        return -1;
    }
    host_spec->external_rootfs = external_rootfs != NULL ? util_strdup_s(external_rootfs) : NULL;

    return 0;
}

static host_config *get_host_spec(const container_create_request *request)
{
    host_config *host_spec = NULL;

    host_spec = get_host_spec_from_request(request);
    if (host_spec == NULL) {
        return NULL;
    }

    if (merge_external_rootfs_to_host_config(host_spec, request->rootfs) != 0) {
        goto error_out;
    }

    if (verify_host_config_settings(host_spec, false)) {
        ERROR("Failed to verify host config settings");
        goto error_out;
    }

    return host_spec;

error_out:
    free_host_config(host_spec);
    return NULL;
}

static container_custom_config *get_custom_spec_from_request(const container_create_request *request)
{
    parser_error err = NULL;

    container_custom_config *custom_spec = container_custom_config_parse_data(request->customconfig, NULL, &err);
    if (custom_spec == NULL) {
        ERROR("Failed to parse custom config data:%s", err);
    }

    free(err);
    return custom_spec;
}

static int add_default_log_config_to_custom_spec(const char *id, const char *runtime_root,
                                                 container_custom_config *custom_config)
{
    int ret = 0;

    /* generate default log path */
    if (custom_config->log_config == NULL) {
        custom_config->log_config = util_common_calloc_s(sizeof(container_custom_config_log_config));
        if (custom_config->log_config == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }
    }

    if (custom_config->log_config->log_file == NULL) {
        char default_path[PATH_MAX] = { 0 };
        int nret = snprintf(default_path, PATH_MAX, "%s/%s/console.log", runtime_root, id);
        if (nret >= PATH_MAX || nret < 0) {
            ERROR("Create default log path for container %s failed", id);
            ret = -1;
            goto out;
        }
        custom_config->log_config->log_file = util_strdup_s(default_path);
    }

out:
    return ret;
}

static container_custom_config *get_custom_spec(const char *id, const char *runtime_root,
                                                const container_create_request *request)
{
    container_custom_config *custom_spec = NULL;

    custom_spec = get_custom_spec_from_request(request);
    if (custom_spec == NULL) {
        return NULL;
    }

    if (add_default_log_config_to_custom_spec(id, runtime_root, custom_spec)) {
        goto error_out;
    }

    return custom_spec;

error_out:
    free_container_custom_config(custom_spec);
    return NULL;
}

static oci_runtime_spec *merge_config(const char *id, const char *image_type, const char *image_name,
                                      const char *ext_config_image, host_config *host_spec,
                                      container_custom_config *custom_spec,
                                      container_config_v2_common_config *v2_spec, char **real_rootfs)
{
    oci_runtime_spec *oci_spec = NULL;

    oci_spec = merge_container_config(id, image_type, image_name, ext_config_image, host_spec, custom_spec,
                                      v2_spec, real_rootfs);
    if (oci_spec == NULL) {
        ERROR("Failed to merge config");
        goto error_out;
    }

    if (merge_global_config(oci_spec) != 0) {
        ERROR("Failed to merge global config");
        goto error_out;
    }

    return oci_spec;

error_out:
    free_oci_runtime_spec(oci_spec);
    return NULL;
}

static int merge_config_for_syscontainer(const container_create_request *request, const host_config *host_spec,
                                         const container_custom_config *custom_spec, const oci_runtime_spec *oci_spec)
{
    int ret = 0;

    if (!host_spec->system_container || request->rootfs == NULL) {
        return 0;
    }

    if (merge_network(host_spec, request->rootfs, custom_spec->hostname) != 0) {
        ERROR("Failed to merge network config");
        ret = -1;
        goto out;
    }

    if (append_json_map_string_string(oci_spec->annotations, "rootfs.mount", request->rootfs)) {
        ERROR("Realloc annotations failed");
        ret = -1;
        goto out;
    }

out:
    return ret;
}


static char *try_generate_id()
{
    int i = 0;
    int max_time = 10;
    char *id = NULL;
    char *value = NULL;

    id = util_common_calloc_s(sizeof(char) * (CONTAINER_ID_MAX_LEN + 1));
    if (id == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    for (i = 0; i < max_time; i++) {
        if (util_generate_random_str(id, (size_t)CONTAINER_ID_MAX_LEN)) {
            ERROR("Generate id failed");
            goto err_out;
        }

        value = name_index_get(id);
        if (value != NULL) {
            continue;
        } else {
            goto out;
        }
    }

err_out:
    free(id);
    id = NULL;
out:
    return id;
}

static int inspect_image(const char *image, imagetool_image **result)
{
    int ret = 0;
    im_status_request *request = NULL;
    im_status_response *response = NULL;

    if (image == NULL) {
        ERROR("Empty image name or id");
        return -1;
    }

    request = (im_status_request *)util_common_calloc_s(sizeof(im_status_request));
    if (request == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    request->image.image = util_strdup_s(image);

    if (im_image_status(request, &response) != 0) {
        if (response != NULL && response->errmsg != NULL) {
            ERROR("failed to inspect inspect image info: %s", response->errmsg);
        } else {
            ERROR("Failed to call status image");
        }
        ret = -1;
        goto cleanup;
    }

    if (response->image_info != NULL) {
        *result = response->image_info->image;
        response->image_info->image = NULL;
    }

cleanup:
    free_im_status_request(request);
    free_im_status_response(response);
    return ret;
}

static int conf_get_image_id(const char *image, char **id)
{
    int ret = 0;
    imagetool_image *ir = NULL;
    size_t len = 0;
    char *image_id = NULL;

    if (image == NULL || strcmp(image, "none") == 0) {
        *id = util_strdup_s("none");
        return 0;
    }

    if (inspect_image(image, &ir) != 0) {
        ERROR("Failed to inspect image status");
        ret = -1;
        goto out;
    }

    if (strlen(ir->id) > SIZE_MAX / sizeof(char) - strlen("sha256:")) {
        ERROR("Invalid image id");
        ret = -1;
        goto out;
    }

    len = strlen("sha256:") + strlen(ir->id) + 1;
    image_id = (char *)util_common_calloc_s(len * sizeof(char));
    if (image_id == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    int nret = snprintf(image_id, len, "sha256:%s", ir->id);
    if (nret < 0 || (size_t)nret >= len) {
        ERROR("Failed to sprintf string");
        ret = -1;
        goto out;
    }

    *id = image_id;
    image_id = NULL;

out:
    free_imagetool_image(ir);
    free(image_id);
    return ret;
}

static int register_new_container(const char *id, const char *runtime, host_config **host_spec,
                                  container_config_v2_common_config **v2_spec)
{
    int ret = -1;
    bool registed = false;
    char *runtime_root = NULL;
    char *runtime_stat = NULL;
    char *image_id = NULL;
    container_t *cont = NULL;

    runtime_root = conf_get_routine_rootdir(runtime);
    if (runtime_root == NULL) {
        goto out;
    }

    runtime_stat = conf_get_routine_statedir(runtime);
    if (runtime_stat == NULL) {
        goto out;
    }

    if (strcmp((*v2_spec)->image_type, IMAGE_TYPE_OCI) == 0) {
        if (conf_get_image_id((*v2_spec)->image, &image_id) != 0) {
            goto out;
        }
    }
    cont = container_new(runtime, runtime_root, runtime_stat, image_id, host_spec, v2_spec);
    if (cont == NULL) {
        ERROR("Failed to create container '%s'", id);
        goto out;
    }

    if (container_to_disk_locking(cont)) {
        ERROR("Failed to save container '%s'", id);
        goto out;
    }

    registed = containers_store_add(id, cont);
    if (!registed) {
        ERROR("Failed to register container '%s'", id);
        goto out;
    }

    ret = 0;
out:
    free(runtime_root);
    free(runtime_stat);
    free(image_id);
    if (ret != 0) {
        container_unref(cont);
    }
    return ret;
}

static int maintain_container_id(const container_create_request *request, char **out_id, char **out_name)
{
    int ret = 0;
    char *id = NULL;
    char *name = NULL;

    id = try_generate_id();
    if (id == NULL) {
        ERROR("Failed to generate container ID");
        isulad_set_error_message("Failed to generate container ID");
        ret = -1;
        goto out;
    }

    set_log_prefix(id);

    if (request->id != NULL) {
        name = util_strdup_s(request->id);
    } else {
        name = util_strdup_s(id);
    }

    if (!util_valid_container_name(name)) {
        ERROR("Invalid container name (%s), only [a-zA-Z0-9][a-zA-Z0-9_.-]+$ are allowed.", name);
        isulad_set_error_message("Invalid container name (%s), only [a-zA-Z0-9][a-zA-Z0-9_.-]+$ are allowed.", name);
        ret = -1;
        goto out;
    }

    EVENT("Event: {Object: %s, Type: Creating %s}", id, name);

    if (!name_index_add(name, id)) {
        ERROR("Name %s is in use", name);
        isulad_set_error_message("Conflict. The name \"%s\" is already in use by container %s. "
                                 "You have to remove (or rename) that container to be able to reuse that name.",
                                 name, name);
        ret = -1;
        goto out;
    }

out:
    *out_id = id;
    *out_name = name;
    return ret;
}

static char *get_runtime_from_request(const container_create_request *request)
{
    return strings_to_lower(request->runtime);
}

static void pack_create_response(container_create_response *response, const char *id, uint32_t cc)
{
    response->cc = cc;
    if (g_isulad_errmsg != NULL) {
        response->errmsg = util_strdup_s(g_isulad_errmsg);
        DAEMON_CLEAR_ERRMSG();
    }
    if (id != NULL) {
        response->id = util_strdup_s(id);
    }
}

static int prepare_host_channel(const host_config_host_channel *host_channel, const char *user_remap)
{
    unsigned int host_uid = 0;
    unsigned int host_gid = 0;
    unsigned int size = 0;

    if (host_channel == NULL) {
        return 0;
    }
    if (util_dir_exists(host_channel->path_on_host)) {
        ERROR("Host path '%s' already exist", host_channel->path_on_host);
        return -1;
    }
    if (util_mkdir_p(host_channel->path_on_host, HOST_PATH_MODE)) {
        ERROR("Failed to create host path '%s'.", host_channel->path_on_host);
        return -1;
    }
    if (user_remap != NULL) {
        if (util_parse_user_remap(user_remap, &host_uid, &host_gid, &size)) {
            ERROR("Failed to split string '%s'.", user_remap);
            return -1;
        }
        if (chown(host_channel->path_on_host, host_uid, host_gid) != 0) {
            ERROR("Failed to chown host path '%s'.", host_channel->path_on_host);
            return -1;
        }
    }
    return 0;
}

void umount_host_channel(const host_config_host_channel *host_channel)
{
    if (host_channel == NULL) {
        return;
    }

    if (detect_mount(host_channel->path_on_host)) {
        if (umount2(host_channel->path_on_host, MNT_DETACH)) {
            ERROR("Failed to umount the target: %s", host_channel->path_on_host);
        }
    }
    if (util_recursive_rmdir(host_channel->path_on_host, 0)) {
        ERROR("Failed to delete host path: %s", host_channel->path_on_host);
    }
}

static int generate_merged_oci_config_json(const char *id, oci_runtime_spec *oci_spec,
                                           char **oci_config_data)
{
    char *err = NULL;

    if (verify_container_settings(oci_spec)) {
        ERROR("Failed to verify container settings");
        return -1;
    }

    /* modify oci_spec by plugin. */
    if (plugin_event_container_pre_create(id, oci_spec)) {
        ERROR("Plugin event pre create failed");
        (void)plugin_event_container_post_remove2(id, oci_spec); /* ignore error */
        return -1;
    }

    *oci_config_data = oci_runtime_spec_generate_json(oci_spec, 0, &err);
    if (*oci_config_data == NULL) {
        ERROR("Failed to generate runtime spec json,erro:%s", err);
        free(err);
        isulad_set_error_message("Failed to generate runtime spec json");
        return -1;
    }
    free(err);
    return 0;
}

static int create_container_root_dir(const char *id, const char *runtime_root)
{
    int ret = 0;
    int nret;
    char container_root[PATH_MAX] = { 0x00 };
    mode_t mask = umask(S_IWOTH);

    nret = snprintf(container_root, sizeof(container_root), "%s/%s", runtime_root, id);
    if ((size_t)nret >= sizeof(container_root) || nret < 0) {
        ret = -1;
        goto out;
    }
    // create container dir
    nret = util_mkdir_p(container_root, CONFIG_DIRECTORY_MODE);
    if (nret != 0 && errno != EEXIST) {
        SYSERROR("Failed to create container path %s", container_root);
        ret = -1;
        goto out;
    }

out:
    umask(mask);
    return ret;
}

static int delete_container_root_dir(const char *id, const char *runtime_root)
{
    int ret = 0;
    char container_root[PATH_MAX] = { 0x00 };

    ret = snprintf(container_root, sizeof(container_root), "%s/%s", runtime_root, id);
    if ((size_t)ret >= sizeof(container_root) || ret < 0) {
        ERROR("Failed to sprintf invalid root directory %s/%s", runtime_root, id);
        ret = -1;
        goto out;
    }

    ret = util_recursive_rmdir(container_root, 0);
    if (ret != 0) {
        ERROR("Failed to delete container's state directory %s", container_root);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

static container_config_v2_common_config *get_config_v2_spec(const char *id, const char *runtime_root,
                                                             const host_config *host_spec)
{
    container_config_v2_common_config *v2_spec = NULL;

    v2_spec = util_common_calloc_s(sizeof(container_config_v2_common_config));
    if (v2_spec == NULL) {
        ERROR("Failed to malloc container_config_v2_common_config");
        return NULL;
    }

    /* set network config to v2_spec */
    if (init_container_network_confs(id, runtime_root, host_spec, v2_spec) != 0) {
        ERROR("Init Network files failed");
        goto error_out;
    }

    return v2_spec;

error_out:
    free_container_config_v2_common_config(v2_spec);
    return NULL;
}

/* save config v2 */
static int create_v2_config_json(const char *id, const char *runtime_root, container_config_v2_common_config *v2_spec)
{
    int ret = 0;
    char *json_v2 = NULL;
    parser_error err = NULL;
    container_config_v2 config_v2 = { 0 };
    container_config_v2_state state = { 0 };

    config_v2.common_config = v2_spec;
    config_v2.state = &state;

    json_v2 = container_config_v2_generate_json(&config_v2, NULL, &err);
    if (json_v2 == NULL) {
        ERROR("Failed to generate container config V2 json string:%s", err ? err : " ");
        ret = -1;
        goto out;
    }

    ret = save_config_v2_json(id, runtime_root, json_v2);
    if (ret != 0) {
        ERROR("Failed to save container config V2 json to file");
        ret = -1;
        goto out;
    }

out:
    free(json_v2);
    free(err);
    return ret;
}

/* save host config */
static int create_host_config_json(const char *id, const char *runtime_root, const host_config *host_spec)
{
    int ret = 0;
    char *json_host_config = NULL;
    parser_error err = NULL;

    json_host_config = host_config_generate_json(host_spec, NULL, &err);
    if (json_host_config == NULL) {
        ERROR("Failed to generate container host config json string:%s", err ? err : " ");
        ret = -1;
        goto out;
    }

    ret = save_host_config(id, runtime_root, json_host_config);
    if (ret != 0) {
        ERROR("Failed to save container host config json to file");
        ret = -1;
        goto out;
    }

out:
    free(json_host_config);
    free(err);

    return ret;
}

static int save_container_config_before_create(const char *id, const char *runtime_root, host_config *host_spec,
                                               container_config_v2_common_config *v2_spec)
{
    if (create_v2_config_json(id, runtime_root, v2_spec) != 0) {
        return -1;
    }

    if (create_host_config_json(id, runtime_root, host_spec) != 0) {
        return -1;
    }

    return 0;
}

static host_config_host_channel *dup_host_channel(const host_config_host_channel *channel)
{
    host_config_host_channel *dup_channel = NULL;

    if (channel == NULL) {
        return NULL;
    }

    dup_channel = (host_config_host_channel *)util_common_calloc_s(sizeof(host_config_host_channel));
    if (dup_channel == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    dup_channel->path_on_host = channel->path_on_host != NULL ? util_strdup_s(channel->path_on_host) : NULL;
    dup_channel->path_in_container = channel->path_in_container != NULL ?
                                     util_strdup_s(channel->path_in_container) : NULL;
    dup_channel->permissions = channel->permissions != NULL ? util_strdup_s(channel->permissions) : NULL;
    dup_channel->size = channel->size;

    return dup_channel;
}

static int verify_merged_custom_config(const container_custom_config *custom_spec)
{
    if (verify_health_check_parameter(custom_spec) != 0) {
        return -1;
    }

    return 0;
}

static int response_allocate_memory(container_create_response **response)
{
    if (response == NULL) {
        ERROR("Invalid NULL input");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(container_create_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    return 0;
}

static int get_request_container_info(const container_create_request *request, char **id, char **name, uint32_t *cc)
{
    if (create_request_check(request) != 0) {
        ERROR("Invalid create container request");
        *cc = ISULAD_ERR_INPUT;
        return -1;
    }

    if (maintain_container_id(request, id, name) != 0) {
        *cc = ISULAD_ERR_EXEC;
        return -1;
    }

    return 0;
}

static int get_request_image_info(const container_create_request *request, char **image_type,
                                  const char **ext_config_image, const char **image_name)
{
    *image_type = im_get_image_type(request->image, request->rootfs);
    if (*image_type == NULL) {
        return -1;
    }

    if (request->rootfs != NULL) {
        *image_name = request->rootfs;
        // Do not use none image because none image has no config.
        if (strcmp(request->image, "none") && strcmp(request->image, "none:latest")) {
            *ext_config_image = request->image;
        }
    } else {
        *image_name = request->image;
    }

    // Check if config image exist if provided.
    if (*ext_config_image != NULL) {
        if (!im_config_image_exist(*ext_config_image)) {
            return -1;
        }
    }

    return 0;
}

static int preparate_runtime_environment(const container_create_request *request, const char *id,
                                         char **runtime, char **runtime_root, uint32_t *cc)
{
    *runtime = get_runtime_from_request(request);
    if (*runtime == NULL) {
        *cc = ISULAD_ERR_INPUT;
        return -1;
    }

    *runtime_root = conf_get_routine_rootdir(*runtime);
    if (*runtime_root == NULL) {
        *cc = ISULAD_ERR_EXEC;
        return -1;
    }

    if (create_container_root_dir(id, *runtime_root) != 0) {
        *cc = ISULAD_ERR_EXEC;
        return -1;
    }

    return 0;
}

static int get_basic_spec(const container_create_request *request, const char *id, const char *runtime_root,
                          host_config **host_spec, container_custom_config **custom_spec)
{
    *host_spec = get_host_spec(request);
    if (*host_spec == NULL) {
        return -1;
    }

    *custom_spec = get_custom_spec(id, runtime_root, request);
    if (*custom_spec == NULL) {
        return -1;
    }

    return 0;
}

static int get_v2_spec(const container_create_request *request, const char *id, const char *name,
                       const char *runtime_root, const host_config *host_spec, const char *image_type,
                       container_config_v2_common_config **v2_spec)
{
    *v2_spec = get_config_v2_spec(id, runtime_root, host_spec);
    if (*v2_spec == NULL) {
        ERROR("Failed to malloc container_config_v2_common_config");
        return -1;
    }

    if (v2_spec_make_basic_info(id, name, request->image, image_type, *v2_spec) != 0) {
        ERROR("Failed to malloc container_config_v2_common_config");
        return -1;
    }

    return 0;
}

static int v2_spec_merge_config(const container_custom_config *custom_spec, const oci_runtime_spec *oci_spec,
                                container_config_v2_common_config *v2_spec)
{
    if (v2_spec_merge_custom_spec(custom_spec, v2_spec) != 0) {
        ERROR("Failed to malloc container_config_v2_common_config");
        return -1;
    }

    if (v2_spec_merge_oci_spec(oci_spec, v2_spec) != 0) {
        ERROR("Failed to malloc container_config_v2_common_config");
        return -1;
    }

    return 0;
}

static int generate_oci_config_json(const char *id, const container_create_request *request,
                                    const host_config *host_spec, const container_custom_config *custom_spec,
                                    oci_runtime_spec *oci_spec, char **oci_config_data)
{
    if (verify_merged_custom_config(custom_spec)) {
        return -1;
    }

    if (merge_config_for_syscontainer(request, host_spec, custom_spec, oci_spec) != 0) {
        ERROR("Failed to merge config for syscontainer");
        return -1;
    }

    if (generate_merged_oci_config_json(id, oci_spec, oci_config_data) != 0) {
        return -1;
    }

    return 0;
}

int container_create_cb(const container_create_request *request,
                        container_create_response **response)
{
    uint32_t cc = ISULAD_SUCCESS;
    char *real_rootfs = NULL;
    char *image_type = NULL;
    char *runtime_root = NULL;
    char *oci_config_data = NULL;
    char *runtime = NULL;
    char *name = NULL;
    char *id = NULL;
    const char *image_name = NULL;
    const char *ext_config_image = NULL;
    oci_runtime_spec *oci_spec = NULL;
    host_config *host_spec = NULL;
    container_custom_config *custom_spec = NULL;
    container_config_v2_common_config *v2_spec = NULL;
    host_config_host_channel *host_channel = NULL;
    rt_create_params_t create_params = { 0 };
    rt_rm_params_t delete_params = { 0 };

    DAEMON_CLEAR_ERRMSG();

    if (response_allocate_memory(response) != 0) {
        return -1;
    }

    if (get_request_container_info(request, &id, &name, &cc) != 0) {
        goto pack_response;
    }

    if (get_request_image_info(request, &image_type, &ext_config_image, &image_name) != 0) {
        cc = ISULAD_ERR_EXEC;
        goto clean_nameindex;
    }

    if (preparate_runtime_environment(request, id, &runtime, &runtime_root, &cc) != 0) {
        goto clean_nameindex;
    }

    if (get_basic_spec(request, id, runtime_root, &host_spec, &custom_spec) != 0) {
        cc = ISULAD_ERR_INPUT;
        goto clean_container_root_dir;
    }

    if (get_v2_spec(request, id, name, runtime_root, host_spec, image_type, &v2_spec) != 0) {
        cc = ISULAD_ERR_EXEC;
        goto clean_container_root_dir;
    }

    if (save_container_config_before_create(id, runtime_root, host_spec, v2_spec) != 0) {
        cc = ISULAD_ERR_EXEC;
        goto clean_container_root_dir;
    }

    host_channel = dup_host_channel(host_spec->host_channel);
    if (prepare_host_channel(host_channel, host_spec->user_remap)) {
        ERROR("Failed to prepare host channel with '%s'", host_spec->host_channel);
        cc = ISULAD_ERR_EXEC;
        goto clean_container_root_dir;
    }

    oci_spec = merge_config(id, image_type, image_name, ext_config_image, host_spec, custom_spec, v2_spec,
                            &real_rootfs);
    if (oci_spec == NULL) {
        cc = ISULAD_ERR_EXEC;
        goto clean_rootfs;
    }
    if (real_rootfs == NULL) {
        ERROR("Can not found rootfs");
        cc = ISULAD_ERR_INPUT;
        goto clean_rootfs;
    }

    if (generate_oci_config_json(id, request, host_spec, custom_spec, oci_spec, &oci_config_data) != 0) {
        cc = ISULAD_ERR_INPUT;
        goto clean_rootfs;
    }

    create_params.real_rootfs = real_rootfs;
    create_params.oci_config_data = oci_config_data;

    if (runtime_create(id, runtime, &create_params) != 0) {
        ERROR("Runtime create container failed");
        cc = ISULAD_ERR_EXEC;
        goto clean_rootfs;
    }

    if (v2_spec_merge_config(custom_spec, oci_spec, v2_spec) != 0) {
        cc = ISULAD_ERR_EXEC;
        goto clean_on_error;
    }

    if (register_new_container(id, runtime, &host_spec, &v2_spec)) {
        ERROR("Failed to register new container");
        cc = ISULAD_ERR_EXEC;
        goto clean_on_error;
    }

    EVENT("Event: {Object: %s, Type: Created %s}", id, name);
    goto pack_response;

clean_on_error:
    delete_params.rootpath = runtime_root;
    (void)runtime_rm(id, runtime, &delete_params);

clean_rootfs:
    (void)im_remove_container_rootfs(image_type, id);
    umount_host_channel(host_channel);

clean_container_root_dir:
    (void)delete_container_root_dir(id, runtime_root);

clean_nameindex:
    name_index_remove(name);

pack_response:
    pack_create_response(*response, id, cc);
    free(runtime);
    free(oci_config_data);
    free(runtime_root);
    free(real_rootfs);
    free(image_type);
    free(name);
    free(id);
    free_oci_runtime_spec(oci_spec);
    free_host_config(host_spec);
    free_container_custom_config(custom_spec);
    free_container_config_v2_common_config(v2_spec);
    free_host_config_host_channel(host_channel);
    free_log_prefix();
    malloc_trim(0);
    return (cc == ISULAD_SUCCESS) ? 0 : -1;
}

