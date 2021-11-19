/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2021. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: XiyouNiGo
 * Create: 2021-09-04
 * Description: provide changes functions
 *******************************************************************************/
#include "changes.h"

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include "constants.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "path.h"
#include "map.h"
#include "utils_string.h"
#include "utils_file.h"

struct walker {
    char *dir1;
    char *dir2;
    struct file_info *root1;
    struct file_info *root2;
};

static void walker_free(struct walker *w, bool free_file_info)
{
    if (w == NULL) {
        return;
    }

    if (w->dir1) {
        free(w->dir1);
        w->dir1 = NULL;
    }

    if (w->dir2) {
        free(w->dir2);
        w->dir2 = NULL;
    }

    if (free_file_info) {
        if (w->root1) {
            file_info_free(w->root1);
            w->root1 = NULL;
        }

        if (w->root2) {
            file_info_free(w->root2);
            w->root2 = NULL;
        }
    }

    free(w);
}

void change_free(struct change *change)
{
    if (change == NULL) {
        return;
    }

    if (change->path) {
        free(change->path);
        change->path = NULL;
    }

    free(change);
}

struct change_result *change_result_new()
{
    struct change_result *result = NULL;

    result = (struct change_result *)util_common_calloc_s(sizeof(struct change_result));
    if (result == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    linked_list_init(&result->list);
    result->list_len = 0;

    return result;
}

void change_result_free(struct change_result *result)
{
    struct linked_list *it = NULL, *next = NULL;
    struct change *change = NULL;

    linked_list_for_each_safe(it, &(result->list), next) {
        change = (struct change*)it->elem;
        linked_list_del(it);
        change_free(change);
        free(it);
        it = NULL;
    }
    result->list_len = 0;

    free(result);
}

void file_info_free(struct file_info *info)
{
    if (info == NULL) {
        return;
    }

    if (info->name) {
        free(info->name);
        info->name = NULL;
    }

    if (info->stat) {
        free(info->stat);
        info->stat = NULL;
    }

    if (info->children) {
        map_free(info->children);
        info->children = NULL;
    }

    free(info);
}

void file_info_map_kvfree(void *key, void *value)
{
    file_info_free((struct file_info *)value);
}

static struct file_info *new_root_file_info()
{
    struct file_info *root = NULL;
    root = (struct file_info *)util_common_calloc_s(sizeof(struct file_info));
    if (root == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    root->name = util_strdup_s("/");
    root->children = map_new(MAP_STR_PTR, MAP_DEFAULT_CMP_FUNC, file_info_map_kvfree);
    if (root->children == NULL) {
        ERROR("Out of memory");
        file_info_free(root);
        return NULL;
    }
    return root;
}

struct file_info *file_info_lookup(struct file_info *info, const char *path)
{
    int i = 0;
    size_t path_elements_len = 0;
    char **path_elements = NULL;
    struct file_info *child = NULL, *parent = NULL;

    if (strcmp(path, "/") == 0) {
        return info;
    }

    parent = info;
    path_elements = util_string_split(path, '/');
    path_elements_len = util_array_len((const char **)path_elements);
    for (i = 0; i < path_elements_len; i++) {
        if (strcmp(path_elements[i], "") != 0) {
            child = (struct file_info*)map_search(parent->children, (void*)path_elements[i]);
            if (child == NULL) {
                util_free_array(path_elements);
                return NULL;
            }
            parent = child;
        }
    }

    util_free_array(path_elements);
    return parent;
}

static void file_info_get_path_helper(struct file_info *info, char *tmp_path)
{
    if (info->parent == NULL) {
        sprintf(tmp_path, "/");
        return;
    }
    file_info_get_path_helper(info->parent, tmp_path);
    if (strcmp(tmp_path, "/") != 0) {
        strncat(tmp_path, "/", PATH_MAX);
    }
    strncat(tmp_path, info->name, PATH_MAX);
}

char *file_info_get_path(struct file_info *info)
{
    char tmp_path[PATH_MAX] = { 0 };

    if (info == NULL) {
        return NULL;
    }

    file_info_get_path_helper(info, tmp_path);

    return util_strdup_s(tmp_path);
}

// {name,inode} pairs used to support the early-pruning logic of the walker type.
struct name_ino {
    char *name;
    uint64_t ino;
};

struct name_ino *name_ino_copy(struct name_ino *ni)
{
    struct name_ino *result = NULL;

    if (ni == NULL) {
        return NULL;
    }

    result = (struct name_ino *)util_common_calloc_s(sizeof(struct name_ino));
    if (result == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    result->name = util_strdup_s(ni->name);
    result->ino = ni->ino;

    return result;
}

static void name_ino_free(struct name_ino *ni)
{
    if (ni == NULL) {
        return;
    }

    free(ni->name);
    ni->name = NULL;

    free(ni);
}

static int compare_name_ino(const void *a, const void *b)
{
    return strcmp(((struct name_ino *)a)->name, ((struct name_ino *)b)->name);
}

struct name_ino_result {
    struct linked_list list;
    int list_len;
};

struct name_ino_result *name_ino_result_new()
{
    struct name_ino_result *result = NULL;

    result = (struct name_ino_result *)util_common_calloc_s(sizeof(struct name_ino_result));
    if (result == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    linked_list_init(&result->list);
    result->list_len = 0;

    return result;
}

static void name_ino_result_free(struct name_ino_result *result)
{
    struct linked_list *it = NULL, *next;
    struct name_ino *ni = NULL;

    if (result ==  NULL) {
        return;
    }

    linked_list_for_each_safe(it, &(result->list), next) {
        ni = (struct name_ino *)it->elem;
        linked_list_del(it);
        name_ino_free(ni);
        free(it);
        it = NULL;
    }
    result->list_len = 0;

    free(result);
}

static bool same_fs_time_spec(struct timespec a, struct timespec b)
{
    return a.tv_sec == b.tv_sec && (a.tv_nsec == b.tv_nsec || a.tv_nsec == 0 || b.tv_nsec == 0);
}

static bool stat_different(struct stat *old_stat, struct stat *new_stat)
{
    return old_stat->st_mode != new_stat->st_mode || old_stat->st_uid != new_stat->st_uid ||
           old_stat->st_gid != new_stat->st_gid || old_stat->st_rdev != new_stat->st_rdev ||
           (S_ISDIR(old_stat->st_mode) && (!same_fs_time_spec(old_stat->st_mtim, new_stat->st_mtim) ||
                                           (old_stat->st_size != new_stat->st_size)));
}

// readdirnames returns list of {filename,inode} pairs when reading directory contents.
static int readdirnames(const char *dirname, struct name_ino_result **result)
{
    int ret = 0;
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    struct name_ino_result *tmp_res = NULL;
    struct linked_list *new_node = NULL;
    struct name_ino *tmp_ni = NULL;

    tmp_res = name_ino_result_new();
    if (tmp_res == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    dir = opendir(dirname);
    if (dir == NULL) {
        ERROR("Failed to opendir %s", dirname);
        ret = -1;
        goto out;
    }

    entry = readdir(dir);
    while (entry != NULL) {
        if (strncmp(entry->d_name, ".", PATH_MAX) == 0 || strncmp(entry->d_name, "..", PATH_MAX) == 0) {
            entry = readdir(dir);
            continue;
        }
        tmp_ni = (struct name_ino*)util_common_calloc_s(sizeof(struct name_ino));
        if (tmp_ni == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        new_node = (struct linked_list*)util_common_calloc_s(sizeof(struct linked_list));
        if (new_node == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        tmp_ni->name = util_strdup_s(entry->d_name);
        tmp_ni->ino = entry->d_ino;
        linked_list_add_elem(new_node, tmp_ni);
        linked_list_add_tail(&tmp_res->list, new_node);
        tmp_res->list_len++;
        entry = readdir(dir);
    }

out:
    if (dir != NULL) {
        closedir(dir);
    }
    if (ret != 0) {
        name_ino_result_free(tmp_res);
        free(new_node);
        new_node = NULL;
        free(tmp_ni);
        tmp_ni = NULL;
    } else {
        linked_list_qsort(tmp_res->list.next, tmp_res->list.prev, &compare_name_ino);
        *result = tmp_res;
    }
    return ret;
}

// Given a struct stat, its path info, and a reference to the root of the tree
// being constructed, register this file with the tree.
static int walkchunk(const char *path, struct stat *fstat, const char *dir, struct file_info *root)
{
    int ret = 0, nret = 0;
    struct file_info *parent = NULL, *info = NULL;
    char *cpath = NULL, *path_dir;

    if (fstat == NULL) {
        return 0;
    }

    nret = util_split_dir_and_base_name(path, &path_dir, NULL);
    if (nret != 0) {
        ERROR("split %s failed", path);
        return -1;
    }

    parent = file_info_lookup(root, path_dir);
    if (parent == NULL) {
        ret = -1;
        ERROR("walkchunk: Unexpectedly no parent for %s", path);
        goto out;
    }

    info = (struct file_info*)util_common_calloc_s(sizeof(struct file_info));
    if (info == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    info->name = util_path_base(path);
    info->children = map_new(MAP_STR_PTR, MAP_DEFAULT_CMP_FUNC, file_info_map_kvfree);
    if (info->children == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    info->parent = parent;
    info->stat = fstat;

    cpath = util_path_join(dir, path);
    if (cpath == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    nret = lgetxattr(cpath, "security.capability", info->capability, XATTR_SIZE_MAX);
    if (nret < 0 && errno != EOPNOTSUPP && errno != ENODATA && errno != ENOENT) {
        ret = -1;
        ERROR("Failed to get xattr of %s", cpath);
        goto out;
    }

    map_insert(parent->children, (void*)info->name, (void*)info);

out:
    free(path_dir);
    free(cpath);
    if (ret != 0) {
        file_info_free(info);
    }
    return 0;
}

// Walk a subtree rooted at the same path in both trees being iterated.
static int walk(struct walker *w, const char *path, struct stat *stat1, struct stat *stat2)
{
    int ret = 0, nret = 0, ix1 = 0, ix2 = 0;
    bool is1dir = false, is2dir = false;
    bool same_device = false;
    char *tmp_path = NULL, *name = NULL, *fname = NULL;
    struct stat *cstat1 = NULL, *cstat2 = NULL;
    struct name_ino *ni1 = NULL, *ni2 = NULL, *tmp_ni = NULL;
    struct name_ino_result *names = NULL, *names1 = NULL, *names2 = NULL;
    struct linked_list *it = NULL, *new_node = NULL;

    if (path == NULL || path[0] == '\0') {
        ERROR("Invalid path");
        return -1;
    }

    if (strcmp(path, "/") != 0) {
        nret = walkchunk(path, stat1, w->dir1, w->root1);
        if (nret != 0) {
            ERROR("Failed to walkchunk");
            return -1;
        }
        nret = walkchunk(path, stat2, w->dir2, w->root2);
        if (nret != 0) {
            ERROR("Failed to walkchunk");
            return -1;
        }
    }

    is1dir = (stat1 != NULL) && S_ISDIR(stat1->st_mode);
    is2dir = (stat2 != NULL) && S_ISDIR(stat2->st_mode);

    if (!is1dir && !is2dir) {
        return 0;
    }

    if (stat1 != NULL && stat2 != NULL) {
        same_device = (stat1->st_dev == stat2->st_dev);
    }

    if (is1dir) {
        tmp_path = util_path_join(w->dir1, path);
        if (tmp_path == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        nret = readdirnames(tmp_path, &names1);
        if (nret != 0) {
            ret = -1;
            ERROR("Failed to read directory names %s", tmp_path);
            goto out;
        }
        free(tmp_path);
        tmp_path = NULL;
    }
    if (is2dir) {
        tmp_path = util_path_join(w->dir2, path);
        if (tmp_path == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        nret = readdirnames(tmp_path, &names2);
        if (nret != 0) {
            ret = -1;
            ERROR("Failed to read directory names %s", tmp_path);
            goto out;
        }
        free(tmp_path);
        tmp_path = NULL;
    }

    names = name_ino_result_new();
    if (names == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    while (names1 && ix1 < names1->list_len && names2 && ix2 < names2->list_len) {
        ni1 = (struct name_ino*)linked_list_at(&names1->list, ix1);
        if (ni1 == NULL) {
            ret = -1;
            ERROR("Cross-border access for list");
            goto out;
        }
        ni2 = (struct name_ino*)linked_list_at(&names2->list, ix2);
        if (ni2 == NULL) {
            ret = -1;
            ERROR("Cross-border access for list");
            goto out;
        }
        nret = compare_name_ino((void*)ni1, (void*)ni2);
        if (nret < 0) {
            tmp_ni = name_ino_copy((struct name_ino*)linked_list_at(&names1->list, ix1));
            if (tmp_ni == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            new_node = (struct linked_list *)util_common_calloc_s(sizeof(struct linked_list));
            if (new_node == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            linked_list_add_elem(new_node, tmp_ni);
            linked_list_add_tail(&names->list, new_node);
            names->list_len++;
            ix1++;
        } else if (nret == 0) {
            if (ni1->ino != ni2->ino || !same_device) {
                tmp_ni = name_ino_copy((struct name_ino*)linked_list_at(&names1->list, ix1));
                if (tmp_ni == NULL) {
                    ret = -1;
                    ERROR("Out of memory");
                    goto out;
                }
                new_node = (struct linked_list *)util_common_calloc_s(sizeof(struct linked_list));
                if (new_node == NULL) {
                    ret = -1;
                    ERROR("Out of memory");
                    goto out;
                }
                linked_list_add_elem(new_node, tmp_ni);
                linked_list_add_tail(&names->list, new_node);
                names->list_len++;
            }
            ix1++;
            ix2++;
        } else if (nret > 0) {
            tmp_ni = name_ino_copy((struct name_ino*)linked_list_at(&names2->list, ix2));
            if (tmp_ni == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            new_node = (struct linked_list *)util_common_calloc_s(sizeof(struct linked_list));
            if (new_node == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            linked_list_add_elem(new_node, tmp_ni);
            linked_list_add_tail(&names->list, new_node);
            names->list_len++;
            ix2++;
        }
    }

    while (names1 && ix1 < names1->list_len) {
        tmp_ni = name_ino_copy((struct name_ino*)linked_list_at(&names1->list, ix1));
        if (tmp_ni == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        new_node = (struct linked_list *)util_common_calloc_s(sizeof(struct linked_list));
        if (new_node == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        linked_list_add_elem(new_node, tmp_ni);
        linked_list_add_tail(&names->list, new_node);
        names->list_len++;
        ix1++;
    }
    while (names2 && ix2 < names2->list_len) {
        tmp_ni = name_ino_copy((struct name_ino*)linked_list_at(&names2->list, ix2));
        if (tmp_ni == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        linked_list_add_elem(new_node, tmp_ni);
        linked_list_add_tail(&names->list, new_node);
        names->list_len++;
        ix2++;
    }

    linked_list_for_each(it, &(names->list)) {
        name = ((struct name_ino*)it->elem)->name;
        fname = util_path_join(path, name);
        if (fname == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        if (is1dir) {
            tmp_path = util_path_join(w->dir1, fname);
            if (tmp_path == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            cstat1 = (struct stat*)util_common_calloc_s(sizeof(struct stat));
            if (cstat1 == NULL) {
                ERROR("Out of memory");
                goto out;
            }
            nret = lstat(tmp_path, cstat1);
            if (nret != 0) {
                if (errno != ENOENT) {
                    ret = -1;
                    ERROR("Failed to get stat of %s", tmp_path);
                    goto out;
                }
                free(cstat1);
                cstat1 = NULL;
            }
            free(tmp_path);
            tmp_path = NULL;
        }
        if (is2dir) {
            tmp_path = util_path_join(w->dir2, fname);
            if (tmp_path == NULL) {
                ret = -1;
                ERROR("Out of memory");
                goto out;
            }
            cstat2 = (struct stat*)util_common_calloc_s(sizeof(struct stat));
            if (cstat2 == NULL) {
                ERROR("Out of memory");
                goto out;
            }
            nret = lstat(tmp_path, cstat2);
            if (nret != 0) {
                if (errno != ENOENT) {
                    ret = -1;
                    ERROR("Failed to get stat of %s", tmp_path);
                    goto out;
                }
                free(cstat2);
                cstat2 = NULL;
            }
            free(tmp_path);
            tmp_path = NULL;
        }
        nret = walk(w, fname, cstat1, cstat2);
        if (nret != 0) {
            ret = -1;
            ERROR("Failed to walk %s", tmp_path ? tmp_path : "");
            goto out;
        }
        free(fname);
        fname = NULL;
    }

out:
    free(fname);
    free(tmp_path);
    name_ino_result_free(names);
    name_ino_result_free(names1);
    name_ino_result_free(names2);
    return ret;
}

// collect_file_info_for_changes returns a complete representation of the trees
// rooted at dir1 and dir2, with one important exception: any subtree or
// leaf where the inode and device numbers are an exact match between dir1
// and dir2 will be pruned from the results. This method is *only* to be used
// to generating a list of changes between the two directories, as it does not
// reflect the full contents.
static int collect_file_info_for_changes(const char *dir1, const char *dir2, struct file_info **fi1,
                                         struct file_info **fi2)
{
    int ret = 0, nret = 0;
    struct stat stat1, stat2;
    struct walker *w = NULL;

    w = (struct walker*)util_common_calloc_s(sizeof(struct walker));
    if (w == NULL) {
        ERROR("Out of memory");
        goto out;
    }
    w->dir1 = util_strdup_s(dir1);
    w->dir2 = util_strdup_s(dir2);
    w->root1 = new_root_file_info();
    w->root2 = new_root_file_info();

    nret = lstat(w->dir1, &stat1);
    if (nret != 0) {
        ret = -1;
        ERROR("Failed to get stat of %s", w->dir1);
        goto out;
    }

    nret = lstat(w->dir2, &stat2);
    if (nret != 0) {
        ret = -1;
        ERROR("Failed to get stat of %s", w->dir2);
        goto out;
    }

    nret = walk(w, "/", &stat1, &stat2);
    if (nret != 0) {
        ret = -1;
        ERROR("Failed to walk on %s and %s", w->dir1, w->dir2);
        goto out;
    }

out:
    if (ret == 0) {
        *fi1 = w->root1;
        *fi2 = w->root2;
    }
    walker_free(w, ret);
    return ret;
}

static int add_changes(struct file_info *new_info, struct file_info *old_info, struct change_result *result)
{
    int ret = 0;
    int size_at_entry = 0;
    char *tmp_path = NULL;
    struct change *tmp_change = NULL;
    map_t *old_children = NULL;
    struct file_info *old_child = NULL;
    struct file_info *new_child = NULL;
    struct linked_list *new_node = NULL;
    map_itor *itor = NULL;

    if (new_info == NULL) {
        ERROR("Can not add change for empty directory");
        return -1;
    }

    if (old_info == NULL) {
        tmp_change = (struct change*)util_common_calloc_s(sizeof(struct change));
        if (tmp_change == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        new_node = (struct linked_list*)util_common_calloc_s(sizeof(struct linked_list));
        if (new_node == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        // add
        tmp_change->path = file_info_get_path(new_info);
        tmp_change->type = CHANGE_ADD;
        linked_list_add_elem(new_node, tmp_change);
        linked_list_add_tail(&result->list, new_node);
        result->list_len++;
        new_info->added = true;
    }

    // make a copy to detect additions.
    old_children = map_new(MAP_STR_PTR, MAP_DEFAULT_CMP_FUNC, map_free_nothing);
    if (old_children == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }
    if (old_info != NULL && (!strcmp(old_info->name, "/") || S_ISDIR(old_info->stat->st_mode))) {
        itor = map_itor_new(old_info->children);
        if (itor == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        for (; map_itor_valid(itor); map_itor_next(itor)) {
            if (!map_insert(old_children, map_itor_key(itor), map_itor_value(itor))) {
                ERROR("Failed to insert child to old children");
                ret = -1;
                goto out;
            }
        }
        map_itor_free(itor);
    }

    itor = map_itor_new(new_info->children);
    if (itor == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }

    for (; map_itor_valid(itor); map_itor_next(itor)) {
        new_child = (struct file_info*)map_itor_value(itor);
        old_child = (struct file_info*)map_search(old_children, (void*)new_child->name);
        if (old_child != NULL) {
            if (stat_different(old_child->stat, new_child->stat) ||
                memcmp(old_child->capability, new_child->capability, XATTR_SIZE_MAX)) {
                tmp_change = (struct change*)util_common_calloc_s(sizeof(struct change));
                if (tmp_change == NULL) {
                    ret = -1;
                    ERROR("Out of memory");
                    goto out;
                }
                new_node = (struct linked_list*)util_common_calloc_s(sizeof(struct linked_list));
                if (new_node == NULL) {
                    ret = -1;
                    ERROR("Out of memory");
                    goto out;
                }
                // modify
                tmp_change->path = file_info_get_path(new_child);
                tmp_change->type = CHANGE_MODIFY;
                linked_list_add_elem(new_node, tmp_change);
                linked_list_add_tail(&result->list, new_node);
                result->list_len++;
                new_child->added = true;
            }
            map_remove(old_children, new_child->name);
        }
        add_changes(new_child, old_child, result);
    }

    itor = map_itor_new(old_children);
    if (itor == NULL) {
        ret = -1;
        ERROR("Out of memory");
        goto out;
    }

    for (; map_itor_valid(itor); map_itor_next(itor)) {
        old_child = (struct file_info *)map_itor_value(itor);
        tmp_change = (struct change*)util_common_calloc_s(sizeof(struct change));
        if (tmp_change == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        new_node = (struct linked_list*)util_common_calloc_s(sizeof(struct linked_list));
        if (new_node == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        // delete
        tmp_change->path = file_info_get_path(old_child);
        tmp_change->type = CHANGE_DELETE;
        linked_list_add_elem(new_node, tmp_change);
        linked_list_add_tail(&result->list, new_node);
        result->list_len++;
        old_child->added = true;
    }

    // If there were changes inside this directory, we need to add it, even if the directory
    // itself wasn't changed. This is needed to properly save and restore filesystem permissions.
    tmp_path = file_info_get_path(new_info);
    if (result->list_len > size_at_entry && strcmp(tmp_path, "/") != 0 &&
        S_ISDIR(new_info->stat->st_mode) && !new_info->added) {
        tmp_change = (struct change*)util_common_calloc_s(sizeof(struct change));
        if (tmp_change == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        new_node = (struct linked_list*)util_common_calloc_s(sizeof(struct linked_list));
        if (new_node == NULL) {
            ret = -1;
            ERROR("Out of memory");
            goto out;
        }
        tmp_change->path = util_strdup_s(tmp_path);
        tmp_change->type = CHANGE_MODIFY;
        linked_list_add_elem(new_node, tmp_change);
        linked_list_add_tail(&result->list, new_node);
        result->list_len++;
    }

out:
    if (ret != 0) {
        free(new_node);
        free(tmp_change);
    }
    free(tmp_path);
    map_free(old_children);
    map_itor_free(itor);
    return ret;
}

// changes_dirs compares two directories and generates a list of change objects describing the changes.
// If old_dir is "", then all files in new_dir will be add-changes.
int changes_dirs(const char *new_dir, const char *old_dir, struct change_result **result)
{
    int ret = 0, nret = 0;
    char *empty_dir = NULL;
    struct file_info *old_root = NULL;
    struct file_info *new_root = NULL;
    struct change_result *tmp_res = NULL;

    if (old_dir == NULL || old_dir[0] == '\0') {
        empty_dir = util_get_tmp_file(NULL, "empty");
        if (empty_dir == NULL) {
            ERROR("Failed to get_tmp file");
            goto out;
        }
        nret = util_mkdir_p(empty_dir, TEMP_DIRECTORY_MODE);
        if (nret != 0) {
            ERROR("Failed to create directory %s", empty_dir);
            goto out;
        }
        old_dir = empty_dir;
    }

    nret = collect_file_info_for_changes(old_dir, new_dir, &old_root, &new_root);
    if (nret != 0) {
        ret = -1;
        ERROR("Failed to collect file info");
        goto out;
    }

    tmp_res = change_result_new();
    if (tmp_res == NULL) {
        ERROR("Out of memory");
        goto out;
    }
    add_changes(new_root, old_root, tmp_res);

out:
    if (empty_dir != NULL) {
        nret = util_recursive_remove_path(empty_dir);
        if (nret != 0) {
            ret = -1;
            ERROR("Failed to remove directory %s", empty_dir);
        }
        free(empty_dir);
        empty_dir = NULL;
    }
    if (ret == 0) {
        *result = tmp_res;
    } else {
        change_result_free(tmp_res);
    }
    return ret;
}

// changes_size calculates the size in bytes of the provided changes list, based on new_dir.
int changes_size(const char *new_dir, struct change_result *result, int64_t *size)
{
    int ret = 0, nret = 0;
    int64_t tmp_size = 0;
    char tmp_path[PATH_MAX] = { 0 };
    char real_path[PATH_MAX] = { 0 };
    struct change *change = NULL;
    struct stat fstat;
    map_t *map = NULL;
    struct linked_list *it = NULL;

    map = map_new(MAP_INT_BOOL, MAP_DEFAULT_CMP_FUNC, MAP_DEFAULT_FREE_FUNC);
    if (map == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    linked_list_for_each(it, &(result->list)) {
        change = (struct change *)it->elem;
        if (change->type == CHANGE_MODIFY || change->type == CHANGE_ADD) {
            nret = snprintf(tmp_path, PATH_MAX - 1, "%s/%s", new_dir, change->path);
            if (nret < 0 || nret >= PATH_MAX) {
                ERROR("Too long tmp path");
                continue;
            }
            if (util_clean_path(tmp_path, real_path, sizeof(real_path)) == NULL) {
                ERROR("failed to get clean path %s", tmp_path);
                continue;
            }
            nret = lstat(tmp_path, &fstat);
            if (nret != 0) {
                ERROR("Can not stat %s", tmp_path);
                continue;
            }
            if (S_ISDIR(fstat.st_mode) == 0) {
                if (fstat.st_nlink > 1) {
                    if (map_search(map, (void*)(&(fstat.st_ino))) == NULL) {
                        tmp_size += fstat.st_size;
                        bool val = true;
                        map_insert(map, (void*)(&(fstat.st_ino)), (void *)&val);
                    }
                } else {
                    tmp_size += fstat.st_size;
                }
            }
        }
    }

    map_free(map);
    *size = tmp_size;
    return ret;
}