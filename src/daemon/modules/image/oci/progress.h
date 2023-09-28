/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: Chenwei
 * Create: 2023-08-25
 * Description: provide pthread safe progress status map
 ******************************************************************************/
#ifndef DAEMON_MODULES_IMAGE_OCI_PROGRESS_STATUS_MAP_H
#define DAEMON_MODULES_IMAGE_OCI_PROGRESS_STATUS_MAP_H

#include "map.h"
#include <pthread.h>
#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct progress_status_map {
    struct _map_t *map;
    pthread_mutex_t mutex;
} progress_status_map;

typedef struct progress_status_map_itor {
    map_itor *itor;
    pthread_mutex_t *mutex;
} progress_status_map_itor;

typedef struct progress {
   int64_t dlnow;
   int64_t dltotal; 
} progress;

/* function to remove element by key */
bool progress_status_map_remove(progress_status_map *progress_status_map, void *key);

/* function to replace key value */
bool progress_status_map_replace(const progress_status_map *progress_status_map, void *key, void *value);

/* function to insert key value */
bool progress_status_map_insert(progress_status_map *progress_status_map, void *key, void *value);

/* function to return map itor */
progress_status_map_itor *progress_status_map_itor_new(const progress_status_map *progress_status_map);

/* function to free map itor */
void progress_status_map_itor_free(progress_status_map_itor *itor_s);

/* function to locate next itor */
bool progress_status_map_itor_next(progress_status_map_itor *itor_s);

/* function to locate prev itor */
bool progress_status_map_itor_prev(progress_status_map_itor *itor_s);

/* function to check itor is valid */
bool progress_status_map_itor_valid(const progress_status_map_itor *itor_s);

/* function to check itor is valid */
void *progress_status_map_itor_key(progress_status_map_itor *itor_s);

/* function to check itor is valid */
void *progress_status_map_itor_value(progress_status_map_itor *itor_s);

progress_status_map *progress_status_map_new(map_type_t kvtype, map_cmp_func comparator, map_kvfree_func kvfree);

size_t progress_status_map_size(const progress_status_map *progress_status_map);

void progress_status_map_free(progress_status_map *map);

void progress_status_map_clear(progress_status_map *map);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif // DAEMON_MODULES_IMAGE_OCI_PROGRESS_STATUS_MAP_H

