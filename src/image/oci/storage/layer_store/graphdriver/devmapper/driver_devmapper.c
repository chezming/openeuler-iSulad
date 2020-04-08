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
* Author: wangfengtu
* Create: 2020-01-19
* Description: provide devicemapper graphdriver function definition
******************************************************************************/
#include "driver_devmapper.h"
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

#include "log.h"
#include "libisulad.h"
#include "utils.h"
#include "wrapper_devmapper.h"
#include "devices_constants.h"
#include "device_setup.h"
#include "libdevmapper.h"

#define DM_LOG_FATAL 2
#define DM_LOG_DEBUG 7


static bool user_base_size = false;
static bool enable_deferred_removal = false;
static bool driver_deferred_removal_support = false;
static bool enable_deferred_deletion = false;

// static int64_t default_udev_wait_timeout = 185;
static uint64_t default_base_fs_size = 10L * 1024L * 1024L * 1204L;

static struct devmapper_conf g_devmapper_conf;

int devmapper_conf_wrlock()
{
    int ret = 0;

    if (pthread_rwlock_wrlock(&g_devmapper_conf.devmapper_driver_rwlock)) {
        ret = -1;
    }

    return ret;
}

int devmapper_conf_rdlock()
{
    int ret = 0;

    if (pthread_rwlock_rdlock(&g_devmapper_conf.devmapper_driver_rwlock)) {
        ret = -1;
    }

    return ret;
}

int devmapper_conf_unlock()
{
    int ret = 0;

    if (pthread_rwlock_unlock(&g_devmapper_conf.devmapper_driver_rwlock)) {
        ret = -1;
    }

    return ret;
}

struct device_set *devmapper_driver_devices_get()
{
    return g_devmapper_conf.devset;

}

static char *util_trim_prefice_string(char *str, const char *prefix)
{
    if (str == NULL || !util_has_prefix(str, prefix)) {
        return str;
    }

    char *begin = str + strlen(prefix);
    char *tmp = str;
    while ((*tmp++ = *begin++)) {}
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
            ERROR("Out of memory");
            return -1;
        }
        p = strchr(dup, '='); // ght 未找到=返回NULL
        if (!p) {
            ERROR("Unable to parse key/value option: '%s'", dup);
            free(dup);
            return -1;
        }
        *p = '\0';
        val = p + 1;
        if (strcasecmp(dup, "dm.fs") == 0) {
            if (strcmp(val, "ext4")) {
                ERROR("Invalid filesystem: '%s': not supported", val);
                ret = -1;
            }
        } else if (strcasecmp(dup, "dm.thinpooldev") == 0) {
            if (!strcmp(val, "")) {
                ERROR("Invalid thinpool device, it must not be empty");
                ret = -1;
            }
            devset->thin_pool_device = util_trim_prefice_string(val, "/dev/mapper");

        } else if (strcasecmp(dup, "dm.min_free_space") == 0) {
            long converted = 0;
            ret = util_parse_percent_string(val, &converted);
            if (ret != 0 || converted == 100) {
                ERROR("Invalid min free space: '%s': %s", val, strerror(-ret));
                ret = -1;
            }
        } else if (strcasecmp(dup, "dm.basesize") == 0) {
            int64_t converted = 0;
            ret = util_parse_byte_size_string(val, &converted);
            if (ret != 0) {
                ERROR("Invalid size: '%s': %s", val, strerror(-ret));
            }
            // 无符号数转换
            // if (converted > 0) {
            user_base_size = true;
            devset->base_fs_size = (uint64_t)converted;
            // }
        } else if (strcasecmp(dup, "dm.mkfsarg") == 0 || strcasecmp(dup, "dm.mountopt") == 0) {
            /* We have no way to check validation here, validation is checked when using them. */
        } else {
            ERROR("devicemapper: unknown option: '%s'", dup);
            ret = -1;
        }
        free(dup);
        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}

void free_device_set(struct device_set *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->root);
    ptr->root = NULL;
    free(ptr->device_prefix);
    ptr->device_prefix = NULL;

    free_image_devmapper_direct_lvm_config(ptr->lvm_setup_config);
    ptr->lvm_setup_config = NULL;
    free(ptr);
}
static int enable_deferred_removal_deletion(struct device_set *devset)
{
    if (enable_deferred_removal) {
        if (!driver_deferred_removal_support) {
            ERROR("devmapper: Deferred removal can not be enabled as kernel does not support it");
            return -1;
        }
        devset->deferred_remove = true;
    }

    if (enable_deferred_deletion) {
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

static char *deviceset_mata_file(struct device_set *devset)
{
    char *dir = NULL;
    char *file = NULL;

    dir = metadata_dir(devset);
    if (dir == NULL) {
        return NULL;
    }

    file = util_path_join(dir, DEVICE_SET_METAFILE);
    if (file == NULL) {
        goto out;
    }

    free(dir);
    return file;

out:
    free(dir);
    dir = NULL;
    free(file);
    file = NULL;
    return file;
}

static void free_arr(char **arr, size_t len)
{
    size_t i = 0;

    if (arr == NULL) {
        return;
    }

    for (; i < len; i++) {
        free(*(arr + i));
        *(arr + i) = NULL;
    }
    free(arr);
    arr = NULL;
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
        if (ret < 0) {
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

static bool thin_pool_exists(struct device_set *devset, const char *pool_name)
{
    int ret;
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
    int ret;
    parser_error err = NULL;

    if (hash == NULL) {
        return NULL;
    }

    metadata_path = metadata_dir(devset);
    if (metadata_path == NULL) {
        goto out;
    }
    if (strcmp(hash, "") == 0) {
        ret = snprintf(metadata_file, sizeof(metadata_file), "%s/base", metadata_path);
    } else {
        ret = snprintf(metadata_file, sizeof(metadata_file), "%s/%s", metadata_path, hash);
    }
    if (ret < 0) {
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

    if (util_array_len((const char**)tmp_args) != CMD_ARGS_NUM) {
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

    info = metadata_store_get(hash);
    if (info == NULL) {
        info = load_metadata(devset, hash);
        if (info == NULL) {
            ERROR("devmapper: Unknown device %s", hash);
            return info;
        }
        res = metadata_store_add(hash, info);
        if (!res) {
            ERROR("devmapper: load device %s failed", hash);
            free(info);
            info = NULL;
        }
    }

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

// static char *get_base_device_fs(struct device_set *devset)
// {
//     return util_strdup_s(devset->base_device_filesystem);
// }

static int device_file_walk(struct device_set *devset)
{
    DIR *dp;
    struct dirent *entry;
    struct stat st;
    image_devmapper_device_info *info = NULL;
    int ret = 0;

    if ((dp = opendir(DEVICE_FILE_DIR)) == NULL) {
        ERROR("devmapper: open dir %s failed", DEVICE_FILE_DIR);
        return -1;
    }

    // 路径权限导致stat为非regular文件，误判为dir，此处需优化
    while ((entry = readdir(dp)) != NULL) {
        stat(entry->d_name, &st);

        if (S_ISDIR(st.st_mode)) {
            continue;
        }

        if (util_has_prefix(entry->d_name, ".")) {
            continue;
        }
        if (strcmp(entry->d_name, DEVICE_SET_METAFILE) == 0 || strcmp(entry->d_name, TRANSACTION_METADATA) == 0) {
            continue;
        }

        info = lookup_device(devset, entry->d_name); // entry->d_name 取值base  hash值等
        if (info == NULL) {
            return -1;
        }
        free(info);
        info = NULL;
    }

    return ret;
}


static void construct_device_id_map(struct device_set *devset)
{
    // TODO:遍历g_metadata_store中全部device
    // for info in devices {
    //     mark_device_id_used(info->device_id);
    // }

}

static void count_deleted_devices(struct device_set *devset)
{
    // TODO:遍历g_metadata_store中全部device
    // for info in devices {
    //     if !info->deleted {
    //      continue
    //    }
    // devset->nr_deleted_devices++;
    // }
}
static int rollback_transaction(struct device_set *devset)
{
    return 0;

}

static int process_pending_transaction(struct device_set *devset)
{
    int ret = 0;

    if (devset == NULL || devset->metadata_trans == NULL) {
        // DEBUG("devmapper: device set or tansaction is NULL");
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
        ERROR("devmapper: Open Transaction id %d is less than pool transaction id %d",
              devset->metadata_trans->open_transaction_id, devset->transaction_id);
        return -1;
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

static int init_metadata(struct device_set *devset, const char *pool_name)
{
    int ret;
    uint64_t start, length;
    char *target_type = NULL;
    char *params = NULL;

    ret = dev_get_status(&start, &length, &target_type, &params, pool_name);
    if (ret != 0) {
        return -1;
    }
    // fmt.Sscanf(params, "%d %d/%d %d/%d", &transactionID, &metadataUsed, &metadataTotal, &dataUsed, &dataTotal)
    // devset->transaction_id = transaction_id


    ret = device_file_walk(devset);
    if (ret != 0) {
        ERROR("devmapper: Failed to load device files");
        return -1;
    }

    construct_device_id_map(devset);
    count_deleted_devices(devset);
    ret = process_pending_transaction(devset);
    if (ret != 0) {
        return -1;
    }

    // TODO: start a thread to cleanup deleted devices

    free(target_type);
    free(params);
    return ret;
}

static int load_deviceset_metadata(struct device_set *devset)
{
    image_devmapper_deviceset_metadata *deviceset_meta = NULL;
    parser_error err;
    char *meta_file = NULL;
    int ret = 0;

    meta_file = deviceset_mata_file(devset);
    if (meta_file == NULL) {
        return -1;
    }

    deviceset_meta = image_devmapper_deviceset_metadata_parse_file(meta_file, NULL, &err);
    if (deviceset_meta == NULL) {
        ERROR("devmapper: load deviceset metadata file error %s", err);
        ret = -1;
        goto out;
    }
    devset->next_device_id = deviceset_meta->next_device_id;
    devset->base_device_filesystem = deviceset_meta->base_device_filesystem;
    devset->base_device_uuid = deviceset_meta->base_device_uuid;

out:
    free(err);
    free_image_devmapper_deviceset_metadata(deviceset_meta);
    free(meta_file);
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
    int mask;
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

static bool is_device_id_free(struct device_set *devset, int device_id)
{
    int mask;
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
static char *metadata_file(struct device_set *devset, const char *hash)
{
    char *file = NULL;
    char *full_path = NULL;
    char *dir = NULL;

    if (hash == NULL) {
        return NULL;
    }

    dir = metadata_dir(devset);
    if (dir == NULL) {
        return NULL;
    }

    file = strcmp(hash, "") == 0 ? util_strdup_s("base") : util_strdup_s(hash);
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
        return -1;
    }

    fname = metadata_file(devset, info->hash);
    if (fname == NULL) {
        ERROR("devmapper: get metadata file full path failed");
        return -1;
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

    if (snprintf(fname, sizeof(fname), "%s/metadata/%s", devset->root,  TRANSACTION_METADATA) < 0) {
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

    fname = deviceset_mata_file(devset);
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

static int remove_metadata(struct device_set *devset, const char *hash)
{
    int ret;
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
    int ret;

    ret = metadata_store_remove(hash);
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
    int ret;
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

    store_res = metadata_store_add(hash, info);
    if (!store_res) {
        ERROR("devmapper: metadata store add failed hash %s", hash);
        goto out;
    }

    ret = save_metadata(devset, info);
    if (ret != 0) {
        if (!metadata_store_remove(hash)) {
            ERROR("devmapper: metadata file %s store remove failed", hash);
        }
        goto out;
    }

    return info;
out:
    free_image_devmapper_device_info(info);
    return NULL;
}

static image_devmapper_device_info *create_register_device(struct device_set *devset, const char *hash)
{
    int ret;
    int device_id;
    char *pool_dev = NULL;
    image_devmapper_device_info *info = NULL;

    ret = get_next_free_device_id(devset, &device_id);
    if (ret == 0) {
        // ERROR();
        return NULL;
    }

    ret = open_transaction(devset, hash, device_id);
    if (ret != 0) {
        ERROR("devmapper: Error opening transaction hash = %s deviceID = %d", hash, device_id);
        return NULL;
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
            if (true) {
                ret = get_next_free_device_id(devset, &device_id);
                if (ret == 0) {
                    // ERROR();
                    return NULL;
                }
                ret = refresh_transaction(devset, device_id);
                if (ret != 0) {
                    ERROR("devmapper: Error refres open transaction deviceID = %d", hash, device_id);
                    return NULL;
                }
                continue;
            }
            mark_device_id_free(devset, device_id);
            return NULL;
        }
        break;
    } while (true);

    info = register_device(devset, device_id, hash, devset->base_fs_size, devset->metadata_trans->open_transaction_id);
    if (info == NULL) {
        ret = unregister_device(devset, hash);
        if (ret != 0) {
            // ERROR();
        }
        ret = dev_delete_device(pool_dev, device_id);
        if (ret != 0) {
            // ERROR()
        }
        mark_device_id_free(devset, device_id);
    }

out:
    UTIL_FREE_AND_SET_NULL(pool_dev);
    return info;
}

static int cancel_deferred_removal(struct device_set *devset, const char *hash)
{
    int i = 0;
    int ret;
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

static int cancel_deferred_removal_if_needed(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *dm_name = NULL;
    struct dm_info dmi = { 0 };

    if (!devset->deferred_remove == 0) {
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
        // ERROR();
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
        ERROR("devmapper: Can't activate device %v as it is marked for deletion", info->hash);
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
        ret = -1;
        goto out;
    }

    ret = dev_active_device(pool_dev_name, dm_name, info->device_id, info->size);

out:
    UTIL_FREE_AND_SET_NULL(dm_name);
    UTIL_FREE_AND_SET_NULL(pool_dev_name);
    return ret;
}

static int deactivate_device_mode(struct device_set *devset, image_devmapper_device_info *dev_info,
                                  bool deferred_remove)
{
    int ret;
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

static int save_base_device_uuid(struct device_set *devset, image_devmapper_device_info *info)
{
    int ret = 0;
    char *base_dev_uuid = NULL;
    char *dev_fname = NULL;
    char *dm_name = NULL;

    ret = activate_device_if_needed(devset, info, false);
    if (ret != 0) {
        // ERROR();
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
        // ERROR();
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

static int create_base_image(struct device_set *devset)
{
    int ret;
    image_devmapper_device_info *info = NULL;

    // create initial device
    info = create_register_device(devset, "");
    if (info == NULL) {
        return -1;
    }

    DEBUG("devmapper: Creating filesystem on base device-mapper thin volume");

    ret = activate_device_if_needed(devset, info, false);
    if (ret != 0) {
        ret = -1;
        goto out;
    }

    // TODO:
    // ret = create_file_system(info);
    // if (ret != 0) {
    //     goto out;
    // }

    info->initialized = true;

    ret = save_metadata(devset, info);
    if (ret != 0) {
        // ERROR();
        info->initialized = false;
        goto out;
    }

    ret = save_base_device_uuid(devset, info);
    if (ret != 0) {
        ERROR("devmapper: Could not query and save base device UUID");
    }

out:
    free_image_devmapper_device_info(info);
    return ret;
}

static int pool_status(struct device_set *devset, uint64_t *total_size_in_sectors, uint64_t *transaction_id, \
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
        return -1;
    }

    ret = dev_get_status(&start, &length, &target_type, &params, name);
    if (ret != 0) {
        free(name);
        return -1;
    }
    *total_size_in_sectors = length;
    // TODO: parse params
    //fmt.Sscanf(params, "%d %d/%d %d/%d", &transactionID, &metadataUsed, &metadataTotal, &dataUsed, &dataTotal)
    *transaction_id = 0;
    *data_total = 0;
    *data_used = 0;
    *metadata_used = 0;
    *metadata_total = 0;

    free(name);
    free(target_type);
    free(params);
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
        //ERROR()
        return -1;
    }

    if (data_used != 0) {
        ERROR("devmapper: Unable to take ownership of thin-pool (%s) that already has used data blocks",
              devset->thin_pool_device);
        return -1;
    }

    if (transaction_id != 0) {
        ERROR("devmapper: Unable to take ownership of thin-pool (%s) with non-zero transaction ID", devset->thin_pool_device);
        return -1;
    }

    DEBUG("devmapper:total_size_in_sectors:%u, data_total:%u, metadata_used:%u, metadata_total:%u", total_size_in_sectors,
          data_total, metadata_used, metadata_total);

    return ret;

}

static int save_base_device_filesystem(struct device_set *devset, const char *fs)
{
    devset->base_device_filesystem = util_strdup_s(fs);
    return save_deviceset_matadata(devset);
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
        // ERROR();
        goto free_out;
    }

    if (strcmp(devset->base_device_uuid, uuid) != 0) {
        ERROR("devmapper: Current Base Device UUID:%s does not match with stored UUID:%s. \
        Possibly using a different thin pool than last invocation", uuid, devset->base_device_uuid);
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
        WARN("devmapper: Base device already exists and has filesystem %s on it. User specified filesystem %s will be ignored.",
             \
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
        ERROR("Error activating devmapper device");
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

    if (!user_base_size) {
        return ret;
    }

    base_dev_size = get_base_device_size(devset);

    if (devset->base_fs_size < base_dev_size) {
        ERROR("devmapper: Base fs size cannot be smaller than %d", base_dev_size);
        return -1;
    }

    if (devset->base_fs_size == base_dev_size) {
        return 0;
    }

    base_info->size = devset->base_fs_size;

    ret = save_metadata(devset, base_info);
    if (ret != 0) {
        // Try to remove unused device

        if (!metadata_store_remove(base_info->hash)) {
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
    int ret;
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
    int ret;
    struct dm_info dinfo;
    char *dm_name = NULL;
    char *dev_fname = NULL;

    ret = activate_device_if_needed(devset, info, true);
    if (ret != 0) {
        // ERROR();
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

int delete_device(struct device_set *devset, const char *hash, bool sync_delete)
{
    int ret;
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
    free_image_devmapper_device_info(info);
    return ret;
}

static int setup_base_image(struct device_set *devset)
{
    int ret;
    image_devmapper_device_info *old_info = NULL;

    old_info = lookup_device(devset, "");
    if (old_info == NULL) {
        ERROR("devmapper: lookup device failed");
        return -1;
    }

    // base image already exists. If it is initialized properly, do UUID
    // verification and return. Otherwise remove image and set it up
    // fresh.
    if (old_info != NULL) {
        if (old_info->initialized && !old_info->deleted) {
            ret = setup_verify_baseimages_uuidfs(devset, old_info);
            if (ret != 0) {
                // ERROR();
                goto out;
            }

            ret = check_grow_base_device_fs(devset, old_info);
            if (ret != 0) {
                // ERROR();
            }
            goto out;
        }

        DEBUG("devmapper: Removing uninitialized base image");
        ret = delete_device(devset, "", true);
        if (ret != 0) {
            goto out;
        }
    }

    // If we are setting up base image for the first time, make sure
    // thin pool is empty.
    if (util_valid_str(devset->thin_pool_device) && old_info == NULL) {
        ret = check_thin_pool(devset);
        if (ret != 0) {
            goto out;
        }
    }

    ret = create_base_image(devset);

out:
    free_image_devmapper_device_info(old_info);
    return ret;
}


static int do_devmapper_init(struct device_set *devset, bool do_init)
{
    int ret;
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
    bool pool_exist;
    char *pool_name = NULL;
    size_t i = 0;

    ret = enable_deferred_removal_deletion(devset);
    if (ret != 0) {
        return -1;
    }

    support = udev_set_sync_support(true);
    if (!support) {
        ERROR("devmapper: Udev sync is not supported. This will lead to data loss and unexpected behavior.");
        if (!devset->overrid_udev_sync_check) {
            ERROR("driver not supported");
            return -1;
        }
    }

    ret = util_mkdir_p(devset->root, DEFAULT_DEVICE_SET_MODE);
    if (ret != 0) {
        //ERROR();
        return -1;
    }
    metadata_path = metadata_dir(devset);
    ret = util_mkdir_p(metadata_path, DEFAULT_DEVICE_SET_MODE);
    if (ret != 0) {
        //ERROR();
        goto out;
    }

    // cfg = read_lvm_config(devset->root); // lvm自动配置暂不支持
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
        //error
        goto out;
    }
    pool_exist = thin_pool_exists(devset, pool_name);

    if (!pool_exist || (devset->thin_pool_device == NULL || strlen(devset->thin_pool_device) == 0)) {
        ERROR("devmapper: thin pool is not exist, please create it firstly");
        goto out;
    }

    ret = init_metadata(devset, pool_name);
    if (ret != 0) {
        // ERROR();
        goto out;
    }

    // Right now this loads only NextDeviceID. If there is more metadata
    // down the line, we might have to move it earlier.
    ret = load_deviceset_metadata(devset);
    if (ret != 0) {
        //ERROR();
        goto out;
    }

    // Setup the base image
    if (do_init) {
        ret = setup_base_image(devset);
        if (ret != 0) {
            // ERROR();
            goto out;
        }
    }

out:
    free(metadata_path);
    free_arr(devices_list, devices_len);
    free(target_type);
    free(params);
    free(pool_name);
    return -1;
}
/* memory store map kvfree */
static void device_id_map_kvfree(void *key, void *value)
{
    free(key);
    free(value);
}


int devmapper_init(struct graphdriver *driver, const char *drvier_home, const char **options, size_t len)
{
    int ret = 0;
    struct device_set *devset = NULL;
    image_devmapper_direct_lvm_config *lvm_setup_config = NULL;

    if (driver == NULL || drvier_home == NULL || options == NULL) {
        return -1;
    }
    // init devmapper log
    log_with_errno_init();

    lvm_setup_config = util_common_calloc_s(sizeof(image_devmapper_direct_lvm_config));
    if (lvm_setup_config == NULL) {
        //ERROR();
    }

    devset = util_common_calloc_s(sizeof(struct device_set));
    if (devset == NULL) {
        // ERROR();
    }
    devset->root = util_strdup_s(driver->home);
    devset->base_fs_size = default_base_fs_size;
    devset->overrid_udev_sync_check = DEFAULT_UDEV_SYNC_OVERRIDE;
    devset->thin_block_size = DEFAULT_THIN_BLOCK_SIZE;
    devset->min_free_space_percent = DEFAULT_MIN_FREE_SPACE_PERCENT;
    devset->device_id_map = map_new(MAP_INT_INT, NULL, device_id_map_kvfree);
    if (devset->device_id_map == NULL) {
        // ERROR();
    }

    // metadata db
    ret = metadata_store_init();
    if (ret != 0) {
        //ERROR();
    }

    // TODO:lock

    g_devmapper_conf.devset = devset;

    // TODO:unlock

    ret = set_dev_dir(DEVICE_DIRECTORY);
    if (ret) {
        //ERROR
    }

    if (util_mkdir_p(drvier_home, 0700) != 0) {
        ERROR("Unable to create driver home directory %s.", drvier_home);
        ret = -1;
        goto out;
    }

    ret = devmapper_parse_options(devset, options, len);
    if (ret != 0) {
        ret = -1;
        goto out;
    }

    ret = validate_lvm_config(lvm_setup_config);
    if (ret != 0) {
        //ERROR();
        goto out;
    }
    devset->lvm_setup_config = lvm_setup_config;

    ret = do_devmapper_init(devset, true);
    if (ret != 0) {
        ret = -1;
        goto out;
    }

    return 0;

out:
    free_device_set(devset); // 递归free
    return ret;
}

bool devmapper_is_quota_options(struct graphdriver *driver, const char *option)
{
    return false;
}

int devmapper_create_rw(const char *id, const char *parent, const struct graphdriver *driver,
                        const struct driver_create_opts *create_opts)
{
    return 0;
}

int devmapper_rm_layer(const char *id, const struct graphdriver *driver)
{
    return 0;
}

char *devmapper_mount_layer(const char *id, const struct graphdriver *driver,
                            const struct driver_mount_opts *mount_opts)
{
    return NULL;
}

int devmapper_umount_layer(const char *id, const struct graphdriver *driver)
{
    return 0;
}

bool devmapper_layer_exists(const char *id, const struct graphdriver *driver)
{
    return true;
}

int devmapper_apply_diff(const char *id, const struct graphdriver *driver, const struct io_read_wrapper *content,
                         int64_t *layer_size)
{
    return 0;
}

int devmapper_get_layer_metadata(const char *id, const struct graphdriver *driver, json_map_string_string *map_info)
{
    return 0;
}

int devmapper_get_driver_status(const struct graphdriver *driver, struct graphdriver_status *status)
{
    return 0;
}
