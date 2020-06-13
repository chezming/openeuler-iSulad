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
* Author: gaohuatao
* Create: 2020-01-19
* Description: provide devicemapper graphdriver function definition
******************************************************************************/
#include "deviceset.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libdevmapper.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/sysmacros.h>
#include <sys/mount.h>
#include <sys/vfs.h>

#include "isula_libutils/log.h"
#include "libisulad.h"
#include "utils.h"
#include "wrapper_devmapper.h"
#include "devices_constants.h"
#include "libdevmapper.h"
#include "driver.h"

#define DM_LOG_FATAL 2
#define DM_LOG_DEBUG 7

// static int64_t default_udev_wait_timeout = 185;
static uint64_t default_base_fs_size = 10L * 1024L * 1024L * 1204L;

static char *util_trim_prefice_string(char *str, const char *prefix)
{
    if (str == NULL || !util_has_prefix(str, prefix)) {
        return str;
    }

    char *begin = str + strlen(prefix);
    char *tmp = str;
    while ((*tmp++ = *begin++)) {
    }
    return str;
}

static int devmapper_parse_options(struct device_set *devset, const char **options, size_t options_len)
{
    size_t i = 0;

    if (devset == NULL) {
        return -1;
    }

    for (i = 0; options != NULL && i < options_len; i++) {
        char *dup = NULL;
        char *p = NULL;
        char *val = NULL;
        int ret = 0;

        dup = util_strdup_s(options[i]);
        if (dup == NULL) {
            isulad_set_error_message("Out of memory");
            return -1;
        }
        p = strchr(dup, '=');
        if (!p) {
            isulad_set_error_message("Unable to parse key/value option: '%s'", dup);
            free(dup);
            return -1;
        }
        *p = '\0';
        val = p + 1;
        if (strcasecmp(dup, "dm.fs") == 0) {
            if (strcmp(val, "ext4") == 0) {
                devset->filesystem = util_strdup_s(val);
            } else {
                isulad_set_error_message("Invalid filesystem: '%s': not supported", val);
                ret = -1;
            }
        } else if (strcasecmp(dup, "dm.thinpooldev") == 0) {
            if (!util_valid_str(val)) {
                isulad_set_error_message("Invalid thinpool device, it must not be empty");
                ret = -1;
                goto out;
            }
            char *tmp_val = util_trim_prefice_string(val, "/dev/mapper/");
            devset->thin_pool_device = util_strdup_s(tmp_val);
        } else if (strcasecmp(dup, "dm.min_free_space") == 0) {
            long converted = 0;
            ret = util_parse_percent_string(val, &converted);
            if (ret != 0 || converted == 100) {
                isulad_set_error_message("Invalid min free space: '%s': %s", val, strerror(-ret));
                ret = -1;
            }
            devset->min_free_space_percent = (uint32_t)converted;
        } else if (strcasecmp(dup, "dm.basesize") == 0) {
            int64_t converted = 0;
            ret = util_parse_byte_size_string(val, &converted);
            if (ret != 0) {
                isulad_set_error_message("Invalid size: '%s': %s", val, strerror(-ret));
                goto out;
            }
            if (converted <= 0) {
                isulad_set_error_message("dm.basesize is lower than zero");
                ret = -1;
                goto out;
            }
            devset->user_base_size = true;
            devset->base_fs_size = (uint64_t)converted;
        } else if (strcasecmp(dup, "dm.mkfsarg") == 0 || strcasecmp(dup, "dm.mountopt") == 0) {
            /* We have no way to check validation here, validation is checked when using them. */
        } else {
            isulad_set_error_message("devicemapper: unknown option: '%s'", dup);
            ret = -1;
        }
out:
        free(dup);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

static int enable_deferred_removal_deletion(struct device_set *devset)
{
    if (devset->enable_deferred_removal) {
        if (!devset->driver_deferred_removal_support) {
            ERROR("devmapper: Deferred removal can not be enabled as kernel does not support it");
            return -1;
        }
        devset->deferred_remove = true;
    }

    if (devset->enable_deferred_deletion) {
        if (!devset->deferred_remove) {
            ERROR("devmapper: Deferred deletion can not be enabled as deferred removal is not enabled. \
                  Enable deferred removal using --storage-opt dm.use_deferred_removal=true parameter");
            return -1;
        }
        devset->deferred_delete = true;
    }
    return 0;
}

static char *metadata_dir(struct device_set *devset)
{
    char *dir = NULL;

    dir = util_path_join(devset->root, "metadata");
    if (dir == NULL) {
        return NULL;
    }

    return dir;
}

static char *deviceset_meta_file(struct device_set *devset)
{
    char *dir = NULL;
    char *file = NULL;

    dir = metadata_dir(devset);
    if (dir == NULL) {
        return NULL;
    }

    file = util_path_join(dir, DEVICE_SET_METAFILE);

    UTIL_FREE_AND_SET_NULL(dir);
    return file;
}

// such as return :container-253:0-409697-401641a00390ccd2b21eb464f5eb5a7b735c3731b717e7bffafe65971f4cb498
// dm_name
static char *get_dm_name(struct device_set *devset, const char *hash)
{
    char buff[PATH_MAX] = { 0 };

    if (hash == NULL) {
        return NULL;
    }

    if (snprintf(buff, sizeof(buff), "%s-%s", devset->device_prefix, strcmp(hash, "") == 0 ? "base" : hash) < 0) {
        return NULL;
    }

    return util_strdup_s(buff);
}

// /dev/mapper/container-253:0-409697-401641a00390ccd2b21eb464f5eb5a7b735c3731b717e7bffafe65971f4cb498
static char *get_dev_name(const char *name)
{
    return util_string_append(name, DEVMAPPER_DECICE_DIRECTORY);
}

char *dev_name(struct device_set *devset, image_devmapper_device_info *info)
{
    char *res_str = NULL;
    char *dm_name = NULL;

    dm_name = get_dm_name(devset, info->hash);
    if (dm_name == NULL) {
        goto out;
    }

    res_str = get_dev_name(dm_name);

out:
    free(dm_name);
    return res_str;
}

// thin-pool or isulad-thinpool
static char *get_pool_name(struct device_set *devset)
{
    char thinp_name[PATH_MAX] = { 0 };
    int ret = 0;

    if (devset == NULL) {
        return NULL;
    }

    if (devset->thin_pool_device == NULL) {
        ret = snprintf(thinp_name, sizeof(thinp_name), "%s-pool", devset->device_prefix);
        if (ret < 0 || (size_t)ret >= sizeof(thinp_name)) {
            ERROR("Print thinpool name %s-pool error", devset->device_prefix);
            return NULL;
        }
        return util_strdup_s(thinp_name);
    }

    return util_strdup_s(devset->thin_pool_device);
}

// /dev/mapper/thin-pool
static char *get_pool_dev_name(struct device_set *devset)
{
    char *pool_name = NULL;
    char *dev_name = NULL;

    pool_name = get_pool_name(devset);
    dev_name = get_dev_name(pool_name);
    if (dev_name == NULL) {
        ERROR("devmapper: pool device name is NULL");
    }

    UTIL_FREE_AND_SET_NULL(pool_name);
    return dev_name;
}

static int deactivate_device_mode(struct device_set *devset, image_devmapper_device_info *dev_info,
                                  bool deferred_remove)
{
    int ret = 0;
    char *dm_name = NULL;
    struct dm_info dinfo;

    dm_name = get_dm_name(devset, dev_info->hash);
    if (dm_name == NULL) {
        ERROR("devmapper: get dm device name failed");
        return -1;
    }

    ret = dev_get_info(&dinfo, dm_name);
    if (ret != 0) {
        ERROR("devmapper: get device info failed");
        goto free_out;
    }

    if (dinfo.exists == 0) {
        ret = 0;
        goto free_out;
    }

    if (deferred_remove) {
        ret = dev_remove_device(dm_name);
    }

free_out:
    UTIL_FREE_AND_SET_NULL(dm_name);
    return ret;
}

static int deactivate_device(struct device_set *devset, image_devmapper_device_info *dev_info)
{
    return deactivate_device_mode(devset, dev_info, devset->deferred_remove);
}

static int pool_status(struct device_set *devset, uint64_t *total_size_in_sectors, uint64_t *transaction_id,
                       uint64_t *data_used, uint64_t *data_total, uint64_t *metadata_used, uint64_t *metadata_total)
{
    uint64_t start;
    uint64_t length;
    char *target_type = NULL;
    char *params = NULL;
    char *name = NULL;
    int ret = 0;

    if (!total_size_in_sectors || !transaction_id || !data_used || !data_total || !metadata_used || !metadata_total) {
        return -1;
    }

    name = get_pool_name(devset);
    if (name == NULL) {
        ret = -1;
        goto out;
    }

    ret = dev_get_status(&start, &length, &target_type, &params, name);
    if (ret != 0) {
        goto out;
    }

    *total_size_in_sectors = length;
    if (sscanf(params, "%lu %lu/%lu %lu/%lu", transaction_id, metadata_used, metadata_total, data_used, data_total) !=
        5) {
        ERROR("devmapper: sscanf device status params failed");
        ret = -1;
    }

out:
    free(name);
    free(target_type);
    free(params);
    return ret;
}

static bool thin_pool_exists(struct device_set *devset, const char *pool_name)
{
    int ret = 0;
    bool exist = true;
    struct dm_info *dinfo = NULL;
    uint64_t start, length;
    char *target_type = NULL;
    char *params = NULL;

    dinfo = util_common_calloc_s(sizeof(struct dm_info));
    if (dinfo == NULL) {
        return false;
    }

    ret = dev_get_info(dinfo, pool_name);
    if (ret != 0) {
        exist = false;
        goto out;
    }

    if (dinfo->exists == 0) {
        exist = false;
        goto out;
    }

    ret = dev_get_status(&start, &length, &target_type, &params, pool_name);
    if (ret != 0 || strcmp(target_type, "thin-pool")) {
        exist = false;
    }

out:
    free(dinfo);
    free(target_type);
    free(params);
    return exist;
}

static image_devmapper_device_info *load_metadata(struct device_set *devset, const char *hash)
{
    image_devmapper_device_info *info = NULL;
    char metadata_file[PATH_MAX] = { 0 };
    char *metadata_path = NULL;
    int ret = 0;
    parser_error err = NULL;

    if (hash == NULL) {
        return NULL;
    }

    metadata_path = metadata_dir(devset);
    if (metadata_path == NULL) {
        goto out;
    }
    if (strcmp(hash, "base") == 0) {
        ret = snprintf(metadata_file, sizeof(metadata_file), "%s/base", metadata_path);
    } else {
        ret = snprintf(metadata_file, sizeof(metadata_file), "%s/%s", metadata_path, hash);
    }
    if (ret < 0 || (size_t)ret >= sizeof(metadata_file)) {
        goto out;
    }
    info = image_devmapper_device_info_parse_file(metadata_file, NULL, &err);
    if (info == NULL) {
        ERROR("load metadata file %s failed %s", metadata_file, err != NULL ? err : "");
        goto out;
    }

    if (info->device_id > MAX_DEVICE_ID) {
        ERROR("devmapper: Ignoring Invalid DeviceId=%d", info->device_id);
        free_image_devmapper_device_info(info);
        info = NULL;
        goto out;
    }

out:
    free(metadata_path);
    free(err);
    return info;
}

static void run_blkid_get_uuid(void *args)
{
    char **tmp_args = (char **)args;
    size_t CMD_ARGS_NUM = 6;

    if (util_array_len((const char **)tmp_args) != CMD_ARGS_NUM) {
        COMMAND_ERROR("Blkid get uuid need six args");
        exit(1);
    }

    execvp(tmp_args[0], tmp_args);
}

// /dev/mapper/container-253:0-409697-401641a00390ccd2b21eb464f5eb5a7b735c3731b717e7bffafe65971f4cb498
static char *get_device_uuid(const char *dev_fname)
{
    char **args = NULL;
    char *stdout = NULL;
    char *stderr = NULL;
    char *uuid = NULL;

    if (dev_fname == NULL) {
        return uuid;
    }

    args = (char **)util_common_calloc_s(sizeof(char *) * 7);
    if (args == NULL) {
        ERROR("Out of memory");
        return uuid;
    }

    args[0] = util_strdup_s("blkid");
    args[1] = util_strdup_s("-s");
    args[2] = util_strdup_s("UUID");
    args[3] = util_strdup_s("-o");
    args[4] = util_strdup_s("value");
    args[5] = util_strdup_s(dev_fname);
    if (!util_exec_cmd(run_blkid_get_uuid, args, NULL, &stdout, &stderr)) {
        ERROR("Unexpected command output %s with error: %s", stdout, stderr);
        goto free_out;
    }

    if (stdout == NULL) {
        ERROR("call blkid -s UUID -o value %s no stdout", dev_fname);
        goto free_out;
    }

    uuid = util_strdup_s(stdout);
    DEBUG("devmapper: UUID for device: %s is:%s", dev_fname, uuid);

free_out:
    util_free_array(args);
    UTIL_FREE_AND_SET_NULL(stdout);
    UTIL_FREE_AND_SET_NULL(stderr);
    return uuid;
}

static void run_grow_rootfs(void *args)
{
    char **tmp_args = (char **)args;
    size_t CMD_ARGS_NUM = 2;

    if (util_array_len((const char **)tmp_args) != CMD_ARGS_NUM) {
        COMMAND_ERROR("grow rootfs need three args");
        exit(1);
    }

    execvp(tmp_args[0], tmp_args);
}

static int exec_grow_fs_command(const char *command, const char *dev_fname)
{
    int ret = 0;
    char **args = NULL;
    char *stdout = NULL;
    char *stderr = NULL;

    if (command == NULL || dev_fname == NULL) {
        // ERROR();
        return -1;
    }

    args = (char **)util_common_calloc_s(sizeof(char *) * 3);
    if (args == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto free_out;
    }

    args[0] = util_strdup_s(command);
    args[1] = util_strdup_s(dev_fname);
    if (!util_exec_cmd(run_grow_rootfs, args, NULL, &stdout, &stderr)) {
        ret = -1;
        ERROR("Grow rootfs failed, unexpected command output %s with error: %s", stdout, stderr);
        goto free_out;
    }

free_out:
    util_free_array(args);
    UTIL_FREE_AND_SET_NULL(stdout);
    UTIL_FREE_AND_SET_NULL(stderr);
    return ret;
}

static image_devmapper_device_info *lookup_device(struct device_set *devset, const char *hash)
{
    image_devmapper_device_info *info = NULL;
    bool res;

    info = metadata_store_get(hash, devset->meta_store);
    if (info == NULL) {
        info = load_metadata(devset, hash);
        if (info == NULL) {
            ERROR("devmapper: Unknown device %s", hash);
            goto out;
        }
        res = metadata_store_add(hash, info, devset->meta_store);
        if (!res) {
            ERROR("devmapper: store device %s failed", hash);
            free_image_devmapper_device_info(info);
            info = NULL;
        }
    }

out:
    return info;
}

static uint64_t get_base_device_size(struct device_set *devset)
{
    uint64_t res;
    image_devmapper_device_info *info = NULL;

    info = lookup_device(devset, "");
    if (info == NULL) {
        return 0;
    }
    res = info->size;
    free_image_devmapper_device_info(info);
    return res;
}

static int device_file_walk(struct device_set *devset)
{
    int ret = 0;
    DIR *dp = NULL;
    struct dirent *entry = NULL;
    struct stat st;
    image_devmapper_device_info *info = NULL;
    char fname[PATH_MAX] = { 0 };
    char *metadir = NULL;

    metadir = metadata_dir(devset);
    if (metadir == NULL) {
        ERROR("Failed to get meta data directory");
        return -1;
    }

    dp = opendir(metadir);
    if (dp == NULL) {
        ERROR("devmapper: open dir %s failed", metadir);
        ret = -1;
        goto out;
    }

    // 路径权限导致stat为非regular文件，误判为dir，此处需优化
    while ((entry = readdir(dp)) != NULL) {
        int pathname_len;

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
            continue;
        }

        (void)memset(fname, 0, sizeof(fname));
        pathname_len = snprintf(fname, PATH_MAX, "%s/%s", metadir, entry->d_name);
        if (pathname_len < 0 || pathname_len >= PATH_MAX) {
            ERROR("Pathname too long");
            continue;
        }

        ret = stat(entry->d_name, &st);
        if (ret != 0) {
            ERROR("devmapper: get %s stat error:%s", fname, strerror(errno));
            goto out;
        }

        if (S_ISDIR(st.st_mode)) {
            DEBUG("devmapper: skipping dir");
            continue;
        }

        if (util_has_suffix(entry->d_name, ".migrated")) {
            DEBUG("devmapper: skipping file %s", entry->d_name);
            continue;
        }

        if (strcmp(entry->d_name, DEVICE_SET_METAFILE) == 0 || strcmp(entry->d_name, TRANSACTION_METADATA) == 0) {
            continue;
        }

        info = lookup_device(devset, entry->d_name); // entry->d_name 取值base  hash值等
        if (info == NULL) {
            ERROR("devmapper: Error looking up device %s", entry->d_name);
            ret = -1;
            goto out;
        }
    }

out:
    if (dp != NULL) {
        closedir(dp);
    }
    free(metadir);
    return ret;
}

static void mark_device_id_used(struct device_set *devset, int device_id)
{
    int mask;
    int value = 0;
    int *value_ptr = NULL;
    int key = device_id / 8;
    bool res;

    mask = 1 << (device_id % 8);

    value_ptr = map_search(devset->device_id_map, &key);
    if (value_ptr == NULL) {
        value = value | mask;
        res = map_insert(devset->device_id_map, &key, &value);
        if (!res) {
            ERROR("devmapper: map insert failed");
            return;
        }
    } else {
        value = *value_ptr | mask;
        res = map_replace(devset->device_id_map, &key, &value);
        if (!res) {
            ERROR("devmapper: map replace failed");
        }
    }
}

static void mark_device_id_free(struct device_set *devset, int device_id)
{
    int mask = 0;
    int value = 0;
    int *value_ptr = NULL;
    int key = device_id / 8;
    bool res;

    mask = ~(1 << (device_id % 8));

    value_ptr = map_search(devset->device_id_map, &key);
    if (value_ptr == NULL) {
        value = value & mask;
        res = map_insert(devset->device_id_map, &key, &value);
        if (!res) {
            ERROR("devmapper: map insert failed");
            return;
        }
    } else {
        value = *value_ptr % mask;
        res = map_replace(devset->device_id_map, &key, &value);
        if (!res) {
            ERROR("devmapper: map replace failed");
        }
    }
    return;
}

static void construct_device_id_map(struct device_set *devset)
{
    char **dev_arr = NULL;
    size_t dev_arr_len = 0;
    size_t i = 0;
    image_devmapper_device_info *info = NULL;

    dev_arr = metadata_store_list_hashes(devset->meta_store);
    dev_arr_len = util_array_len((const char **)dev_arr);

    for (i = 0; i < dev_arr_len; i++) {
        info = metadata_store_get(dev_arr[i], devset->meta_store);
        if (info == NULL) {
            ERROR("devmapper: get device %s from store failed", dev_arr[i]);
            continue;
        }
        mark_device_id_used(devset, info->device_id);
    }
}

static void count_deleted_devices(struct device_set *devset)
{
    char **dev_arr = NULL;
    size_t dev_arr_len = 0;
    size_t i = 0;
    image_devmapper_device_info *info = NULL;

    dev_arr = metadata_store_list_hashes(devset->meta_store);
    dev_arr_len = util_array_len((const char **)dev_arr);

    for (i = 0; i < dev_arr_len; i++) {
        info = metadata_store_get(dev_arr[i], devset->meta_store);
        if (info == NULL) {
            ERROR("devmapper: get device %s from store failed", dev_arr[i]);
            continue;
        }
        if (!info->deleted) {
            continue;
        }
        devset->nr_deleted_devices++;
    }
}

static int rollback_transaction(struct device_set *devset)
{
    return 0;
}

static int process_pending_transaction(struct device_set *devset)
{
    int ret = 0;

    if (devset == NULL || devset->metadata_trans == NULL) {
        ERROR("devmapper: device set or tansaction is NULL");
        return -1;
    }

    // If there was open transaction but pool transaction ID is same
    // as open transaction ID, nothing to roll back.
    if (devset->transaction_id == devset->metadata_trans->open_transaction_id) {
        return 0;
    }

    // If open transaction ID is less than pool transaction ID, something
    // is wrong. Bail out.
    if (devset->transaction_id > devset->metadata_trans->open_transaction_id) {
        WARN("devmapper: Open Transaction id %ld is less than pool transaction id %ld",
             devset->metadata_trans->open_transaction_id, devset->transaction_id);
        return 0;
    }

    // TODO: Pool transaction ID is not same as open transaction. There is
    // a transaction which was not completed.
    ret = rollback_transaction(devset);
    if (ret != 0) {
        ERROR("devmapper: Rolling back open transaction failed");
        return -1;
    }

    devset->metadata_trans->open_transaction_id = devset->transaction_id;

    return ret;
}

static void cleanup_deleted_devices(struct device_set *devset)
{
    int ret = 0;
    char **idsarray = NULL;
    size_t ids_len;
    size_t i = 0;

    // If there are no deleted devices, there is nothing to do.
    if (devset->nr_deleted_devices == 0) {
        return;
    }

    idsarray = metadata_store_list_hashes(devset->meta_store);
    if (idsarray == NULL) {
        ERROR("devmapper: get metadata store list failed");
    }
    ids_len = util_array_len((const char **)idsarray);

    for (; i < ids_len; i++) {
        ret = delete_device(idsarray[i], false, devset);
        if (ret != 0) {
            WARN("devmapper:Deletion of device %s failed", idsarray[i]);
        }
    }

    util_free_array_by_len(idsarray, ids_len);
}

static int init_metadata(struct device_set *devset, const char *pool_name)
{
    int ret = 0;
    uint64_t total_size_in_sectors, transaction_id, data_used;
    uint64_t data_total, metadata_used, metadata_total;

    ret = pool_status(devset, &total_size_in_sectors, &transaction_id, &data_used, &data_total, &metadata_used,
                      &metadata_total);
    if (ret != 0) {
        ERROR("devmapper: get pool %s status failed", pool_name);
        goto out;
    }

    devset->transaction_id = transaction_id;

    ret = device_file_walk(devset);
    if (ret != 0) {
        ERROR("devmapper: Failed to load device files");
        goto out;
    }

    construct_device_id_map(devset);
    count_deleted_devices(devset);
    ret = process_pending_transaction(devset);
    if (ret != 0) {
        ERROR("devmapper: process pending transaction failed");
        goto out;
    }

    cleanup_deleted_devices(devset);

out:
    return ret;
}

static int load_deviceset_metadata(struct device_set *devset)
{
    image_devmapper_deviceset_metadata *deviceset_meta = NULL;
    parser_error err = NULL;
    char *meta_file = NULL;
    int ret = 0;

    meta_file = deviceset_meta_file(devset);
    if (meta_file == NULL) {
        ERROR("Get device metadata file %s full path failed", DEVICE_SET_METAFILE);
        return -1;
    }

    if (!util_file_exists(meta_file)) {
        DEBUG("devmapper: device metadata file %s not exist", DEVICE_SET_METAFILE);
        goto out;
    }

    deviceset_meta = image_devmapper_deviceset_metadata_parse_file(meta_file, NULL, &err);
    if (deviceset_meta == NULL) {
        ERROR("devmapper: load deviceset metadata file error %s", err);
        ret = -1;
        goto out;
    }
    devset->next_device_id = deviceset_meta->next_device_id;
    devset->base_device_filesystem = util_strdup_s(deviceset_meta->base_device_filesystem);
    devset->base_device_uuid = util_strdup_s(deviceset_meta->base_device_uuid);

out:
    free(err);
    free_image_devmapper_deviceset_metadata(deviceset_meta);
    free(meta_file);
    return ret;
}

static bool is_device_id_free(struct device_set *devset, int device_id)
{
    int mask = 0;
    int value = 0;
    int *value_ptr = NULL;
    int key = device_id / 8;

    mask = 1 << (device_id % 8);
    value_ptr = map_search(devset->device_id_map, &key);
    return value_ptr ? (*value_ptr & mask) == 0 : (value & mask) == 0;
}

static void inc_next_device_id(struct device_set *devset)
{
    devset->next_device_id = (devset->next_device_id + 1) & MAX_DEVICE_ID;
}

static int get_next_free_device_id(struct device_set *devset, int *next_id)
{
    int i;
    bool res;

    if (next_id == NULL) {
        return -1;
    }

    inc_next_device_id(devset);
    for (i = 0; i <= MAX_DEVICE_ID; i++) {
        res = is_device_id_free(devset, devset->next_device_id);
        if (res) {
            mark_device_id_used(devset, devset->next_device_id);
            *next_id = devset->next_device_id;
            return 0;
        }
        inc_next_device_id(devset);
    }

    return -1;
}

static int pool_has_free_space(struct device_set *devset)
{
    int ret = 0;
    uint64_t total_size_in_sectors, transaction_id, data_used;
    uint64_t data_total, metadata_used, metadata_total;
    uint64_t min_free_data, data_free, min_free_metadata, metadata_free;

    if (devset->min_free_space_percent == 0) {
        return ret;
    }

    ret = pool_status(devset, &total_size_in_sectors, &transaction_id, &data_used, &data_total, &metadata_used,
                      &metadata_total);
    if (ret != 0) {
        // ERROR();
        goto out;
    }

    min_free_data = (data_total * (uint64_t)devset->min_free_space_percent) / 100;
    if (min_free_data < 1) {
        min_free_data = 1;
    }
    data_free = data_total - data_used;
    if (data_free < min_free_data) {
        ret = -1;
        ERROR("devmapper: Thin Pool has %lu free data blocks which is less than minimum required\
        %lu free data blocks. Create more free space in thin pool or use dm.min_free_space option to change behavior",
              data_total - data_used, min_free_data);
        goto out;
    }

    min_free_metadata = (metadata_total * (uint64_t)devset->min_free_space_percent) / 100;
    if (min_free_metadata < 1) {
        min_free_metadata = 1;
    }

    metadata_free = metadata_total - metadata_used;
    if (metadata_free < min_free_metadata) {
        ret = -1;
        ERROR("devmapper: Thin Pool has %lu free metadata blocks \
        which is less than minimum required %lu free metadata blocks. \
        Create more free metadata space in thin pool or use dm.min_free_space option to change behavior",
              metadata_total - metadata_used, min_free_metadata);
        goto out;
    }

out:
    return ret;
}

static char *metadata_file(struct device_set *devset, const char *hash)
{
    char *file = NULL;
    char *full_path = NULL;
    char *dir = NULL;

    if (hash == NULL) {
        ERROR("devmapper: get metadata file param is null");
        return NULL;
    }

    dir = metadata_dir(devset);
    if (dir == NULL) {
        ERROR("devmapper: get metadata dir of device %s failed", hash);
        return NULL;
    }

    file = util_strdup_s(hash);
    if (file == NULL) {
        goto out;
    }

    full_path = util_path_join(dir, file);

out:
    free(dir);
    free(file);
    return full_path;
}

static int save_metadata(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *metadata_json = NULL;
    char *fname = NULL;
    parser_error err = NULL;

    if (info == NULL) {
        ERROR("devmapper: input param is null");
        return -1;
    }

    fname = metadata_file(devset, info->hash);
    if (fname == NULL) {
        ret = -1;
        ERROR("devmapper: get device %s metadata file full path failed", info->hash);
        goto out;
    }

    metadata_json = image_devmapper_device_info_generate_json(info, NULL, &err);
    if (metadata_json == NULL) {
        ret = -1;
        ERROR("devmapper: generate metadata json error %s", err);
        goto out;
    }

    if (util_write_file(fname, metadata_json, strlen(metadata_json), DEFAULT_SECURE_FILE_MODE) != 0) {
        ret = -1;
        ERROR("failed write process.json");
        goto out;
    }

out:
    UTIL_FREE_AND_SET_NULL(err);
    UTIL_FREE_AND_SET_NULL(metadata_json);
    UTIL_FREE_AND_SET_NULL(fname);
    return ret;
}

static int save_transaction_metadata(struct device_set *devset)
{
    image_devmapper_transaction *trans = NULL;
    char *trans_json = NULL;
    char fname[PATH_MAX] = { 0 };
    parser_error err = NULL;
    int ret = 0;

    ret = snprintf(fname, sizeof(fname), "%s/metadata/%s", devset->root, TRANSACTION_METADATA);
    if (ret < 0 || (size_t)ret >= sizeof(fname)) {
        ERROR("devmapper: failed make transaction-metadata full path");
        return -1;
    }

    trans = devset->metadata_trans;
    trans_json = image_devmapper_transaction_generate_json(trans, NULL, &err);
    if (trans_json == NULL) {
        ret = -1;
        ERROR("devmapper: generate transaction json error %s", err);
        goto out;
    }

    if (util_write_file(fname, trans_json, strlen(trans_json), DEFAULT_SECURE_FILE_MODE) != 0) {
        ret = -1;
        ERROR("failed write process.json");
        goto out;
    }
    ret = 0;
out:
    UTIL_FREE_AND_SET_NULL(err);
    UTIL_FREE_AND_SET_NULL(trans_json);
    return ret;
}

static int save_deviceset_matadata(struct device_set *devset)
{
    int ret = 0;
    image_devmapper_deviceset_metadata *devset_metadata = NULL;
    char *metadata_json = NULL;
    char *fname = NULL;
    parser_error err = NULL;

    fname = deviceset_meta_file(devset);
    if (fname == NULL) {
        ret = -1;
        ERROR("devmapper: get deviceset metadata file full path failed");
        goto free_out;
    }

    devset_metadata = util_common_calloc_s(sizeof(image_devmapper_deviceset_metadata));
    if (devset_metadata == NULL) {
        ret = -1;
        ERROR("devmapper: Out of memory");
        goto free_out;
    }

    devset_metadata->base_device_filesystem = util_strdup_s(devset->base_device_filesystem);
    devset_metadata->base_device_uuid = util_strdup_s(devset->base_device_uuid);
    devset_metadata->next_device_id = devset->next_device_id;

    metadata_json = image_devmapper_deviceset_metadata_generate_json(devset_metadata, NULL, &err);
    if (metadata_json == NULL) {
        ret = -1;
        ERROR("devmapper: generate deviceset metadata json error %s", err);
        goto free_out;
    }

    if (util_write_file(fname, metadata_json, strlen(metadata_json), DEFAULT_SECURE_FILE_MODE) != 0) {
        ret = -1;
        ERROR("failed write process.json");
        goto free_out;
    }

free_out:
    free_image_devmapper_deviceset_metadata(devset_metadata);
    UTIL_FREE_AND_SET_NULL(err);
    UTIL_FREE_AND_SET_NULL(metadata_json);
    UTIL_FREE_AND_SET_NULL(fname);
    return ret;
}

static int open_transaction(struct device_set *devset, const char *hash, int id)
{
    int ret = 0;

    if (devset->metadata_trans == NULL || hash == NULL) {
        ERROR("devmapper: open transaction params null");
        return -1;
    }
    devset->metadata_trans->open_transaction_id = devset->transaction_id + 1;
    devset->metadata_trans->device_hash = util_strdup_s(hash);
    devset->metadata_trans->device_id = id;

    ret = save_transaction_metadata(devset);
    if (ret != 0) {
        ret = -1;
        ERROR("devmapper: Error saving transaction metadata");
    }

    return ret;
}

static int refresh_transaction(struct device_set *devset, int id)
{
    int ret = 0;

    if (devset->metadata_trans == NULL) {
        ERROR("devmapper: refresh transaction params null");
        return -1;
    }

    devset->metadata_trans->device_id = id;
    ret = save_transaction_metadata(devset);
    if (ret != 0) {
        ret = -1;
        ERROR("devmapper: Error saving transaction metadata");
    }

    return ret;
}

static int update_pool_transaction_id(struct device_set *devset)
{
    int ret = 0;
    char *pool_name = NULL;

    pool_name = get_pool_dev_name(devset);
    if (pool_name == NULL) {
        ret = -1;
        goto out;
    }
    ret = dev_set_transaction_id(pool_name, devset->transaction_id, devset->metadata_trans->open_transaction_id);
    if (ret != 0) {
        goto out;
    }

    devset->transaction_id = devset->metadata_trans->open_transaction_id;
out:
    UTIL_FREE_AND_SET_NULL(pool_name);
    return ret;
}

static int close_transaction(struct device_set *devset)
{
    int ret = 0;

    ret = update_pool_transaction_id(devset);
    if (ret != 0) {
        DEBUG("devmapper: Failed to close Transaction");
    }

    return ret;
}

static int remove_metadata(struct device_set *devset, const char *hash)
{
    int ret = 0;
    char *fname = NULL;

    fname = metadata_file(devset, hash);
    if (fname == NULL) {
        // ERROR();
        return -1;
    }

    ret = util_path_remove(fname);
    if (ret != 0) {
        ERROR("devmapper: remove metadata file %s failed", hash);
    }

    return ret;
}

static int unregister_device(struct device_set *devset, const char *hash)
{
    int ret = 0;

    ret = metadata_store_remove(hash, devset->meta_store);
    if (ret != 0) {
        ret = -1;
        ERROR("devmapper: remove metadata store %s failed", hash);
    }

    ret = remove_metadata(devset, hash);
    if (ret != 0) {
        ret = -1;
        ERROR("devmapper: remove metadata file %s failed", hash);
    }

    return ret;
}

static image_devmapper_device_info *register_device(struct device_set *devset, int id, const char *hash, uint64_t size,
                                                    uint64_t transaction_id)
{
    int ret = 0;
    bool store_res = false;
    image_devmapper_device_info *info = NULL;

    info = util_common_calloc_s(sizeof(image_devmapper_device_info));
    if (info == NULL) {
        ERROR("devmapper: Out of memory");
        return NULL;
    }

    info->device_id = id;
    info->size = size;
    info->transaction_id = transaction_id;
    info->initialized = false;
    info->hash = util_strdup_s(hash);
    info->deleted = false;

    store_res = metadata_store_add(hash, info, devset->meta_store);
    if (!store_res) {
        ERROR("devmapper: metadata store add failed hash %s", hash);
        free_image_devmapper_device_info(info);
        goto out;
    }

    ret = save_metadata(devset, info);
    if (ret != 0) {
        ERROR("devmapper: save metadata of device %s failed", hash);
        if (!metadata_store_remove(hash, devset->meta_store)) {
            ERROR("devmapper: metadata file %s store remove failed", hash);
        }
        goto out;
    }

    return info;
out:
    return NULL;
}

static image_devmapper_device_info *create_register_device(struct device_set *devset, const char *hash)
{
    int ret = 0;
    int device_id = 0;
    char *pool_dev = NULL;
    image_devmapper_device_info *info = NULL;

    ret = get_next_free_device_id(devset, &device_id);
    if (ret != 0) {
        ERROR("devmapper: cannot get next free device id");
        return NULL;
    }

    ret = open_transaction(devset, hash, device_id);
    if (ret != 0) {
        ERROR("devmapper: Error opening transaction hash = %s deviceID = %d", hash, device_id);
        mark_device_id_free(devset, device_id);
        goto out;
    }
    pool_dev = get_pool_dev_name(devset);
    if (pool_dev == NULL) {
        ERROR("devmapper: get pool device name failed");
        goto out;
    }

    do {
        ret = dev_create_device(pool_dev, device_id);
        if (ret != 0) {
            // TODO: 如果错误类型为device id exists
            // Device ID already exists. This should not
            // happen. Now we have a mechanism to find
            // a free device ID. So something is not right.
            // Give a warning and continue.
            if (ret == ERR_DEVICE_ID_EXISTS) {
                ERROR("devmapper: device id %d exists in pool but it is supposed to be unused", device_id);
                ret = get_next_free_device_id(devset, &device_id);
                if (ret != 0) {
                    ERROR("devmapper: cannot get next free device id");
                    goto out;
                }
                ret = refresh_transaction(devset, device_id);
                if (ret != 0) {
                    ERROR("devmapper: Error refres open transaction deviceID %s = %d", hash, device_id);
                }
                continue;
            }
            mark_device_id_free(devset, device_id);
            goto out;
        }
        break;
    } while (true);

    info = register_device(devset, device_id, hash, devset->base_fs_size, devset->metadata_trans->open_transaction_id);
    if (info == NULL) {
        ERROR("devmapper: register device %d failed, start to delete device", device_id);
        ret = dev_delete_device(pool_dev, device_id);
        mark_device_id_free(devset, device_id);
    }

    ret = close_transaction(devset);
    if (ret != 0) {
        ret = unregister_device(devset, hash);
        if (ret != 0) {
            ERROR("devmapper: unregister device %s failed", hash);
        }

        ret = dev_delete_device(pool_dev, device_id);
        if (ret != 0) {
            ERROR("devmapper: delete device with hash:%s, device id:%d failed", hash, device_id);
        }

        mark_device_id_free(devset, device_id);
        // free_image_devmapper_device_info(info);
        // info = NULL;
    }

out:
    UTIL_FREE_AND_SET_NULL(pool_dev);
    return info;
}

static int create_register_snap_device(struct device_set *devset, image_devmapper_device_info *base_info,
                                       const char *hash, uint64_t size)
{
    int ret = 0;
    int device_id = 0;
    char *pool_dev = NULL;
    image_devmapper_device_info *info = NULL;

    ret = get_next_free_device_id(devset, &device_id);
    if (ret != 0) {
        ERROR("devmapper: cannot get next free device id");
        return ret;
    }

    ret = open_transaction(devset, hash, device_id);
    if (ret != 0) {
        ERROR("devmapper: Error opening transaction hash = %s deviceID = %d", hash, device_id);
        mark_device_id_free(devset, device_id);
        return ret;
    }
    pool_dev = get_pool_dev_name(devset);
    if (pool_dev == NULL) {
        ERROR("devmapper: get pool device name failed");
        goto out;
    }

    do {
        ret = dev_create_snap_device_raw(pool_dev, device_id, base_info->device_id);
        if (ret != 0) {
            // TODO: 如果错误类型为device id exists
            // Device ID already exists. This should not
            // happen. Now we have a mechanism to find
            // a free device ID. So something is not right.
            // Give a warning and continue.
            if (ret == ERR_DEVICE_ID_EXISTS) {
                ret = get_next_free_device_id(devset, &device_id);
                if (ret != 0) {
                    ERROR("devmapper: cannot get next free device id");
                    goto out;
                }
                ret = refresh_transaction(devset, device_id);
                if (ret != 0) {
                    ERROR("devmapper: Error refresh open transaction deviceID %s = %d", hash, device_id);
                    goto out;
                }
                continue;
            }
            DEBUG("devmapper: error creating snap device");
            mark_device_id_free(devset, device_id);
            goto out;
        }
        break;
    } while (true);

    info = register_device(devset, device_id, hash, devset->base_fs_size, devset->metadata_trans->open_transaction_id);
    if (info == NULL) {
        DEBUG("devmapper: Error registering device");
        (void)dev_delete_device(pool_dev, device_id);
        ret = -1;
        mark_device_id_free(devset, device_id);
    }

    ret = close_transaction(devset);
    if (ret != 0) {
        (void)unregister_device(devset, hash);
        (void)dev_delete_device(pool_dev, device_id);
        mark_device_id_free(devset, device_id);
        goto out;
    }

out:
    UTIL_FREE_AND_SET_NULL(pool_dev);
    return ret;
}

static int cancel_deferred_removal(struct device_set *devset, const char *hash)
{
    int i = 0;
    int ret = 0;
    int retries = 100;
    char *dm_name = NULL;

    dm_name = get_dm_name(devset, hash);
    if (dm_name == NULL) {
        ERROR("devmapper: get dm device name failed");
        return -1;
    }

    for (; i < retries; i++) {
        ret = dev_cancel_deferred_remove(dm_name);
        if (ret != 0) {
            if (ret != ERR_BUSY) {
                sleep(0.1);
                continue;
            }
        }
        break;
    }

    UTIL_FREE_AND_SET_NULL(dm_name);
    return ret;
}

static int take_snapshot(struct device_set *devset, const char *hash, image_devmapper_device_info *base_info,
                         uint64_t size)
{
    int ret = 0;
    struct dm_info *dmi = NULL;
    char *dm_name = NULL;
    bool resume_dev = false;
    bool deactive_dev = false;

    dmi = util_common_calloc_s(sizeof(struct dm_info));
    if (dmi == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    dm_name = get_dm_name(devset, base_info->hash);
    if (dm_name == NULL) {
        ret = -1;
        goto out;
    }

    ret = pool_has_free_space(devset);
    if (ret != 0) {
        goto out;
    }

    if (devset->deferred_remove) {
        ret = dev_get_info_with_deferred(dm_name, dmi);
        if (ret != 0) {
            goto out;
        }

        if (dmi->deferred_remove != 0) {
            ret = cancel_deferred_removal(devset, base_info->hash);
            if (ret != 0) {
                if (ret != ERR_ENXIO) {
                    goto out;
                }
                free(dmi);
                dmi = NULL;
            } else {
                deactive_dev = true;
            }
        }
    } else {
        ret = dev_get_info(dmi, dm_name);
        if (ret != 0) {
            goto out;
        }
    }

    if (dmi != NULL && dmi->exists != 0) {
        if (dev_suspend_device(dm_name) != 0) {
            ret = -1;
            goto out;
        }
        resume_dev = true;
    }

    ret = create_register_snap_device(devset, base_info, hash, size);
    if (ret != 0) {
        ERROR("devmapper: creat snap device from device %s failed", hash);
    }

out:
    if (deactive_dev) {
        (void)deactivate_device(devset, base_info);
    }

    if (resume_dev) {
        (void)dev_resume_device(dm_name);
    }
    UTIL_FREE_AND_SET_NULL(dm_name);
    return ret;
}

static int cancel_deferred_removal_if_needed(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *dm_name = NULL;
    struct dm_info dmi = { 0 };

    if (!devset->deferred_remove == 0) {
        ERROR("devmapper: cancel deferred remove input param null");
        return ret;
    }

    dm_name = get_dm_name(devset, info->hash);
    if (dm_name == NULL) {
        ERROR("devmapper: get dm device name failed");
        goto out;
    }

    DEBUG("devmapper: cancelDeferredRemovalIfNeeded START(%s)", dm_name);

    ret = dev_get_info_with_deferred(dm_name, &dmi);
    if (ret != 0) {
        ERROR("devmapper: can not get info from dm %s", dm_name);
        goto out;
    }

    if (dmi.deferred_remove == 0) {
        ret = 0;
        goto out;
    }

    ret = cancel_deferred_removal(devset, info->hash);
    if (ret != 0 && ret != ERR_BUSY) {
        // If Error is ErrEnxio. Device is probably already gone. Continue.
        goto out;
    }
    ret = 0;

out:
    UTIL_FREE_AND_SET_NULL(dm_name);
    return ret;
}

static int activate_device_if_needed(struct device_set *devset, image_devmapper_device_info *info, bool ignore_deleted)
{
    int ret = 0;
    struct dm_info dinfo = { 0 };
    char *dm_name = NULL;
    char *pool_dev_name = NULL;

    if (info->deleted && !ignore_deleted) {
        ERROR("devmapper: Can't activate device %s as it is marked for deletion", info->hash);
        return -1;
    }

    ret = cancel_deferred_removal_if_needed(devset, info);
    if (ret != 0) {
        ERROR("devmapper: Device Deferred Removal Cancellation Failed");
        return ret;
    }

    dm_name = get_dm_name(devset, info->hash);
    if (dm_name == NULL) {
        ERROR("devmapper: get dm device name failed");
        return -1;
    }

    ret = dev_get_info(&dinfo, dm_name);
    if (ret != 0) {
        ERROR("devmapper: get device info failed");
        goto out;
    }

    if (dinfo.exists != 0) {
        ret = 0;
        goto out;
    }

    pool_dev_name = get_pool_dev_name(devset);
    if (pool_dev_name == NULL) {
        ERROR("devmapper: get pool dev name failed");
        ret = -1;
        goto out;
    }

    ret = dev_active_device(pool_dev_name, dm_name, info->device_id, info->size);
    if (ret != 0) {
        ERROR("devmapper: active device with hash:%d, id:%s, failed", info->device_id, info->hash);
    }

out:
    UTIL_FREE_AND_SET_NULL(dm_name);
    UTIL_FREE_AND_SET_NULL(pool_dev_name);
    return ret;
}

static int save_base_device_uuid(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *base_dev_uuid = NULL;
    char *dev_fname = NULL;
    char *dm_name = NULL;

    ret = activate_device_if_needed(devset, info, false);
    if (ret != 0) {
        ERROR("devmapper: activate device %s failed", info->hash);
        return ret;
    }

    dm_name = get_dm_name(devset, info->hash);
    if (dm_name == NULL) {
        ret = -1;
        ERROR("devmapper: get dm name failed");
        goto free_out;
    }
    dev_fname = get_dev_name(dm_name);

    base_dev_uuid = get_device_uuid(dev_fname);
    if (base_dev_uuid == NULL) {
        ret = -1;
        ERROR("devmapper: get base dev %s uuid failed", dev_fname);
        goto free_out;
    }

    devset->base_device_uuid = util_strdup_s(base_dev_uuid);

    ret = save_deviceset_matadata(devset);
    if (ret != 0) {
        ERROR("devmapper: save deviceset metadata failed");
        goto free_out;
    }

free_out:
    deactivate_device(devset, info);
    UTIL_FREE_AND_SET_NULL(dm_name);
    UTIL_FREE_AND_SET_NULL(dev_fname);
    UTIL_FREE_AND_SET_NULL(base_dev_uuid);
    return ret;
}

static void run_mkfs_ext4(void *args)
{
    char **tmp_args = (char **)args;
    size_t CMD_ARGS_NUM = 4;

    if (util_array_len((const char **)tmp_args) != CMD_ARGS_NUM) {
        COMMAND_ERROR("mkfs.ext4 need four args");
        exit(1);
    }

    execvp(tmp_args[0], tmp_args);
}

static int save_base_device_filesystem(struct device_set *devset, const char *fs)
{
    free(devset->base_device_filesystem);
    devset->base_device_filesystem = util_strdup_s(fs);
    return save_deviceset_matadata(devset);
}

static int create_file_system(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *dev_fname = NULL;
    char **args = NULL;
    char *stdout = NULL;
    char *stderr = NULL;

    dev_fname = dev_name(devset, info);
    if (dev_fname == NULL) {
        ret = -1;
        goto out;
    }

    if (!util_valid_str(devset->filesystem)) {
        free(devset->filesystem);
        devset->filesystem = util_strdup_s("ext4");
    }

    ret = save_base_device_filesystem(devset, devset->filesystem);
    if (ret != 0) {
        goto out;
    }

    // Only support ext4 fs type
    if (strcmp(devset->filesystem, "ext4") != 0) {
        ERROR("devmapper: Unsupported filesystem type %s", devset->filesystem);
        ret = -1;
        goto out;
    }

    args = (char **)util_common_calloc_s(sizeof(char *) * 5);
    if (args == NULL) {
        ERROR("devmapper: out of memory");
        ret = -1;
        goto out;
    }

    args[0] = util_strdup_s("mkfs.ext4");
    args[1] = util_strdup_s("-E");
    args[2] = util_strdup_s("nodiscard,lazy_itable_init=0,lazy_journal_init=0");
    args[3] = util_strdup_s(dev_fname);
    if (!util_exec_cmd(run_mkfs_ext4, args, NULL, &stdout, &stderr)) {
        ERROR("Unexpected command output %s with error: %s", stdout, stderr);
        ret = -1;
    }

out:
    util_free_array(args);
    UTIL_FREE_AND_SET_NULL(stdout);
    UTIL_FREE_AND_SET_NULL(stderr);
    UTIL_FREE_AND_SET_NULL(dev_fname);
    return ret;
}

static int create_base_image(struct device_set *devset)
{
    int ret = 0;
    image_devmapper_device_info *info = NULL;

    // create initial device
    info = create_register_device(devset, "base");
    if (info == NULL) {
        ERROR("devmapper: create and register base device failed");
        return -1;
    }

    DEBUG("devmapper: Creating filesystem on base device-mapper thin volume");

    ret = activate_device_if_needed(devset, info, false);
    if (ret != 0) {
        ERROR("devmapper: activate device %s failed", info->hash);
        ret = -1;
        goto out;
    }

    ret = create_file_system(devset, info);
    if (ret != 0) {
        ERROR("devmapper: create file system for base dev failed");
        goto out;
    }

    info->initialized = true;

    ret = save_metadata(devset, info);
    if (ret != 0) {
        ERROR("devmapper: save metadata for device %s failed", info->hash);
        info->initialized = false;
        goto out;
    }

    ret = save_base_device_uuid(devset, info);
    if (ret != 0) {
        ERROR("devmapper: Could not query and save base device UUID");
    }

out:
    return ret;
}

static int check_thin_pool(struct device_set *devset)
{
    uint64_t total_size_in_sectors, transaction_id, data_used;
    uint64_t data_total, metadata_used, metadata_total;
    int ret = 0;

    ret = pool_status(devset, &total_size_in_sectors, &transaction_id, &data_used, &data_total, &metadata_used,
                      &metadata_total);
    if (ret != 0) {
        ERROR("devmapper: get pool status failed");
        goto out;
    }

    if (data_used != 0) {
        ERROR("devmapper: Unable to take ownership of thin-pool (%s) that already has used data blocks",
              devset->thin_pool_device);
        ret = -1;
        goto out;
    }

    if (transaction_id != 0) {
        ERROR("devmapper: Unable to take ownership of thin-pool (%s) with non-zero transaction ID",
              devset->thin_pool_device);
        ret = -1;
        goto out;
    }

    DEBUG("devmapper:total_size_in_sectors:%lu, data_total:%lu, metadata_used:%lu, metadata_total:%lu",
          total_size_in_sectors, data_total, metadata_used, metadata_total);

out:
    return ret;
}

static int verify_base_device_uuidfs(struct device_set *devset, image_devmapper_device_info *base_info)
{
    int ret = 0;
    char *dm_name = NULL;
    char *dev_fname = NULL;
    char *uuid = NULL;
    char *fs_type = NULL;

    ret = activate_device_if_needed(devset, base_info, false);
    if (ret != 0) {
        ERROR("devmapper: activate device %s failed", base_info->hash);
        return ret;
    }

    dm_name = get_dm_name(devset, base_info->hash);
    if (dm_name == NULL) {
        ret = -1;
        ERROR("devmapper: get dm name failed");
        goto free_out;
    }
    dev_fname = get_dev_name(dm_name);

    uuid = get_device_uuid(dev_fname);
    if (uuid == NULL) {
        ret = -1;
        ERROR("devmapper: get uuid err from device %s", dev_fname);
        goto free_out;
    }

    if (strcmp(devset->base_device_uuid, uuid) != 0) {
        ERROR("devmapper: Current Base Device UUID:%s does not match with stored UUID:%s. \
        Possibly using a different thin pool than last invocation",
              uuid, devset->base_device_uuid);
        goto free_out;
    }

    if (devset->base_device_filesystem == NULL) {
        fs_type = probe_fs_type(dev_fname);
        if (fs_type == NULL) {
            goto free_out;
        }

        ret = save_base_device_filesystem(devset, fs_type);
        if (ret != 0) {
            goto free_out;
        }
    }

    if (devset->filesystem == NULL || strcmp(devset->base_device_filesystem, devset->filesystem) != 0) {
        WARN("devmapper: Base device already exists and has filesystem %s on it. \
        User specified filesystem %s will be ignored.",
             devset->base_device_filesystem, devset->filesystem);
        devset->filesystem = util_strdup_s(devset->base_device_filesystem);
    }

free_out:
    deactivate_device(devset, base_info);
    UTIL_FREE_AND_SET_NULL(dm_name);
    UTIL_FREE_AND_SET_NULL(dev_fname);
    UTIL_FREE_AND_SET_NULL(uuid);
    UTIL_FREE_AND_SET_NULL(fs_type);
    return ret;
}

static int setup_verify_baseimages_uuidfs(struct device_set *devset, image_devmapper_device_info *base_info)
{
    int ret = 0;

    if (base_info == NULL) {
        return -1;
    }

    // If BaseDeviceUUID is nil (upgrade case), save it and return success.
    if (devset->base_device_uuid == NULL) {
        ret = save_base_device_uuid(devset, base_info);
        if (ret != 0) {
            ERROR("devmapper: Could not query and save base device UUID");
        }
        return ret;
    }

    ret = verify_base_device_uuidfs(devset, base_info);
    if (ret != 0) {
        ERROR("devmapper: Base Device UUID and Filesystem verification failed");
    }
    return ret;
}

// 对未挂载的文件系统扩容或者在线扩容，需要内核支持此功能
static int grow_fs(struct device_set *devset, image_devmapper_device_info *info)
{
#define FS_MOUNT_POINT "/run/containers/storage/mnt"
    int ret = 0;
    char *mount_opt = NULL;
    char *pool_name = NULL;
    char *dev_fname = NULL;

    if (activate_device_if_needed(devset, info, false) != 0) {
        ERROR("devmapper:error activating devmapper device %s", info->hash);
        return -1;
    }

    if (!util_dir_exists(FS_MOUNT_POINT)) {
        ret = util_mkdir_p(FS_MOUNT_POINT, DEFAULT_DEVICE_SET_MODE);
        if (ret != 0) {
            goto free_out;
        }
    }

    if (strcmp(devset->base_device_filesystem, "xfs") == 0) {
        append_mount_options(&mount_opt, "nouuid");
    }
    append_mount_options(&mount_opt, devset->mount_options);

    pool_name = get_pool_name(devset);
    dev_fname = get_dev_name(pool_name);
    if (dev_fname == NULL) {
        ERROR("devmapper: pool device name is NULL");
        goto free_out;
    }

    ret = util_mount(dev_fname, FS_MOUNT_POINT, devset->base_device_filesystem, mount_opt);
    if (ret != 0) {
        ERROR("Error mounting '%s' on '%s' ", dev_fname, FS_MOUNT_POINT);
        goto free_out;
    }

    if (strcmp(devset->base_device_filesystem, "ext4") == 0) {
        if (exec_grow_fs_command("resize2fs", dev_fname) != 0) {
            ERROR("Failed execute resize2fs to grow rootfs");
        }
    } else if (strcmp(devset->base_device_filesystem, "xfs") == 0) {
        if (exec_grow_fs_command("xfs_growfs", dev_fname) != 0) {
            ERROR("Failed execute xfs_growfs to grow rootfs");
        }
    } else {
        ERROR("Unsupported filesystem type %s", devset->base_device_filesystem);
    }

    ret = umount2(FS_MOUNT_POINT, MNT_DETACH);
    if (ret < 0 && errno != EINVAL) {
        WARN("Failed to umount directory %s:%s", FS_MOUNT_POINT, strerror(errno));
    }

free_out:
    deactivate_device(devset, info);
    UTIL_FREE_AND_SET_NULL(pool_name);
    UTIL_FREE_AND_SET_NULL(dev_fname);
    UTIL_FREE_AND_SET_NULL(mount_opt);
    return ret;
}

static int check_grow_base_device_fs(struct device_set *devset, image_devmapper_device_info *base_info)
{
    int ret = 0;
    uint64_t base_dev_size;

    if (!devset->user_base_size) {
        return ret;
    }

    base_dev_size = get_base_device_size(devset);

    if (devset->base_fs_size < base_dev_size) {
        ERROR("devmapper: Base fs size cannot be smaller than %ld", base_dev_size);
        return -1;
    }

    if (devset->base_fs_size == base_dev_size) {
        return 0;
    }

    base_info->size = devset->base_fs_size;

    ret = save_metadata(devset, base_info);
    if (ret != 0) {
        // Try to remove unused device

        if (!metadata_store_remove(base_info->hash, devset->meta_store)) {
            ERROR("devmapper: remove unused device from store failed");
        }
        return -1;
    }
    return grow_fs(devset, base_info);
}

static int mark_for_deferred_deletion(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;

    if (info->deleted) {
        return ret;
    }

    info->deleted = true;

    ret = save_metadata(devset, info);
    if (ret != 0) {
        info->deleted = false;
        return ret;
    }
    devset->nr_deleted_devices++;
    return 0;
}

static int delete_transaction(struct device_set *devset, image_devmapper_device_info *info, bool sync_delete)
{
    int ret = 0;
    char *pool_fname = NULL;

    ret = open_transaction(devset, info->hash, info->device_id);
    if (ret != 0) {
        return -1;
    }

    pool_fname = get_pool_dev_name(devset);
    ret = dev_delete_device(pool_fname, info->device_id);
    if (ret != 0) {
        // If syncDelete is true, we want to return error. If deferred
        // deletion is not enabled, we return an error. If error is
        // something other then EBUSY, return an error.
        if (sync_delete || !devset->deferred_delete || ret == ERR_BUSY) {
            DEBUG("devmapper: Error deleting device");
            return ret;
        }
    }

    if (ret == 0) {
        ret = unregister_device(devset, info->hash);
        if (ret != 0) {
            goto out;
        }
        if (info->deleted) {
            devset->nr_deleted_devices--;
        }
        mark_device_id_free(devset, info->device_id);
    } else {
        ret = mark_for_deferred_deletion(devset, info);
        if (ret != 0) {
            goto out;
        }
    }

out:
    UTIL_FREE_AND_SET_NULL(pool_fname);
    return 0;
}

// Issue discard only if device open count is zero.
static void issue_discard(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    struct dm_info dinfo;
    char *dm_name = NULL;
    char *dev_fname = NULL;

    ret = activate_device_if_needed(devset, info, true);
    if (ret != 0) {
        ERROR("devmapper: activate device %s failed", info->hash);
        goto free_out;
    }

    dm_name = get_dm_name(devset, info->hash);
    if (dm_name == NULL) {
        goto free_out;
    }

    ret = dev_get_info(&dinfo, dm_name);
    if (ret != 0) {
        goto free_out;
    }

    if (dinfo.open_count != 0) {
        DEBUG("devmapper: Device: %s is in use. OpenCount=%d. Not issuing discards.", info->hash, dinfo.open_count);
        goto free_out;
    }

    dev_fname = get_dev_name(dm_name);
    if (dev_fname == NULL) {
        goto free_out;
    }

    ret = dev_block_device_discard(dev_fname);

free_out:
    UTIL_FREE_AND_SET_NULL(dm_name);
    UTIL_FREE_AND_SET_NULL(dev_fname);
}

static int do_delete_device(struct device_set *devset, const char *hash, bool sync_delete)
{
    int ret = 0;
    bool deferred_remove;
    image_devmapper_device_info *info = NULL;

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ERROR("devmapper: lookup device failed");
        return -1;
    }
    if (devset->do_blk_discard) {
        issue_discard(devset, info);
    }

    deferred_remove = devset->deferred_remove;
    if (!devset->deferred_delete) {
        deferred_remove = false;
    }

    ret = deactivate_device_mode(devset, info, deferred_remove);
    if (ret != 0) {
        ERROR("devmapper: Error deactivating device");
        goto free_out;
    }

    ret = delete_transaction(devset, info, sync_delete);
    if (ret != 0) {
        goto free_out;
    }

free_out:
    return ret;
}

static int setup_base_image(struct device_set *devset)
{
    int ret = 0;
    image_devmapper_device_info *old_info = NULL;

    old_info = lookup_device(devset, "base");

    // base image already exists. If it is initialized properly, do UUID
    // verification and return. Otherwise remove image and set it up
    // fresh.
    if (old_info != NULL) {
        if (old_info->initialized && !old_info->deleted) {
            ret = setup_verify_baseimages_uuidfs(devset, old_info);
            if (ret != 0) {
                ERROR("devmapper: do base image uuid verification failed");
                goto out;
            }

            ret = check_grow_base_device_fs(devset, old_info);
            if (ret != 0) {
                ERROR("devmapper: grow base device fs failed");
            }
            goto out;
        }

        ret = do_delete_device(devset, "base", true);
        if (ret != 0) {
            ERROR("devmapper: remove uninitialized base image failed");
            goto out;
        }
    }

    // If we are setting up base image for the first time, make sure
    // thin pool is empty.
    if (util_valid_str(devset->thin_pool_device) && old_info == NULL) {
        ret = check_thin_pool(devset);
        if (ret != 0) {
            ERROR("devmapper: check thin pool failed");
            goto out;
        }
    }

    ret = create_base_image(devset);
    if (ret != 0) {
        ERROR("devmapper: create base image failed");
    }

out:
    return ret;
}

static int do_devmapper_init(struct device_set *devset)
{
    int ret = 0;
    bool support = false;
    char *metadata_path = NULL;
    struct stat st;
    char prefix[PATH_MAX] = { 0 };
    char device_path[PATH_MAX] = { 0 };
    char **devices_list = NULL;
    size_t devices_len = 0;
    uint64_t start, length;
    char *target_type = NULL;
    char *params = NULL;
    bool pool_exist = false;
    char *pool_name = NULL;
    size_t i = 0;

    ret = enable_deferred_removal_deletion(devset);
    if (ret != 0) {
        ERROR("devmapper: enable deferred remove failed");
        return -1;
    }

    support = udev_set_sync_support(true);
    if (!support) {
        ERROR("devmapper: Udev sync is not supported. This will lead to data loss and unexpected behavior.");
        if (!devset->override_udev_sync_check) {
            ERROR("devmapper: driver do not support udev sync");
            return -1;
        }
    }

    ret = util_mkdir_p(devset->root, DEFAULT_DEVICE_SET_MODE);
    if (ret != 0) {
        ERROR("mkdir path %s failed", devset->root);
        return -1;
    }

    metadata_path = metadata_dir(devset);
    ret = util_mkdir_p(metadata_path, DEFAULT_DEVICE_SET_MODE);
    if (ret != 0) {
        ERROR("mkdir path %s failed", metadata_path);
        goto out;
    }

    ret = stat(devset->root, &st);
    if (ret < 0) {
        ERROR("devmapper: Error looking up dir %s", devset->root);
        goto out;
    }
    ret = snprintf(prefix, sizeof(prefix), "container-%u:%u-%u", major(st.st_dev), minor(st.st_dev),
                   (unsigned int)st.st_ino);
    if (ret < 0 || (size_t)ret >= sizeof(prefix)) {
        ERROR("Failed to sprintf device prefix");
        goto out;
    }
    devset->device_prefix = util_strdup_s(prefix);

    ret = dev_get_device_list(&devices_list, &devices_len);
    if (ret != 0) {
        DEBUG("devicemapper: failed to get device list");
    }

    for (i = 0; i < devices_len; i++) {
        if (!util_has_prefix(*(devices_list + i), devset->device_prefix)) {
            continue;
        }
        ret = dev_get_status(&start, &length, &target_type, &params, *(devices_list + i));
        if (ret != 0) {
            WARN("devmapper: get device status %s failed", *(devices_list + i));
            continue;
        }
        // remove broken device
        if (length == 0) {
            ret = dev_remove_device(*(devices_list + i));
            if (ret != 0) {
                WARN("devmapper: remove broken device %s failed", *(devices_list + i));
            }
            DEBUG("devmapper: remove broken device: %s", *(devices_list + i));
        }
        (void)memset(device_path, 0, sizeof(device_path));
        (void)snprintf(device_path, sizeof(device_path), "/dev/mapper/%s", *(devices_list + i));
        if (stat(device_path, &st)) {
            ret = dev_remove_device(*(devices_list + i));
            if (ret != 0) {
                WARN("devmapper: remove incompelete device %s", *(devices_list + i));
            }
            DEBUG("devmapper: remove incompelete device: %s", *(devices_list + i));
        }
    }

    // Check for the existence of the thin-pool device
    pool_name = get_pool_name(devset);
    if (pool_name == NULL) {
        ERROR("devmapper: pool name is null");
        goto out;
    }
    pool_exist = thin_pool_exists(devset, pool_name);

    if (!pool_exist || !util_valid_str(devset->thin_pool_device)) {
        ERROR("devmapper: thin pool is not exist or caller did not pass us a pool, please create it firstly");
        goto out;
    }

    ret = init_metadata(devset, pool_name);
    if (ret != 0) {
        ERROR("devmapper: init metadata failed");
        goto out;
    }

    // Right now this loads only NextDeviceID. If there is more metadata
    // down the line, we might have to move it earlier.
    ret = load_deviceset_metadata(devset);
    if (ret != 0) {
        ERROR("devmapper: load device set metadata failed");
        goto out;
    }

    // Setup the base image
    ret = setup_base_image(devset);
    if (ret != 0) {
        ERROR("devmapper: setup base image failed");
    }

out:
    free(metadata_path);
    util_free_array_by_len(devices_list, devices_len);
    free(target_type);
    free(params);
    free(pool_name);
    return ret;
}
/* memory store map kvfree */
static void device_id_map_kvfree(void *key, void *value)
{
    free(key);
    free(value);
}

static int determine_driver_capabilities(const char *version, struct device_set *devset)
{
    int ret = 0;
    int64_t major, minor;
    char **tmp_str = NULL;
    size_t tmp_str_len = 0;

    tmp_str = util_string_split(version, '.');
    if (tmp_str == NULL) {
        ret = -1;
        goto out;
    }
    tmp_str_len = util_array_len((const char **)tmp_str);
    if (tmp_str_len < 2) {
        ERROR("devmapper: driver version:%s format error", version);
        ret = -1;
        goto out;
    }

    ret = util_parse_byte_size_string(tmp_str[0], &major);
    if (ret != 0) {
        ERROR("devmapper: invalid size: '%s': %s", tmp_str[0], strerror(-ret));
        goto out;
    }

    if (major > 4) {
        devset->driver_deferred_removal_support = true;
        goto out;
    }

    if (major < 4) {
        goto out;
    }

    ret = util_parse_byte_size_string(tmp_str[1], &minor);
    if (ret != 0) {
        ERROR("devmapper: invalid size: '%s': %s", tmp_str[1], strerror(-ret));
        goto out;
    }

    /*
     * If major is 4 and minor is 27, then there is no need to
     * check for patch level as it can not be less than 0.
     */
    if (minor >= 27) {
        devset->driver_deferred_removal_support = true;
        goto out;
    }

out:
    util_free_array(tmp_str);
    return ret;
}

static int devmapper_init_cap_by_version(struct device_set *devset)
{
    int ret = 0;
    char *version = NULL;

    version = dev_get_driver_version();
    if (version == NULL) {
        ERROR("devmapper: driver not supported");
        ret = -1;
        goto out;
    }

    ret = determine_driver_capabilities(version, devset);
    if (ret != 0) {
        ERROR("devmapper: determine driver capabilities failed");
        goto out;
    }

    if (devset->driver_deferred_removal_support) {
        devset->enable_deferred_deletion = true;
        devset->enable_deferred_removal = true;
    }

out:
    return ret;
}

static int devmapper_init_devset(const char *driver_home, const char **options, size_t len, struct graphdriver *driver)
{
    int ret = 0;
    struct device_set *devset = NULL;

    devset = util_common_calloc_s(sizeof(struct device_set));
    if (devset == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    driver->devset = devset;

    devset->root = util_strdup_s(driver_home);
    devset->base_fs_size = default_base_fs_size;
    devset->override_udev_sync_check = DEFAULT_UDEV_SYNC_OVERRIDE;
    devset->do_blk_discard = false;
    devset->thinp_block_size = DEFAULT_THIN_BLOCK_SIZE;
    devset->min_free_space_percent = DEFAULT_MIN_FREE_SPACE_PERCENT;
    devset->device_id_map = map_new(MAP_INT_INT, NULL, device_id_map_kvfree);
    if (devset->device_id_map == NULL) {
        ERROR("devmapper: failed to allocate device id map");
        ret = -1;
        goto out;
    }
    devset->udev_wait_timeout = DEFAULT_UDEV_WAITTIMEOUT;
    devset->metadata_trans = util_common_calloc_s(sizeof(image_devmapper_transaction));
    if (devset->metadata_trans == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto out;
    }

    if (devmapper_parse_options(devset, options, len) != 0) {
        ERROR("devmapper: parse options failed");
        ret = -1;
        goto out;
    }

    if (devmapper_init_cap_by_version(devset) != 0) {
        ERROR("failed to init devmapper cap");
        ret = -1;
        goto out;
    }

    devset->meta_store = metadata_store_new();
    if (devset->meta_store == NULL) {
        ERROR("Failed to init metadata store");
        ret = -1;
        goto out;
    }

    if (pthread_rwlock_init(&devset->devmapper_driver_rwlock, NULL) != 0) {
        ERROR("Failed to init devmapper conf rwlock");
        ret = -1;
        goto out;
    }

out:
    return ret;
}

int device_set_init(struct graphdriver *driver, const char *driver_home, const char **options, size_t len)
{
    int ret = 0;

    if (driver == NULL || driver_home == NULL || options == NULL) {
        return -1;
    }
    // init devmapper log
    log_with_errno_init();

    if (devmapper_init_devset(driver_home, options, len, driver) != 0) {
        ERROR("Failed to init devset");
        ret = -1;
        goto out;
    }

    ret = set_dev_dir(DEVICE_DIRECTORY);
    if (ret != 0) {
        ERROR("devmapper: set dev dir /dev failed");
        goto out;
    }

    set_udev_wait_timeout(driver->devset->udev_wait_timeout);

    ret = do_devmapper_init(driver->devset);
    if (ret != 0) {
        ERROR("Fail to do devmapper init");
        ret = -1;
        goto out;
    }

    return 0;

out:
    return ret;
}

static int parse_storage_opt(const json_map_string_string *opts, uint64_t *size)
{
    int ret = 0;
    size_t i = 0;

    if (size == NULL || opts == NULL) {
        ret = -1;
        goto out;
    }

    for (i = 0; i < opts->len; i++) {
        if (strcasecmp("size", opts->keys[i]) == 0) {
            int64_t converted = 0;
            ret = util_parse_byte_size_string(opts->values[i], &converted);
            if (ret != 0) {
                ERROR("Invalid size: '%s': %s", opts->values[i], strerror(-ret));
                ret = -1;
                goto out;
            }
            *size = (uint64_t)converted;
            break;
        } else {
            ERROR("Unknown option %s", opts->keys[i]);
            ret = -1;
            goto out;
        }
    }
out:
    return ret;
}

// AddDevice adds a device and registers in the hash.
int add_device(const char *hash, const char *base_hash, struct device_set *devset,
               const json_map_string_string *storage_opts)
{
    int ret = 0;
    image_devmapper_device_info *base_info = NULL;
    image_devmapper_device_info *info = NULL;
    uint64_t size = 0;

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    base_info = lookup_device(devset, base_hash);
    if (base_info == NULL) {
        ERROR("devmapper: lookup device %s failed", base_hash);
        ret = -1;
        goto free_out;
    }

    if (base_info->deleted) {
        ret = -1;
        ERROR("devmapper: Base device %s has been marked for deferred deletion", base_info->hash);
        goto free_out;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        // ERROR();
        ret = -1;
        goto free_out;
    }

    ret = parse_storage_opt(storage_opts, &size);
    if (ret != 0) {
        goto free_out;
    }

    if (size == 0) {
        size = base_info->size;
    }

    if (size < base_info->size) {
        ERROR("devmapper: Container size cannot be smaller than %lu", base_info->size);
        goto free_out;
    }

    ret = take_snapshot(devset, hash, base_info, size);
    if (ret != 0) {
        goto free_out;
    }

    // Grow the container rootfs.
    if (size > base_info->size) {
        free_image_devmapper_device_info(info);
        info = NULL;
        info = lookup_device(devset, hash);
        if (info == NULL) {
            ERROR("devmapper: lookup device %s failed", hash);
            ret = -1;
            goto free_out;
        }

        ret = grow_fs(devset, info);
        if (ret != 0) {
            goto free_out;
        }
    }
    ret = 0;
free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("unlock devmapper conf failed");
        return -1;
    }
    free_image_devmapper_device_info(base_info);
    free_image_devmapper_device_info(info);
    return ret;
}

// moptions->options_len > 0
static char *generate_mount_options(const struct driver_mount_opts *moptions, const char *dev_options)
{
    char *res_str = NULL;
    char *options = NULL;
    bool add_nouuid = false;

    options = util_strdup_s(dev_options);
    if (moptions != NULL && moptions->options_len > 0) {
        add_nouuid = !util_valid_str(options) || strings_contains_word("nouuid", options);
        free(options);
        options = util_string_join(",", (const char **)moptions->options, moptions->options_len);
        if (add_nouuid) {
            res_str = util_strdup_s("nouuid");
        }
    }

    append_mount_options(&res_str, options);

    free(options);
    return res_str;
}

int mount_device(const char *hash, const char *path, const struct driver_mount_opts *mount_opts,
                 struct device_set *devset)
{
    int ret = 0;
    image_devmapper_device_info *info = NULL;
    char *dev_fname = NULL;
    char *options = NULL;

    if (hash == NULL || path == NULL || mount_opts == NULL) {
        ERROR("devmapper: failed to mount device");
        return -1;
    }

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ERROR("devmapper: lookup device %s failed", hash);
        ret = -1;
        goto free_out;
    }

    if (info->deleted) {
        ret = -1;
        ERROR("devmapper: Base device %s has been marked for deferred deletion", info->hash);
        goto free_out;
    }
    dev_fname = dev_name(devset, info);
    if (dev_fname == NULL) {
        ERROR("devmapper: failed to get device full name");
        goto free_out;
    }

    ret = activate_device_if_needed(devset, info, false);
    if (ret != 0) {
        ERROR("devmapper: Error activating devmapper device for %s", hash);
        goto free_out;
    }

    options = generate_mount_options(mount_opts, devset->mount_options);

    ret = util_mount(dev_fname, path, "ext4", options);
    if (ret != 0) {
        ERROR("devmapper: Error mounting %s on %s", dev_fname, path);
        goto free_out;
    }

free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("unlock devmapper conf failed");
        ret = -1;
    }
    free_image_devmapper_device_info(info);
    free(dev_fname);
    free(options);
    return ret;
}

// UnmountDevice unmounts the device and removes it from hash.
int unmount_device(const char *hash, const char *mount_path, struct device_set *devset)
{
    int ret = 0;
    image_devmapper_device_info *info = NULL;

    if (hash == NULL || mount_path == NULL) {
        ERROR("devmapper: failed to unmount device");
        return -1;
    }

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ERROR("devmapper: lookup device %s failed", hash);
        ret = -1;
        goto free_out;
    }

    if (util_detect_mounted(mount_path)) {
        ret = umount2(mount_path, MNT_DETACH);
        if (ret < 0 && errno != EINVAL) {
            WARN("Failed to umount directory %s:%s", mount_path, strerror(errno));
            goto free_out;
        }
    }

    ret = util_path_remove(mount_path);
    if (ret != 0) {
        DEBUG("devmapper: doing remove on a unmounted device %s failed", mount_path);
    }

    ret = deactivate_device(devset, info);
    if (ret != 0) {
        ERROR("devmapper: Error deactivating device");
    }

free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("unlock devmapper conf failed");
        ret = -1;
    }
    free_image_devmapper_device_info(info);
    return ret;
}

bool has_device(const char *hash, struct device_set *devset)
{
    bool res = false;
    image_devmapper_device_info *info = NULL;

    if (hash == NULL) {
        ERROR("devmapper: failed to judge device metadata exists");
        return false;
    }

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ERROR("devmapper: lookup device %s failed", hash);
        goto free_out;
    }

    res = true;
free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("unlock devmapper conf failed");
    }
    free_image_devmapper_device_info(info);
    return res;
}

int delete_device(const char *hash, bool sync_delete, struct device_set *devset)
{
    int ret = 0;
    image_devmapper_device_info *info = NULL;

    if (hash == NULL) {
        return -1;
    }

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ret = -1;
        ERROR("devmapper: lookup device %s failed", hash);
        goto free_out;
    }

    ret = do_delete_device(devset, hash, sync_delete);

free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ret = -1;
        ERROR("unlock devmapper conf failed");
    }
    free_image_devmapper_device_info(info);
    return ret;
}

int export_device_metadata(struct device_metadata *dev_metadata, const char *hash, struct device_set *devset)
{
    int ret = 0;
    char *dm_name = NULL;
    image_devmapper_device_info *info = NULL;

    if (hash == NULL || dev_metadata == NULL) {
        return -1;
    }

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return -1;
    }

    dm_name = get_dm_name(devset, hash);
    if (dm_name == NULL) {
        ret = -1;
        ERROR("devmapper: failed to get dm %s name", hash);
        goto free_out;
    }

    info = lookup_device(devset, hash);
    if (info == NULL) {
        ret = -1;
        ERROR("devmapper: lookup device %s failed", hash);
        goto free_out;
    }

    dev_metadata->device_id = info->device_id;
    dev_metadata->device_size = info->size;
    dev_metadata->device_name = util_strdup_s(dm_name);

free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ret = -1;
        ERROR("unlock devmapper conf failed");
    }
    free_image_devmapper_device_info(info);
    free(dm_name);
    return ret;
}

void free_devmapper_status(struct status *st)
{
    if (st == NULL) {
        return;
    }
    free(st->pool_name);
    st->pool_name = NULL;
    free(st->data_file);
    st->data_file = NULL;
    free(st->data_loopback);
    st->data_loopback = NULL;
    free(st->metadata_file);
    st->metadata_file = NULL;
    free(st->metadata_loopback);
    st->metadata_loopback = NULL;
    free(st->base_device_fs);
    st->base_device_fs = NULL;

    free(st);
}

static bool is_real_file(const char *f)
{
    struct stat st;
    int nret = 0;

    if (f == NULL) {
        return false;
    }

    nret = stat(f, &st);
    if (nret < 0) {
        return false;
    }

    return S_ISREG(st.st_mode);
}

static int get_underlying_available_space(const char *loop_file, uint64_t *available)
{
    struct statfs buf;
    int ret = 0;

    if (loop_file == NULL) {
        return -1;
    }

    ret = statfs(loop_file, &buf);
    if (ret < 0) {
        WARN("devmapper: can not stat loopfile filesystem %s", loop_file);
        return ret;
    }

    *available = buf.f_bfree * buf.f_bsize;

    return 0;
}

struct status *device_set_status(struct device_set *devset)
{
    int ret = 0;
    struct status *st = NULL;
    uint64_t total_size_in_sectors, transaction_id, data_used;
    uint64_t data_total, metadata_used, metadata_total;
    uint64_t min_free_data;

    if (pthread_rwlock_wrlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("lock devmapper conf failed");
        return NULL;
    }

    st = util_common_calloc_s(sizeof(struct status));
    if (st == NULL) {
        ERROR("devmapper: out of memory");
        goto free_out;
    }

    st->pool_name = get_pool_name(devset);
    st->data_file = util_strdup_s(devset->data_device);
    st->data_loopback = util_strdup_s(devset->data_loop_file);
    st->metadata_file = util_strdup_s(devset->metadata_device);
    st->metadata_loopback = util_strdup_s(devset->metadata_loop_file);
    st->udev_sync_supported = udev_sync_supported();
    st->deferred_remove_enabled = devset->deferred_remove;
    st->deferred_delete_enabled = devset->deferred_delete;
    st->deferred_deleted_device_count = devset->nr_deleted_devices;
    st->base_device_size = get_base_device_size(devset);
    st->base_device_fs = util_strdup_s(devset->base_device_filesystem);

    ret = pool_status(devset, &total_size_in_sectors, &transaction_id, &data_used, &data_total, &metadata_used,
                      &metadata_total);
    if (ret == 0) {
        uint64_t block_size_in_sectors = total_size_in_sectors / data_total;
        st->data.used = data_used * block_size_in_sectors * 512;
        st->data.total = data_total * block_size_in_sectors * 512;
        st->data.available = st->data.total - st->data.used;

        st->metadata.used = metadata_used * 4096;
        st->metadata.total = metadata_total * 4096;
        st->metadata.available = st->metadata.total - st->metadata.used;

        st->sector_size = block_size_in_sectors * 512;

        if (is_real_file(devset->data_loop_file)) {
            uint64_t actual_space;
            ret = get_underlying_available_space(devset->data_loop_file, &actual_space);
            if (ret == 0 && actual_space < st->metadata.available) {
                st->data.available = actual_space;
            }
        }

        if (is_real_file(devset->metadata_loop_file)) {
            uint64_t actual_space;
            ret = get_underlying_available_space(devset->data_loop_file, &actual_space);
            if (ret == 0 && actual_space < st->metadata.available) {
                st->metadata.available = actual_space;
            }
        }

        min_free_data = (data_total * (uint64_t)devset->min_free_space_percent) / 100;
        st->min_free_space = min_free_data * block_size_in_sectors * 512;
    }

free_out:
    if (pthread_rwlock_unlock(&(devset->devmapper_driver_rwlock)) != 0) {
        ERROR("unlock devmapper conf failed");
    }
    return st;
}