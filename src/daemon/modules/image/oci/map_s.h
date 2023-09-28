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
 * Description: provide pthread safe container map definition
 ******************************************************************************/
#ifndef DAEMON_MODULES_IMAGE_OCI_MAP_S_H
#define DAEMON_MODULES_IMAGE_OCI_MAP_S_H

#include "map.h"
#include <pthread.h>
#include <stdint.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct map_s {
    struct _map_t *map;
    pthread_mutex_t mutex;
} map_s;

typedef struct map_s_itor {
    map_itor *itor;
    pthread_mutex_t *mutex;
} map_s_itor;

/* function to remove element by key */
bool map_s_remove(map_s *map_s, void *key);

/* function to search key */
void *map_s_search(const map_s *map_s, void *key);

/* function to get size of map */
size_t map_s_size(const map_s *map_s);

/* function to replace key value */
bool map_s_replace(const map_s *map_s, void *key, void *value);

/* function to insert key value */
bool map_s_insert(map_s *map_s, void *key, void *value);

/* function to return map itor */
map_s_itor *map_s_itor_new(const map_s *map_s);

/* function to free map itor */
void map_s_itor_free(map_s_itor *itor_s);

/* function to locate first map itor */
bool map_s_itor_first(map_s_itor *itor_s);

/* function to locate last map itor */
bool map_s_itor_last(map_s_itor *itor_s);

/* function to locate next itor */
bool map_s_itor_next(map_s_itor *itor_s);

/* function to locate prev itor */
bool map_s_itor_prev(map_s_itor *itor_s);

/* function to check itor is valid */
bool map_s_itor_valid(const map_s_itor *itor_s);

/* function to check itor is valid */
void *map_s_itor_key(map_s_itor *itor_s);

/* function to check itor is valid */
void *map_s_itor_value(map_s_itor *itor_s);

map_s *map_s_new(map_type_t kvtype, map_cmp_func comparator, map_kvfree_func kvfree);

void map_s_free(map_s *map);

void map_s_clear(map_s *map);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif // DAEMON_MODULES_IMAGE_OCI_MAP_S_H

