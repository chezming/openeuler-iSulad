
#include "device_setup.h"
#include "utils_file.h"
#include "isula_libutils/log.h"
#include "utils_string.h"
#include "utils.h"

#define LVM_DISK_SCAN "lvmdiskscan"
#define PVDISPLAY "pvdisplay"

int validate_lvm_config(image_devmapper_direct_lvm_config *cfg)
{
    int ret = -1;

    if (cfg == NULL) {
        ERROR("direct lvm config is empty");
        return -1;
    }

    if (!util_valid_str(cfg->device)) {
        ERROR("must provide device path in `dm.directlvm_device` in order to configure direct-lvm");
        return ret;
    }

    if ((cfg->thinp_percent > 0 && cfg->thinp_meta_percent == 0) ||
        (cfg->thinp_meta_percent > 0 && cfg->thinp_percent)) {
        ERROR("must set both `dm.thinp_percent` and `dm.thinp_metapercent` if either is specified");
        return ret;
    }

    if (cfg->thinp_percent + cfg->thinp_meta_percent > 100) {
        ERROR("combined `dm.thinp_percent` and `dm.thinp_metapercent` must not be greater than 100");
        return ret;
    }

    return 0;
}

int check_dev_available(const char *dev)
{
    // char *err = NULL;
    // char *lvm_scan_fullpath = NULL;
    // lvm_scan_fullpath = look_path("lvmdiskscan", &err);

    // TODO: exec and get output and match dev

    return 0;
}

int check_dev_invg(const char *dev)
{
    // char *err = NULL;
    // char *pvdisplay_fullpath = NULL;
    // pvdisplay_fullpath = look_path("pvdisplay", &err);

    return 0;
}

int check_dev_hasfs(const char *dev)
{
    // char *err = NULL;
    // char *blkid_fullpath = NULL;
    // pvdisplay_fullpath = look_path("blkid", &err);

    return 0;
}

int verify_block_device(const char *dev, bool force)
{
    int ret = 0;

    ret = check_dev_available(dev);
    if (ret != 0) {
        //ERROR();
        return ret;
    }

    ret = check_dev_invg(dev);
    if (ret != 0) {
        //ERROR();
        return ret;
    }

    if (force) {
        return ret;
    }

    ret = check_dev_hasfs(dev);
    if (ret != 0) {
        //ERROR();
        return ret;
    }
    return ret;
}

image_devmapper_direct_lvm_config *read_lvm_config(const char *root)
{
    parser_error err = NULL;
    image_devmapper_direct_lvm_config *cfg = NULL;
    char *path = NULL;

    path = util_path_join(root, "setup-config.json");
    if (!util_file_exists(path)) {
        goto out;
    }

    cfg = image_devmapper_direct_lvm_config_parse_file(path, NULL, &err);
    if (cfg == NULL) {
        ERROR("load setup-config.json failed %s", err != NULL ? err : "");
        goto out;
    }

out:
    free(path);
    free(err);
    return cfg;
}

int write_lvm_config(const char *root, image_devmapper_direct_lvm_config *cfg)
{
    char *path = NULL;
    int ret = 0;

    path = util_path_join(root, "setup-config.json");
    if (!util_file_exists(path)) {
        goto out;
    }
    // TODO:write to file

out:
    free(path);
    return ret;
}

int setup_direct_lvm(image_devmapper_direct_lvm_config *cfg)
{
    // char *lvm_profile_dir = "/etc/lvm/profile";
    // char *binaries[] = {"pvcreate", "vgcreate", "lvcreate", "lvconvert", "lvchange", "thin_check"};
    //lookpath
    // TODO: 执行上述命令
    return 0;
}

// ProbeFsType returns the filesystem name for the given device id.
// device: /dev/mapper/%s-hash
char *probe_fs_type(const char *device)
{
    return NULL;
}

void append_mount_options(char **dest, const char *suffix)
{
    char *res_string = NULL;
    size_t length;

    if (dest == NULL) {
        // ERROR();
        return;
    }

    if (*dest == NULL) {
        *dest = util_strdup_s(suffix);
    }

    if (suffix == NULL) {
        return;
    }

    if (strlen(suffix) > ((SIZE_MAX - strlen(*dest) - strlen(",")) - 1)) {
        ERROR("String is too long to be appended");
        return;
    }

    length = strlen(*dest) + strlen(",") + strlen(suffix) + 1;
    res_string = util_common_calloc_s(length);
    if (res_string == NULL) {
        ERROR("Out of memory");
        return;
    }
    (void)strcat(res_string, *dest);
    (void)strcat(res_string, ",");
    (void)strcat(res_string, suffix);

    free(*dest);
    *dest = res_string;
}