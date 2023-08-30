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
 * Author: tanyifeng
 * Create: 2018-11-1
 * Description: provide tar functions
 ********************************************************************************/
#define _GNU_SOURCE /* See feature_test_macros(7) */
#include "util_archive.h"
#include <archive.h>
#include <archive_entry.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <pwd.h>
#include <netdb.h>
#include <sys/mount.h>
#include <sys/capability.h>

#include <isula_libutils/log.h>
#include <isula_libutils/go_crc64.h>
#include <isula_libutils/storage_entry.h>

#include "error.h"
#include "map.h"
#include "path.h"
#include "stdbool.h"
#include "utils.h"
#include "utils_file.h"
#include "utils_string.h"
#include "buffer.h"

struct archive;
struct archive_entry;

#define ARCHIVE_READ_BUFFER_SIZE (10 * 1024)
#define ARCHIVE_WRITE_BUFFER_SIZE (10 * 1024)
#define TAR_DEFAULT_MODE 0600
#define TAR_DEFAULT_FLAG (O_WRONLY | O_CREAT | O_TRUNC)

#define WHITEOUT_PREFIX ".wh."
#define WHITEOUT_META_PREFIX ".wh..wh."
#define WHITEOUT_OPAQUEDIR ".wh..wh..opq"

struct archive_context {
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    pid_t pid;
    char *safe_dir;
};

struct archive_content_data {
    const struct io_read_wrapper *content;
    char buff[ARCHIVE_READ_BUFFER_SIZE];
};

static ssize_t read_content(struct archive *a, void *client_data, const void **buff)
{
    struct archive_content_data *mydata = client_data;

    memset(mydata->buff, 0, sizeof(mydata->buff));

    *buff = mydata->buff;

    return mydata->content->read(mydata->content->context, mydata->buff, sizeof(mydata->buff));
}

static void do_disable_unneccessary_caps()
{
    cap_t caps;
    caps = cap_get_proc();
    if (caps == NULL) {
        SYSERROR("Failed to do get cap");
        return;
    }
    cap_value_t cap_list[] = { CAP_SETUID };
    // clear all capabilities
    cap_clear(caps);

    if (cap_set_flag(caps, CAP_EFFECTIVE, sizeof(cap_list) / sizeof(cap_value_t), cap_list, CAP_SET) != 0) {
        SYSERROR("Failed to clear caps");
        return;
    }

    cap_set_proc(caps);
    cap_free(caps);
}

static int create_safedir(const char *dstdir, char **safe_dir)
{
    struct stat buf;
    char *isulad_tmpdir_env = NULL;
    char isula_tmpdir[PATH_MAX] = { 0 };
    char cleanpath[PATH_MAX] = { 0 };
    char tmp_dir[PATH_MAX] = { 0 };
    int nret;

    isulad_tmpdir_env = getenv("ISULAD_TMPDIR");
    if (!util_valid_str(isulad_tmpdir_env)) {
        // if not setted isulad tmpdir, just use /tmp
        isulad_tmpdir_env = "/tmp";
    }

    nret = snprintf(isula_tmpdir, PATH_MAX, "%s/isulad_tmpdir", isulad_tmpdir_env);
    if (nret < 0 || (size_t)nret >= PATH_MAX) {
        ERROR("Failed to snprintf");
        return -1;
    }

    if (util_clean_path(isula_tmpdir, cleanpath, sizeof(cleanpath)) == NULL) {
        ERROR("clean path for %s failed", isula_tmpdir);
        return -1;
    }

    nret = snprintf(tmp_dir, PATH_MAX, "%s/tar-chroot-XXXXXX", cleanpath);
    if (nret < 0 || (size_t)nret >= PATH_MAX) {
        ERROR("Failed to snprintf string");
        return -1;
    }

    if (stat(dstdir, &buf) < 0) {
        SYSERROR("Check chroot dir failed");
        return -1;
    }

    // ensure parent dir is exist
    if (util_mkdir_p(cleanpath, buf.st_mode) != 0) {
        return -1;
    }

    if (mkdtemp(tmp_dir) == NULL) {
        SYSERROR("Create temp dir failed");
        return -1;
    }

    // ensure mode of new safe dir, same to dstdir
    if (util_mkdir_p(tmp_dir, buf.st_mode) != 0) {
        return -1;
    }

    *safe_dir = util_strdup_s(tmp_dir);
    return 0;
}

// Unshare new mount namespace.
// Because bind mount usually makes safedir shared mount point,
// and sometimes it will cause "mount point explosion".
// E.g. concurrently execute isula cp /tmp/<XXX-File> <CONTAINER-ID>:<CONTAINER-PATH>
static int switch_new_mount_namespace(void)
{
    if (unshare(CLONE_NEWNS) != 0) {
        ERROR("Failed to unshare");
        fprintf(stderr, "Failed to unshare");
        return -1;
    }

    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) != 0) {
        ERROR("Failed to recursively turn root mount tree into dependent mount");
        fprintf(stderr, "Failed to recursively turn root mount tree into dependent mount");
        return -1;
    }

    return 0;
}

// fix loading of nsswitch based config inside chroot under glibc
static int do_safe_chroot(const char *dstdir, const char *safe_dir)
{
    if (mount(dstdir, safe_dir, "none", MS_BIND, NULL) != 0) {
        SYSERROR("Failed to mount dstdir %s to safedir %s", dstdir, safe_dir);
        fprintf(stderr, "Failed to mount dstdir %s to safedir %s: %s", dstdir, safe_dir, strerror(errno));
    }

    if (mount(safe_dir, safe_dir, "none", MS_BIND | MS_REMOUNT | MS_NOEXEC, NULL) != 0) {
        SYSERROR("Failed to mount dstdir %s to safedir %s", dstdir, safe_dir);
        fprintf(stderr, "Failed to mount dstdir %s to safedir %s: %s", dstdir, safe_dir, strerror(errno));
    }

    // don't call getpwnam
    // because it will change file with nobody uid/gid which copied from host to container
    // if nobody uid/gid is different between host and container

    // set No New Privileges
    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

    if (chroot(safe_dir) != 0) {
        SYSERROR("Failed to chroot to %s", safe_dir);
        fprintf(stderr, "Failed to chroot to %s: %s", safe_dir, strerror(errno));
        return -1;
    }

    do_disable_unneccessary_caps();

    return 0;
}

static bool overlay_whiteout_convert_read(struct archive_entry *entry, const char *dst_path, map_t *unpacked_path_map)
{
    bool do_write = true;
    char *base = NULL;
    char *dir = NULL;
    char *originalpath = NULL;

    base = util_path_base(dst_path);
    if (base == NULL) {
        ERROR("Failed to get base of %s", dst_path);
        goto out;
    }

    dir = util_path_dir(dst_path);
    if (dir == NULL) {
        ERROR("Failed to get dir of %s", dst_path);
        goto out;
    }

    if (strcmp(base, WHITEOUT_OPAQUEDIR) == 0) {
        if (setxattr(dir, "trusted.overlay.opaque", "y", 1, 0) != 0) {
            SYSERROR("Failed to set attr for dir %s", dir);
        }
        do_write = false;
        goto out;
    }

    if (strncmp(base, WHITEOUT_PREFIX, strlen(WHITEOUT_PREFIX)) == 0) {
        char *origin_base = &base[strlen(WHITEOUT_PREFIX)];
        originalpath = util_path_join(dir, origin_base);
        if (originalpath == NULL) {
            ERROR("Failed to get original path of %s", dst_path);
            goto out;
        }

        uid_t uid = archive_entry_uid(entry);
        gid_t gid = archive_entry_gid(entry);

        ERROR("mknod %s", originalpath);

        if (mknod(originalpath, S_IFCHR, 0) != 0) {
            SYSERROR("Failed to mknod for dir %s", originalpath);
        }

        if (chown(originalpath, uid, gid) != 0) {
            SYSERROR("Failed to chown for dir %s", originalpath);
        }

        do_write = false;
        goto out;
    }

out:
    free(base);
    free(dir);
    free(originalpath);
    return do_write;
}

static int copy_data(struct archive *ar, struct archive *aw)
{
    int r;
    const void *buff = NULL;
    size_t size;
    int64_t offset;

    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) {
            return ARCHIVE_OK;
        }
        if (r < ARCHIVE_OK) {
            return r;
        }
        r = archive_write_data_block(aw, buff, size, offset);
        if (r < ARCHIVE_OK) {
            ERROR("tar extraction error: %s, %s", archive_error_string(aw), strerror(archive_errno(aw)));
            return r;
        }
    }
}

static int remove_files_in_opq_dir(const char *dirpath, int recursive_depth, map_t *unpacked_path_map)
{
    struct dirent *pdirent = NULL;
    DIR *directory = NULL;
    int ret = 0;
    char fname[PATH_MAX] = { 0 };

    if ((recursive_depth + 1) > MAX_PATH_DEPTH) {
        ERROR("Reach max path depth: %s", dirpath);
        return -1;
    }

    directory = opendir(dirpath);
    if (directory == NULL) {
        ERROR("Failed to open %s", dirpath);
        return -1;
    }
    pdirent = readdir(directory);
    for (; pdirent != NULL; pdirent = readdir(directory)) {
        struct stat fstat;
        int pathname_len;

        if (!strcmp(pdirent->d_name, ".") || !strcmp(pdirent->d_name, "..")) {
            continue;
        }

        (void)memset(fname, 0, sizeof(fname));

        pathname_len = snprintf(fname, PATH_MAX, "%s/%s", dirpath, pdirent->d_name);
        if (pathname_len < 0 || (size_t)pathname_len >= PATH_MAX) {
            ERROR("Pathname too long");
            ret = -1;
            continue;
        }

        // not exist in unpacked paths map, just remove the path
        if (map_search(unpacked_path_map, (void *)fname) == NULL) {
            if (util_recursive_remove_path(fname) != 0) {
                ERROR("Failed to remove path %s", fname);
                ret = -1;
            }
            continue;
        }

        if (lstat(fname, &fstat) != 0) {
            ERROR("Failed to stat %s", fname);
            ret = -1;
            continue;
        }

        if (S_ISDIR(fstat.st_mode)) {
            if (remove_files_in_opq_dir(fname, recursive_depth + 1, unpacked_path_map) != 0) {
                ret = -1;
                continue;
            }
        }
    }

    if (closedir(directory) != 0) {
        ERROR("Failed to close directory %s", dirpath);
        ret = -1;
    }

    return ret;
}

static bool remove_whiteout_convert(struct archive_entry *entry, const char *dst_path, map_t *unpacked_path_map)
{
    bool do_write = true;
    char *base = NULL;
    char *dir = NULL;
    char *originalpath = NULL;

    base = util_path_base(dst_path);
    if (base == NULL) {
        ERROR("Failed to get base of %s", dst_path);
        goto out;
    }

    dir = util_path_dir(dst_path);
    if (dir == NULL) {
        ERROR("Failed to get dir of %s", dst_path);
        goto out;
    }

    if (strcmp(base, WHITEOUT_OPAQUEDIR) == 0) {
        if (remove_files_in_opq_dir(dir, 0, unpacked_path_map) != 0) {
            SYSERROR("Failed to remove files in opq dir %s", dir);
            goto out;
        }
        do_write = false;
        goto out;
    }

    if (strncmp(base, WHITEOUT_PREFIX, strlen(WHITEOUT_PREFIX)) == 0) {
        char *origin_base = &base[strlen(WHITEOUT_PREFIX)];
        originalpath = util_path_join(dir, origin_base);
        if (originalpath == NULL) {
            ERROR("Failed to get original path of %s", dst_path);
            goto out;
        }

        if (util_recursive_remove_path(originalpath) != 0) {
            ERROR("Failed to delete original path %s", originalpath);
            goto out;
        }

        do_write = false;
        goto out;
    }

out:
    free(base);
    free(dir);
    free(originalpath);
    return do_write;
}

typedef bool (*whiteout_convert_call_back_t)(struct archive_entry *entry, const char *dst_path,
                                             map_t *unpacked_path_map);

struct whiteout_convert_map {
    whiteout_format_type type;
    whiteout_convert_call_back_t wh_cb;
};

struct whiteout_convert_map g_wh_cb_map[] = { { OVERLAY_WHITEOUT_FORMATE, overlay_whiteout_convert_read },
    { REMOVE_WHITEOUT_FORMATE, remove_whiteout_convert }
};

static whiteout_convert_call_back_t get_whiteout_convert_cb(whiteout_format_type whiteout_type)
{
    size_t i = 0;

    for (i = 0; i < sizeof(g_wh_cb_map) / sizeof(g_wh_cb_map[0]); i++) {
        if (whiteout_type == g_wh_cb_map[i].type) {
            return g_wh_cb_map[i].wh_cb;
        }
    }

    return NULL;
}

static char *to_relative_path(const char *path)
{
    char *dst_path = NULL;

    if (path != NULL && path[0] == '/') {
        if (strcmp(path, "/") == 0) {
            dst_path = util_strdup_s(".");
        } else {
            dst_path = util_strdup_s(path + 1);
        }
    } else {
        dst_path = util_strdup_s(path);
    }

    return dst_path;
}

static int rebase_pathname(struct archive_entry *entry, const char *src_base, const char *dst_base)
{
    int nret = 0;
    const char *pathname = archive_entry_pathname(entry);
    char path[PATH_MAX] = { 0 };

    if (src_base == NULL || dst_base == NULL || !util_has_prefix(pathname, src_base)) {
        return 0;
    }

    nret = snprintf(path, sizeof(path), "%s%s", dst_base, pathname + strlen(src_base));
    if (nret < 0 || (size_t)nret >= sizeof(path)) {
        ERROR("snprintf %s%s failed", dst_base, pathname + strlen(src_base));
        fprintf(stderr, "snprintf %s%s failed", dst_base, pathname + strlen(src_base));
        return -1;
    }

    archive_entry_set_pathname(entry, path);

    return 0;
}

static char *update_entry_for_pathname(struct archive_entry *entry, const char *src_base, const char *dst_base)
{
    char *dst_path = NULL;
    const char *pathname = NULL;

    if (rebase_pathname(entry, src_base, dst_base) != 0) {
        return NULL;
    }

    pathname = archive_entry_pathname(entry);
    if (pathname == NULL) {
        ERROR("Failed to get archive entry path name");
        fprintf(stderr, "Failed to get archive entry path name");
        return NULL;
    }

    // if path in archive is absolute, we need to translate it to relative because
    // libarchive can not support absolute path when unpack
    dst_path = to_relative_path(pathname);
    if (dst_path == NULL) {
        ERROR("translate %s to relative path failed", pathname);
        fprintf(stderr, "translate %s to relative path failed", pathname);
        goto out;
    }

    archive_entry_set_pathname(entry, dst_path);
out:

    return dst_path;
}

static int rebase_hardlink(struct archive_entry *entry, const char *src_base, const char *dst_base)
{
    int nret = 0;
    const char *linkname = NULL;
    char path[PATH_MAX] = { 0 };

    linkname = archive_entry_hardlink(entry);
    if (linkname == NULL) {
        return 0;
    }

    if (src_base == NULL || dst_base == NULL || !util_has_prefix(linkname, src_base)) {
        return 0;
    }

    nret = snprintf(path, sizeof(path), "%s%s", dst_base, linkname + strlen(src_base));
    if (nret < 0 || (size_t)nret >= sizeof(path)) {
        ERROR("snprintf %s%s failed", dst_base, linkname + strlen(src_base));
        fprintf(stderr, "snprintf %s%s failed", dst_base, linkname + strlen(src_base));
        return -1;
    }

    archive_entry_set_hardlink(entry, path);

    return 0;
}

// if dst path exits, we just want to remove and replace it.
// exception: when the exited dstpath is directory and the file from the layer is also a directory.
static void try_to_replace_exited_dst(const char *dst_path, struct archive_entry *entry)
{
    struct stat s;
    int nret;

    nret = lstat(dst_path, &s);
    if (nret < 0) {
        return;
    }

    if (S_ISDIR(s.st_mode) && archive_entry_filetype(entry) == AE_IFDIR) {
        return;
    }

    if (util_recursive_remove_path(dst_path) != 0) {
        ERROR("Failed to remove path %s while unpack", dst_path);
    }
}

int archive_unpack_handler(const struct io_read_wrapper *content, const struct archive_options *options)
{
    int ret = 0;
    struct archive *a = NULL;
    struct archive *ext = NULL;
    struct archive_content_data *mydata = NULL;
    struct archive_entry *entry = NULL;
    char *dst_path = NULL;
    int flags;
    whiteout_convert_call_back_t wh_handle_cb = NULL;
    map_t *unpacked_path_map = NULL; // used for hanling opaque dir, marke paths had been unpacked

    unpacked_path_map = map_new(MAP_STR_BOOL, MAP_DEFAULT_CMP_FUNC, MAP_DEFAULT_FREE_FUNC);
    if (unpacked_path_map == NULL) {
        ERROR("Out of memory");
        fprintf(stderr, "Out of memory");
        ret = -1;
        goto out;
    }

    mydata = util_common_calloc_s(sizeof(struct archive_content_data));
    if (mydata == NULL) {
        ERROR("Memory out");
        fprintf(stderr, "Memory out");
        ret = -1;
        goto out;
    }
    mydata->content = content;

    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_OWNER;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;
    flags |= ARCHIVE_EXTRACT_SECURE_SYMLINKS;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;
    flags |= ARCHIVE_EXTRACT_XATTR;
    flags |= ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS;

    a = archive_read_new();
    if (a == NULL) {
        ERROR("archive read new failed");
        fprintf(stderr, "archive read new failed");
        ret = -1;
        goto out;
    }
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    ext = archive_write_disk_new();
    if (ext == NULL) {
        ERROR("archive write disk new failed");
        fprintf(stderr, "archive write disk new failed");
        ret = -1;
        goto out;
    }
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    ret = archive_read_open(a, mydata, NULL, read_content, NULL);
    if (ret != 0) {
        ERROR("Failed to open archive: %s", archive_error_string(a));
        fprintf(stderr, "Failed to open archive: %s", strerror(errno));
        ret = -1;
        goto out;
    }

    wh_handle_cb = get_whiteout_convert_cb(options->whiteout_format);

    for (;;) {
        free(dst_path);
        dst_path = NULL;
        ret = archive_read_next_header(a, &entry);
        if (ret == ARCHIVE_EOF) {
            break;
        }

        if (ret != ARCHIVE_OK) {
            ERROR("Warning reading tar header: %s, %s", archive_error_string(a), strerror(archive_errno(a)));
            (void)fprintf(stderr, "Warning reading tar header: %s, %s", archive_error_string(a),
                          strerror(archive_errno(a)));
            ret = -1;
            goto out;
        }

        dst_path = update_entry_for_pathname(entry, options->src_base, options->dst_base);
        if (dst_path == NULL) {
            ERROR("Failed to update pathname");
            fprintf(stderr, "Failed to update pathname");
            ret = -1;
            goto out;
        }

        ret = rebase_hardlink(entry, options->src_base, options->dst_base);
        if (ret != 0) {
            ERROR("Failed to rebase hardlink");
            fprintf(stderr, "Failed to rebase hardlink");
            ret = -1;
            goto out;
        }

        if (wh_handle_cb != NULL && !wh_handle_cb(entry, dst_path, unpacked_path_map)) {
            continue;
        }

        try_to_replace_exited_dst(dst_path, entry);

#ifdef ENABLE_USERNS_REMAP
        archive_entry_set_uid(entry, archive_entry_uid(entry) + options->uid);
        archive_entry_set_gid(entry, archive_entry_gid(entry) + options->gid);
#endif

        ret = archive_write_header(ext, entry);
        if (ret != ARCHIVE_OK) {
            ERROR("Fail to handle tar header: %s, %s", archive_error_string(ext), strerror(archive_errno(ext)));
            (void)fprintf(stderr, "Fail to handle tar header: %s, %s", archive_error_string(ext),
                          strerror(archive_errno(ext)));
            ret = -1;
            goto out;
        } else if (archive_entry_size(entry) > 0) {
            ret = copy_data(a, ext);
            if (ret != ARCHIVE_OK) {
                ERROR("Failed to do copy tar data: %s, %s", archive_error_string(ext), strerror(archive_errno(ext)));
                (void)fprintf(stderr, "Failed to do copy tar data: %s, %s", archive_error_string(ext),
                              strerror(archive_errno(ext)));
                ret = -1;
                goto out;
            }
        }
        ret = archive_write_finish_entry(ext);
        if (ret != ARCHIVE_OK) {
            ERROR("Failed to freeing archive entry: %s, %s", archive_error_string(ext), strerror(archive_errno(ext)));
            (void)fprintf(stderr, "Failed to freeing archive entry: %s, %s", archive_error_string(ext),
                          strerror(archive_errno(ext)));
            ret = -1;
            goto out;
        }

        bool b = true;
        if (!map_replace(unpacked_path_map, (void *)dst_path, (void *)(&b))) {
            ERROR("Failed to replace unpacked path map element");
            fprintf(stderr, "Failed to replace unpacked path map element");
            ret = -1;
            goto out;
        }
    }

    ret = 0;

out:
    map_free(unpacked_path_map);
    free(dst_path);
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
    free(mydata);
    return ret;
}

static void close_archive_pipes_fd(int *pipes, size_t pipe_size)
{
    size_t i = 0;

    for (i = 0; i < pipe_size; i++) {
        if (pipes[i] >= 0) {
            close(pipes[i]);
            pipes[i] = -1;
        }
    }
}

static void set_child_process_pdeathsig(void)
{
    if (prctl(PR_SET_PDEATHSIG, SIGKILL) < 0) {
        SYSERROR("Failed to set child process pdeathsig");
    }
}

int archive_unpack(const struct io_read_wrapper *content, const char *dstdir, const struct archive_options *options,
                   char **errmsg)
{
    int ret = 0;
    pid_t pid = -1;
    int keepfds[] = { -1, -1, -1 };
    int pipe_stderr[2] = { -1, -1 };
    char errbuf[BUFSIZ + 1] = { 0 };
    char *safe_dir = NULL;

    if (create_safedir(dstdir, &safe_dir) != 0) {
        ERROR("Prepare safe dir failed");
        return -1;
    }

    if (pipe2(pipe_stderr, O_CLOEXEC) != 0) {
        ERROR("Failed to create pipe");
        ret = -1;
        goto cleanup;
    }

    pid = fork();
    if (pid == (pid_t) -1) {
        ERROR("Failed to fork: %s", strerror(errno));
        goto cleanup;
    }

    if (pid == (pid_t)0) {
        set_child_process_pdeathsig();

        keepfds[0] = isula_libutils_get_log_fd();
        keepfds[1] = *(int *)(content->context);
        keepfds[2] = pipe_stderr[1];

        if (switch_new_mount_namespace() != 0) {
            ret = -1;
            goto child_out;
        }

        ret = util_check_inherited_exclude_fds(true, keepfds, 3);
        if (ret != 0) {
            ERROR("Failed to close fds.");
            fprintf(stderr, "Failed to close fds.");
            ret = -1;
            goto child_out;
        }

        // child process, dup2 pipe_for_read[1] to stderr,
        if (dup2(pipe_stderr[1], 2) < 0) {
            ERROR("Dup fd error: %s", strerror(errno));
            ret = -1;
            goto child_out;
        }

        if (do_safe_chroot(dstdir, safe_dir) != 0) {
            SYSERROR("Failed to chroot to %s", safe_dir);
            fprintf(stderr, "Failed to chroot to %s: %s", safe_dir, strerror(errno));
            ret = -1;
            goto child_out;
        }

        if (chdir("/") != 0) {
            SYSERROR("Failed to chroot to /");
            fprintf(stderr, "Failed to chroot to /: %s", strerror(errno));
            ret = -1;
            goto child_out;
        }

        ret = archive_unpack_handler(content, options);

child_out:
        if (ret != 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    close(pipe_stderr[1]);
    pipe_stderr[1] = -1;

    ret = util_wait_for_pid(pid);
    if (ret != 0) {
        ERROR("Wait archive_untar_handler failed with error:%s", strerror(errno));
        fcntl(pipe_stderr[0], F_SETFL, O_NONBLOCK);
        if (util_read_nointr(pipe_stderr[0], errbuf, BUFSIZ) < 0) {
            ERROR("read error message from child failed");
        }
    }

cleanup:
    close_archive_pipes_fd(pipe_stderr, 2);
    if (errmsg != NULL && strlen(errbuf) != 0) {
        *errmsg = util_strdup_s(errbuf);
    }
    if (umount(safe_dir) != 0) {
        ERROR("Failed to umount target %s", safe_dir);
    }
    if (util_path_remove(safe_dir) != 0) {
        ERROR("Failed to remove path %s", safe_dir);
    }
    free(safe_dir);
    return ret;
}

bool valid_archive_format(const char *file)
{
    int ret = ARCHIVE_FAILED;
    struct archive *read_archive = NULL;
    struct archive_entry *entry = NULL;

    if (file == NULL) {
        ERROR("Invalid NULL file path when checking archive format");
        return false;
    }

    read_archive = archive_read_new();
    if (read_archive == NULL) {
        ERROR("archive read new failed");
        return false;
    }

    ret = archive_read_support_filter_all(read_archive);
    if (ret != ARCHIVE_OK) {
        ERROR("Failed to set archive read support filter all, result is %d, errmsg: %s, %s", ret,
              archive_error_string(read_archive), strerror(archive_errno(read_archive)));
        goto out;
    }

    ret = archive_read_support_format_all(read_archive);
    if (ret != ARCHIVE_OK) {
        ERROR("Failed to set archive read support format all, result is %d, errmsg: %s, %s", ret,
              archive_error_string(read_archive), strerror(archive_errno(read_archive)));
        goto out;
    }

    ret = archive_read_open_filename(read_archive, file, ARCHIVE_READ_BUFFER_SIZE);
    if (ret != ARCHIVE_OK) {
        ERROR("Failed to open archive %s: %s, %s", file, archive_error_string(read_archive),
              strerror(archive_errno(read_archive)));
        goto out;
    }

    // format code upated when archive_read_next_header is called
    ret = archive_read_next_header(read_archive, &entry);
    if (ret == ARCHIVE_EOF) {
        ERROR("Invalid empty archive, it's not archive format");
        goto out;
    }
    if (ret != ARCHIVE_OK) {
        ERROR("Failed to read next header for file %s: %s, %s", file, archive_error_string(read_archive),
              strerror(archive_errno(read_archive)));
        goto out;
    }

out:
    if (archive_read_free(read_archive) != ARCHIVE_OK) {
        ERROR("Failed to free archive %s: %s, %s", file, archive_error_string(read_archive),
              strerror(archive_errno(read_archive)));
    }
    read_archive = NULL;

    return (ret == ARCHIVE_OK);
}

static int copy_data_between_archives(struct archive *ar, struct archive *aw)
{
    int ret = ARCHIVE_FAILED;
    char *buff = NULL;
    ssize_t size = 0;

    buff = util_common_calloc_s(ARCHIVE_BLOCK_SIZE);
    if (buff == NULL) {
        ERROR("out of memory");
        fprintf(stderr, "out of memory");
        return ARCHIVE_FAILED;
    }

    for (;;) {
        size = archive_read_data(ar, buff, ARCHIVE_BLOCK_SIZE);
        if (size == 0) {
            ret = ARCHIVE_OK;
            goto out;
        }
        if (size < 0) {
            ERROR("tar archive read result %d, error: %s, %s", ret, archive_error_string(ar),
                  strerror(archive_errno(ar)));
            (void)fprintf(stderr, "tar archive read result %d, error: %s, %s", ret, archive_error_string(ar),
                          strerror(archive_errno(ar)));
            ret = ARCHIVE_FAILED;
            goto out;
        }
        ret = archive_write_data(aw, buff, size);
        if (ret < ARCHIVE_OK) {
            ERROR("tar archive write result %d, error: %s, %s", ret, archive_error_string(aw),
                  strerror(archive_errno(aw)));
            (void)fprintf(stderr, "tar archive write result %d, error: %s, %s", ret, archive_error_string(aw),
                          strerror(archive_errno(aw)));
            goto out;
        }
    }

out:
    free(buff);
    return ret;
}

int update_entry_for_hardlink(map_t *map_link, struct archive_entry *entry, const char *src_base, const char *dst_base)
{
    const char *path = archive_entry_pathname(entry);
    char *linkname = NULL;
    unsigned int nlink = archive_entry_nlink(entry);
    int ino = archive_entry_ino(entry);
    const char *hardlink = archive_entry_hardlink(entry);

    if (hardlink != NULL && rebase_hardlink(entry, src_base, dst_base) != 0) {
        return -1;
    }

    // try to use hardlink to reduce tar size
    if (nlink <= 1) {
        return 0;
    }

    linkname = map_search(map_link, (void *)&ino);
    if (linkname == NULL) {
        linkname = (char *)path;
        if (!map_insert(map_link, (void *)&ino, linkname)) {
            ERROR("insert to map failed");
            fprintf(stderr, "insert to map failed");
            return -1;
        }
        return 0;
    }

    archive_entry_set_size(entry, 0);
    archive_entry_set_hardlink(entry, linkname);

    return 0;
}

static void link_kvfree(void *key, void *value)
{
    free(key);
    free(value);
    return;
}

int tar_handler(struct archive *r, struct archive *w, const char *src_base, const char *dst_base)
{
    int ret = ARCHIVE_OK;
    struct archive_entry *entry = NULL;
    map_t *map_link = NULL;
    char *pathname = NULL;

    map_link = map_new(MAP_INT_STR, MAP_DEFAULT_CMP_FUNC, link_kvfree);
    if (map_link == NULL) {
        ERROR("out of memory");
        fprintf(stderr, "out of memory\n");
        return -1;
    }

    for (;;) {
        ret = archive_read_next_header(r, &entry);
        if (ret == ARCHIVE_EOF) {
            ret = ARCHIVE_OK;
            break;
        }

        if (ret != ARCHIVE_OK) {
            ERROR("read from disk failed: %s, %s", archive_error_string(r), strerror(archive_errno(r)));
            (void)fprintf(stderr, "read from disk failed: %s, %s", archive_error_string(r), strerror(archive_errno(r)));
            break;
        }

        pathname = update_entry_for_pathname(entry, src_base, dst_base);
        if (pathname == NULL) {
            ret = ARCHIVE_FAILED;
            break;
        }
        free(pathname);
        pathname = NULL;

        if (update_entry_for_hardlink(map_link, entry, src_base, dst_base) != 0) {
            ret = ARCHIVE_FAILED;
            break;
        }
        ret = archive_write_header(w, entry);
        if (ret != ARCHIVE_OK) {
            ERROR("Fail to write tar header: %s, %s\nlink:%s target:%s", archive_error_string(w),
                  strerror(archive_errno(w)), archive_entry_pathname(entry), archive_entry_hardlink(entry));
            (void)fprintf(stderr, "Fail to write tar header: %s, %s\nlink:%s target:%s\n", archive_error_string(w),
                          strerror(archive_errno(w)), archive_entry_pathname(entry), archive_entry_hardlink(entry));
            break;
        }

        if (archive_entry_size(entry) > 0) {
            ret = copy_data_between_archives(r, w);
            if (ret != ARCHIVE_OK) {
                ERROR("Failed to do copy data: %s, %s", archive_error_string(w), strerror(archive_errno(w)));
                (void)fprintf(stderr, "Failed to do copy data: %s, %s\n", archive_error_string(w),
                              strerror(archive_errno(w)));
                break;
            }
        }

        ret = archive_write_finish_entry(w);
        if (ret != ARCHIVE_OK) {
            ERROR("Failed to freeing archive entry: %s, %s", archive_error_string(w), strerror(archive_errno(w)));
            (void)fprintf(stderr, "Failed to freeing archive entry: %s, %s\n", archive_error_string(w),
                          strerror(archive_errno(w)));
            break;
        }

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            ret = archive_read_disk_descend(r);
            if (ret != ARCHIVE_OK) {
                ERROR("read disk descend failed: %s, %s", archive_error_string(w), strerror(archive_errno(w)));
                (void)fprintf(stderr, "read disk descend failed: %s, %s\n", archive_error_string(w),
                              strerror(archive_errno(w)));
                break;
            }
        }
    }

    map_free(map_link);

    return ret;
}

static ssize_t stream_write_data(struct archive *a, void *client_data, const void *buffer, size_t length)
{
    struct io_write_wrapper *writer = (struct io_write_wrapper *)client_data;
    size_t written_length = 0;
    size_t size = 0;

    if (length > SSIZE_MAX) {
        ERROR("Too large data to write.");
        return -1;
    }

    while (length > written_length) {
        if (length - written_length > ARCHIVE_WRITE_BUFFER_SIZE) {
            size = ARCHIVE_WRITE_BUFFER_SIZE;
        } else {
            size = length - written_length;
        }
        if (!writer->write_func(writer->context, (const char *)buffer + written_length, size)) {
            ERROR("write stream failed");
            return -1;
        }
        written_length += size;
    }

    return (ssize_t)written_length;
}

static int tar_all(const struct io_write_wrapper *writer, const char *tar_dir, const char *src_base,
                   const char *dst_base)
{
    struct archive *r = NULL;
    struct archive *w = NULL;
    int ret = ARCHIVE_OK;

    r = archive_read_disk_new();
    if (r == NULL) {
        ERROR("archive read disk new failed");
        fprintf(stderr, "archive read disk new failed");
        return -1;
    }
    archive_read_disk_set_standard_lookup(r);
    archive_read_disk_set_symlink_physical(r);
    archive_read_disk_set_behavior(r, ARCHIVE_READDISK_NO_TRAVERSE_MOUNTS);
    ret = archive_read_disk_open(r, tar_dir);
    if (ret != ARCHIVE_OK) {
        ERROR("open archive read failed: %s, %s", archive_error_string(r), strerror(archive_errno(r)));
        (void)fprintf(stderr, "open archive read failed: %s, %s\n", archive_error_string(r),
                      strerror(archive_errno(r)));
        goto out;
    }

    w = archive_write_new();
    if (w == NULL) {
        ERROR("archive write new failed");
        fprintf(stderr, "archive write new failed");
        ret = ARCHIVE_FAILED;
        goto out;
    }
    archive_write_set_format_pax(w);
    archive_write_set_options(w, "xattrheader=SCHILY");
    ret = archive_write_open(w, (void *)writer, NULL, stream_write_data, NULL);
    if (ret != ARCHIVE_OK) {
        ERROR("open archive write failed: %s, %s", archive_error_string(w), strerror(archive_errno(w)));
        (void)fprintf(stderr, "open archive write failed: %s, %s\n", archive_error_string(w),
                      strerror(archive_errno(w)));
        goto out;
    }

    ret = tar_handler(r, w, src_base, dst_base);

out:
    archive_free(r);
    archive_free(w);

    return (ret == ARCHIVE_OK) ? 0 : -1;
}

static ssize_t fd_write(void *context, const void *data, size_t len)
{
    return util_write_nointr(*(int *)context, data, len);
}

int archive_chroot_tar(char *path, char *file, char **errmsg)
{
    struct io_write_wrapper pipe_context = { 0 };
    int ret = 0;
    pid_t pid;
    int pipe_for_read[2] = { -1, -1 };
    int keepfds[] = { -1, -1 };
    char errbuf[BUFSIZ + 1] = { 0 };
    int fd = 0;
    char *safe_dir = NULL;

    if (create_safedir(path, &safe_dir) != 0) {
        ERROR("Prepare safe dir failed");
        return -1;
    }

    if (pipe2(pipe_for_read, O_CLOEXEC) != 0) {
        ERROR("Failed to create pipe");
        ret = -1;
        goto cleanup;
    }

    pid = fork();
    if (pid == (pid_t) -1) {
        ERROR("Failed to fork()");
        ret = -1;
        goto cleanup;
    }

    if (pid == (pid_t)0) {
        set_child_process_pdeathsig();

        keepfds[0] = isula_libutils_get_log_fd();
        keepfds[1] = pipe_for_read[1];

        if (switch_new_mount_namespace() != 0) {
            ret = -1;
            goto child_out;
        }

        ret = util_check_inherited_exclude_fds(true, keepfds, 2);
        if (ret != 0) {
            ERROR("Failed to close fds.");
            ret = -1;
            goto child_out;
        }

        // child process, dup2 pipe_for_read[1] to stderr,
        if (dup2(pipe_for_read[1], 2) < 0) {
            ERROR("Dup fd error: %s", strerror(errno));
            ret = -1;
            goto child_out;
        }

        fd = open(file, TAR_DEFAULT_FLAG, TAR_DEFAULT_MODE);
        if (fd < 0) {
            ERROR("Failed to open file %s for export: %s", file, strerror(errno));
            fprintf(stderr, "Failed to open file %s for export: %s\n", file, strerror(errno));
            ret = -1;
            goto child_out;
        }

        if (do_safe_chroot(path, safe_dir) != 0) {
            ERROR("Failed to chroot to %s", safe_dir);
            fprintf(stderr, "Failed to chroot to %s\n", safe_dir);
            ret = -1;
            goto child_out;
        }

        if (chdir("/") != 0) {
            ERROR("Failed to chroot to /");
            fprintf(stderr, "Failed to chroot to /\n");
            ret = -1;
            goto child_out;
        }

        pipe_context.context = (void *)&fd;
        pipe_context.write_func = fd_write;
        ret = tar_all(&pipe_context, ".", ".", NULL);

child_out:

        if (ret != 0) {
            exit(EXIT_FAILURE);
        } else {
            exit(EXIT_SUCCESS);
        }
    }
    close(pipe_for_read[1]);
    pipe_for_read[1] = -1;

    ret = util_wait_for_pid(pid);
    if (ret != 0) {
        ERROR("tar failed");
        fcntl(pipe_for_read[0], F_SETFL, O_NONBLOCK);
        if (util_read_nointr(pipe_for_read[0], errbuf, BUFSIZ) < 0) {
            ERROR("read error message from child failed");
        }
    }

cleanup:
    close_archive_pipes_fd(pipe_for_read, 2);
    if (errmsg != NULL && strlen(errbuf) != 0) {
        *errmsg = util_strdup_s(errbuf);
    }
    if (umount(safe_dir) != 0) {
        ERROR("Failed to umount target %s", safe_dir);
    }
    if (util_path_remove(safe_dir) != 0) {
        ERROR("Failed to remove path %s", safe_dir);
    }
    free(safe_dir);
    return ret;
}

static ssize_t pipe_read(void *context, void *buf, size_t len)
{
    return util_read_nointr(*(int *)context, buf, len);
}

static ssize_t archive_context_write(const void *context, const void *buf, size_t len)
{
    struct archive_context *ctx = (struct archive_context *)context;
    if (ctx == NULL) {
        return -1;
    }
    if (ctx->stdin_fd >= 0) {
        return util_write_nointr(ctx->stdin_fd, buf, len);
    }
    return 0;
}

static ssize_t pipe_write(void *context, const void *data, size_t len)
{
    return util_write_nointr(*(int *)context, data, len);
}

static ssize_t archive_context_read(void *context, void *buf, size_t len)
{
    struct archive_context *ctx = (struct archive_context *)context;
    if (ctx == NULL) {
        return -1;
    }
    if (ctx->stdout_fd >= 0) {
        return util_read_nointr(ctx->stdout_fd, buf, len);
    }
    return 0;
}

static int close_wait_pid(struct archive_context *ctx, int *status)
{
    int ret = 0;

    // close stdin and stdout first, this will make sure the process of tar exit.
    if (ctx->stdin_fd >= 0) {
        close(ctx->stdin_fd);
    }

    if (ctx->stdout_fd >= 0) {
        close(ctx->stdout_fd);
    }

    if (ctx->pid > 0) {
        if (waitpid(ctx->pid, status, 0) != ctx->pid) {
            ERROR("Failed to wait pid %d", ctx->pid);
            ret = -1;
        }
    }

    return ret;
}

static int archive_context_close(void *context, char **err)
{
    int ret = 0;
    int status = 0;
    char *reason = NULL;
    ssize_t size_read = 0;
    char buffer[BUFSIZ + 1] = { 0 };
    struct archive_context *ctx = (struct archive_context *)context;
    char *marshaled = NULL;

    if (ctx == NULL) {
        return 0;
    }

    ret = close_wait_pid(ctx, &status);

    if (WIFSIGNALED((unsigned int)status)) {
        status = WTERMSIG(status);
        reason = "signaled";
    } else if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);
        reason = "exited";
    } else {
        reason = "unknown";
    }
    if (ctx->stderr_fd >= 0) {
        size_read = util_read_nointr(ctx->stderr_fd, buffer, BUFSIZ);
        if (size_read > 0) {
            reason = buffer;
            marshaled = util_marshal_string(buffer);
            if (marshaled == NULL) {
                ERROR("Can not marshal json buffer: %s", buffer);
            } else {
                reason = marshaled;
            }
        }
        close(ctx->stderr_fd);
    }

    if (size_read > 0 || status != 0) {
        format_errorf(err, "tar exited with status %d: %s", status, reason);
        ret = -1;
    }

    if (ctx->safe_dir != NULL) {
        if (umount(ctx->safe_dir) != 0) {
            ERROR("Failed to umount target %s", ctx->safe_dir);
        }
        if (util_path_remove(ctx->safe_dir) != 0) {
            ERROR("Failed to remove path %s", ctx->safe_dir);
        }
        free(ctx->safe_dir);
        ctx->safe_dir = NULL;
    }
    free(marshaled);
    free(ctx);
    return ret;
}

int archive_chroot_untar_stream(const struct io_read_wrapper *context, const char *chroot_dir, const char *untar_dir,
                                const char *src_base, const char *dst_base, char **errmsg)
{
    struct io_read_wrapper pipe_context = { 0 };
    int pipe_stream[2] = { -1, -1 };
    int pipe_stderr[2] = { -1, -1 };
    int keepfds[] = { -1, -1, -1 };
    int ret = -1;
    int cret = 0;
    pid_t pid;
    struct archive_context *ctx = NULL;
    char *buf = NULL;
    size_t buf_len = ARCHIVE_BLOCK_SIZE;
    ssize_t read_len;
    struct archive_options options = { .whiteout_format = NONE_WHITEOUT_FORMATE,
               .src_base = src_base,
               .dst_base = dst_base
    };
    char *safe_dir = NULL;

    if (create_safedir(chroot_dir, &safe_dir) != 0) {
        ERROR("Prepare safe dir failed");
        return -1;
    }

    if (pipe(pipe_stderr) != 0) {
        ERROR("Failed to create pipe: %s", strerror(errno));
        goto cleanup;
    }
    if (pipe(pipe_stream) != 0) {
        ERROR("Failed to create pipe: %s", strerror(errno));
        goto cleanup;
    }

    pid = fork();
    if (pid == (pid_t) -1) {
        ERROR("Failed to fork: %s", strerror(errno));
        goto cleanup;
    }

    if (pid == (pid_t)0) {
        set_child_process_pdeathsig();

        keepfds[0] = isula_libutils_get_log_fd();
        keepfds[1] = pipe_stderr[1];
        keepfds[2] = pipe_stream[0];

        if (switch_new_mount_namespace() != 0) {
            ret = -1;
            goto child_out;
        }

        ret = util_check_inherited_exclude_fds(true, keepfds, 3);
        if (ret != 0) {
            ERROR("Failed to close fds.");
            ret = -1;
            goto child_out;
        }

        // child process, dup2 pipe_stderr[1] to stderr,
        if (dup2(pipe_stderr[1], 2) < 0) {
            ERROR("Dup fd error: %s", strerror(errno));
            ret = -1;
            goto child_out;
        }

        if (do_safe_chroot(chroot_dir, safe_dir) != 0) {
            SYSERROR("Failed to chroot to %s", safe_dir);
            ret = -1;
            goto child_out;
        }

        if (chdir("/") != 0 || chdir(untar_dir) != 0) {
            SYSERROR("Failed to chdir to %s", untar_dir);
            fprintf(stderr, "Failed to chdir to %s", untar_dir);
            ret = -1;
            goto child_out;
        }

        pipe_context.context = (void *)&pipe_stream[0];
        pipe_context.read = pipe_read;
        ret = archive_unpack_handler(&pipe_context, &options);

child_out:
        if (ret != 0) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    close(pipe_stderr[1]);
    pipe_stderr[1] = -1;
    close(pipe_stream[0]);
    pipe_stream[0] = -1;

    buf = util_common_calloc_s(buf_len);
    if (buf == NULL) {
        ERROR("Out of memory");
        goto cleanup;
    }

    ctx = util_common_calloc_s(sizeof(struct archive_context));
    if (ctx == NULL) {
        goto cleanup;
    }

    ctx->pid = pid;
    ctx->stdin_fd = pipe_stream[1];
    pipe_stream[1] = -1;
    ctx->stdout_fd = -1;
    ctx->stderr_fd = pipe_stderr[0];
    pipe_stderr[0] = -1;

    read_len = context->read(context->context, buf, buf_len);
    while (read_len > 0) {
        ssize_t writed_len = archive_context_write(ctx, buf, (size_t)read_len);
        if (writed_len < 0) {
            DEBUG("Tar may exited: %s", strerror(errno));
            break;
        }
        read_len = context->read(context->context, buf, buf_len);
    }

    ret = 0;

cleanup:
    free(buf);
    cret = archive_context_close(ctx, errmsg);
    ret = (cret != 0) ? cret : ret;
    close_archive_pipes_fd(pipe_stderr, 2);
    close_archive_pipes_fd(pipe_stream, 2);
    if (umount(safe_dir) != 0) {
        ERROR("Failed to umount target %s", safe_dir);
    }
    if (util_path_remove(safe_dir) != 0) {
        ERROR("Failed to remove path %s", safe_dir);
    }
    free(safe_dir);

    return ret;
}

int archive_chroot_tar_stream(const char *chroot_dir, const char *tar_path, const char *src_base, const char *dst_base,
                              struct io_read_wrapper *reader)
{
    struct io_write_wrapper pipe_context = { 0 };
    int keepfds[] = { -1, -1, -1 };
    int pipe_stderr[2] = { -1, -1 };
    int pipe_stream[2] = { -1, -1 };
    int ret = -1;
    pid_t pid;
    struct archive_context *ctx = NULL;
    char *safe_dir = NULL;

    if (create_safedir(chroot_dir, &safe_dir) != 0) {
        ERROR("Prepare safe dir failed");
        return -1;
    }

    if (pipe(pipe_stderr) != 0) {
        ERROR("Failed to create pipe: %s", strerror(errno));
        goto free_out;
    }
    if (pipe(pipe_stream) != 0) {
        ERROR("Failed to create pipe: %s", strerror(errno));
        goto free_out;
    }

    pid = fork();
    if (pid == (pid_t) -1) {
        ERROR("Failed to fork: %s", strerror(errno));
        goto free_out;
    }

    if (pid == (pid_t)0) {
        char *tar_dir_name = NULL;
        char *tar_base_name = NULL;

        set_child_process_pdeathsig();
        if (switch_new_mount_namespace() != 0) {
            ret = -1;
            goto child_out;
        }

        keepfds[0] = isula_libutils_get_log_fd();
        keepfds[1] = pipe_stderr[1];
        keepfds[2] = pipe_stream[1];
        ret = util_check_inherited_exclude_fds(true, keepfds, 3);
        if (ret != 0) {
            ERROR("Failed to close fds.");
            ret = -1;
            goto child_out;
        }

        // child process, dup2 pipe_stderr[1] to stderr,
        if (dup2(pipe_stderr[1], 2) < 0) {
            ERROR("Dup fd error: %s", strerror(errno));
            ret = -1;
            goto child_out;
        }

        if (do_safe_chroot(chroot_dir, safe_dir) != 0) {
            ERROR("Failed to chroot to %s", safe_dir);
            fprintf(stderr, "Failed to chroot to %s\n", safe_dir);
            ret = -1;
            goto child_out;
        }

        if (util_split_dir_and_base_name(tar_path, &tar_dir_name, &tar_base_name) != 0) {
            ERROR("Failed to split %s", tar_path);
            fprintf(stderr, "Failed to split %s\n", tar_path);
            ret = -1;
            goto child_out;
        }

        if (chdir("/") != 0 || chdir(tar_dir_name) != 0) {
            ERROR("Failed to chdir to %s", tar_dir_name);
            fprintf(stderr, "Failed to chdir to %s\n", tar_dir_name);
            ret = -1;
            goto child_out;
        }

        pipe_context.context = (void *)&pipe_stream[1];
        pipe_context.write_func = pipe_write;
        ret = tar_all(&pipe_context, tar_base_name, src_base, dst_base);

child_out:
        free(tar_dir_name);
        free(tar_base_name);

        if (ret != 0) {
            exit(EXIT_FAILURE);
        } else {
            exit(EXIT_SUCCESS);
        }
    }

    close(pipe_stderr[1]);
    pipe_stderr[1] = -1;
    close(pipe_stream[1]);
    pipe_stream[1] = -1;

    ctx = util_common_calloc_s(sizeof(struct archive_context));
    if (ctx == NULL) {
        goto free_out;
    }

    ctx->stdin_fd = -1;
    ctx->stdout_fd = pipe_stream[0];
    pipe_stream[0] = -1;
    ctx->stderr_fd = pipe_stderr[0];
    pipe_stderr[0] = -1;
    ctx->pid = pid;
    ctx->safe_dir = safe_dir;
    safe_dir = NULL;

    reader->close = archive_context_close;
    reader->context = ctx;
    ctx = NULL;
    reader->read = archive_context_read;

    ret = 0;
free_out:
    close_archive_pipes_fd(pipe_stderr, 2);
    close_archive_pipes_fd(pipe_stream, 2);
    free(ctx);
    if (safe_dir != NULL) {
        if (umount(safe_dir) != 0) {
            ERROR("Failed to umount target %s", safe_dir);
        }
        if (util_path_remove(safe_dir) != 0) {
            ERROR("Failed to remove path %s", safe_dir);
        }
        free(safe_dir);
    }

    return ret;
}


static void free_archive_read(struct archive *read_a)
{
    if (read_a == NULL) {
        return;
    }
    archive_read_close(read_a);
    archive_read_free(read_a);
}

#define READ_BLOCK_SIZE 10240

static struct archive *create_archive_read(int fd)
{
    int nret = 0;
    struct archive *ret = NULL;

    ret = archive_read_new();
    if (ret == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    nret = archive_read_support_filter_all(ret);
    if (nret != 0) {
        ERROR("archive read support compression all failed");
        goto err_out;
    }
    nret = archive_read_support_format_all(ret);
    if (nret != 0) {
        ERROR("archive read support format all failed");
        goto err_out;
    }
    nret = archive_read_open_fd(ret, fd, READ_BLOCK_SIZE);
    if (nret != 0) {
        ERROR("archive read open file failed: %s", archive_error_string(ret));
        goto err_out;
    }

    return ret;
err_out:
    free_archive_read(ret);
    return NULL;
}

typedef int (*archive_entry_cb_t)(struct archive_entry *entry, struct archive *ar, int32_t position, Buffer *json_buf,
                                  int64_t *size);

static void free_storage_entry_data(storage_entry *entry)
{
    if (entry->name != NULL) {
        free(entry->name);
        entry->name = NULL;
    }
    if (entry->payload != NULL) {
        free(entry->payload);
        entry->payload = NULL;
    }
}

static int caculate_playload(struct archive *ar, char **result)
{
    int r = 0;
    unsigned char *block_buf = NULL;
    size_t block_size = 0;
#if ARCHIVE_VERSION_NUMBER >= 3000000
    int64_t block_offset = 0;
#else
    off_t block_offset = 0;
#endif
    int ret = 0;
    const isula_crc_table_t *ctab = NULL;
    uint64_t crc = 0;
    // max crc bits is 8
    unsigned char sum_data[8] = { 0 };
    // add \0 at crc bits last, so need a 9 bits array
    unsigned char tmp_data[9] = { 0 };
    bool empty = true;

    ctab = new_isula_crc_table(ISO_POLY);

    if (ctab == NULL) {
        return -1;
    }

    for (;;) {
        r = archive_read_data_block(ar, (const void **)&block_buf, &block_size, &block_offset);
        if (r == ARCHIVE_EOF) {
            break;
        }
        if (r != ARCHIVE_OK) {
            ERROR("Read archive failed");
            ret = -1;
            goto out;
        }
        if (!isula_crc_update(ctab, &crc, block_buf, block_size)) {
            ERROR("Do crc update failed");
            ret = -1;
            goto out;
        }
        empty = false;
    }
    if (empty) {
        goto out;
    }

    isula_crc_sum(crc, sum_data);
    // max crc bits is 8
    for (r = 0; r < 8; r++) {
        tmp_data[r] = sum_data[r];
    }
    ret = util_base64_encode(tmp_data, 8, result);
    if (ret != 0) {
        ERROR("Do encode failed");
    }

out:
    return ret;
}

static int archive_entry_parse(struct archive_entry *entry, struct archive *ar, int32_t position, Buffer *json_buf,
                               int64_t *size)
{
    storage_entry sentry = { 0 };
    struct parser_context ctx = { OPT_GEN_SIMPLIFY, stderr };
    parser_error jerr = NULL;
    char *data = NULL;
    int ret = -1;
    ssize_t nret = 0;

    // get entry information: name, size
    sentry.type = 1;
    sentry.name = util_strdup_s(archive_entry_pathname(entry));
    sentry.size = archive_entry_size(entry);
    sentry.position = position;
    // caculate playload
    if (caculate_playload(ar, &sentry.payload) != 0) {
        ERROR("Caculate playload failed");
        goto out;
    }

    data = storage_entry_generate_json(&sentry, &ctx, &jerr);
    if (data == NULL) {
        ERROR("parse entry failed: %s", jerr);
        goto out;
    }
    nret = buffer_append(json_buf, data, strlen(data));
    if (nret != 0) {
        goto out;
    }
    nret = buffer_append(json_buf, "\n", 1);
    if (nret != 0) {
        goto out;
    }

    *size = *size + sentry.size;

    ret = 0;
out:
    free_storage_entry_data(&sentry);
    free(data);
    free(jerr);
    return ret;
}

static int foreach_archive_entry(archive_entry_cb_t cb, int fd, const char *dist, int64_t *size)
{
    const size_t entry_init_buf_size = 4096;
    int ret = -1;
    int nret = 0;
    struct archive *read_a = NULL;
    struct archive_entry *entry = NULL;
    int32_t position = 0;
    Buffer *json_buf = NULL;

    // we need reset fd point to first position
    if (lseek(fd, 0, SEEK_SET) == -1) {
        ERROR("can not reposition of archive file");
        return -1;
    }
    json_buf = buffer_alloc(entry_init_buf_size);
    if (json_buf == NULL) {
        ERROR("Failed to malloc output_buffer");
        return -1;
    }

    read_a = create_archive_read(fd);
    if (read_a == NULL) {
        goto out;
    }

    for (;;) {
        nret = archive_read_next_header(read_a, &entry);
        if (nret == ARCHIVE_EOF) {
            DEBUG("read entry: %d", position);
            break;
        }
        if (nret != ARCHIVE_OK) {
            ERROR("archive read header failed: %s", archive_error_string(read_a));
            goto out;
        }
        nret = cb(entry, read_a, position, json_buf, size);
        if (nret != 0) {
            goto out;
        }
        position++;
    }
    nret = util_atomic_write_file(dist, json_buf->contents, json_buf->bytes_used, SECURE_CONFIG_FILE_MODE, true);
    if (nret != 0) {
        ERROR("save tar split failed");
        goto out;
    }

    ret = 0;
out:
    buffer_free(json_buf);
    free_archive_read(read_a);
    return ret;
}

// copy oci layer tar split archive file to dist path and return ahchive uncompress size
int archive_copy_oci_tar_split_and_ret_size(int src_fd, const char *dist_file, int64_t *ret_size)
{
    if (src_fd < 0 || dist_file == NULL || ret_size == NULL) {
        ERROR("Invalid arguments");
        return -1;
    }

    return foreach_archive_entry(archive_entry_parse, src_fd, dist_file, ret_size);
}