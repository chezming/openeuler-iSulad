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
 * Description: provide changes definition
 ******************************************************************************/
#ifndef UTILS_CUTILS_CHANGES_H
#define UTILS_CUTILS_CHANGES_H

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "map.h"
#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CHANGE_MODIFY = 0,  // represents the modify operation.
    CHANGE_ADD = 1,     // represents the add operation.
    CHANGE_DELETE = 2,  // represents the delete operation.
} change_type;

// change represents a change, it wraps the change type and path.
// It describes changes of the files in the path respect to the
// parent layers. The change could be modify, add, delete.
// This is used for layer diff.
struct change {
    char *path;
    change_type type;
};

void change_free(struct change *change);

struct change_result {
    struct linked_list list;
    int list_len;
};

struct change_result *change_result_new();

void change_result_free(struct change_result *result);

// file_info describes the information of a file.
struct file_info {
    struct file_info *parent;
    char *name;
    struct stat *stat;
    map_t *children;
    char capability[XATTR_SIZE_MAX];
    bool added;
};

void file_info_free(struct file_info *info);

void file_info_map_kvfree(void *key, void *value);

struct file_info *file_info_lookup(struct file_info *info, const char *path);

char *file_info_get_path(struct file_info *info);

int changes_dirs(const char *new_dir, const char *old_dir, struct change_result **result);

int changes_size(const char *new_dir, struct change_result *result, int64_t *size);

#ifdef __cplusplus
}
#endif

#endif // UTILS_CUTILS_CHANGES_H