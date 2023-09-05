/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
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
#include "map_s.h"
#include <stdlib.h>

#include "isula_libutils/log.h"
#include "utils.h"

/* function to return map itor */
map_s_itor *map_s_itor_new(const map_s *map_s)
{
    if (map_s == NULL) {
        ERROR("invalid parameter");
        return NULL;
    }

    map_s_itor *itor_s = util_common_calloc_s(sizeof(struct map_s_itor));
    if (itor_s == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    itor_s->mutex = (pthread_mutex_t *) & (map_s->mutex);
    itor_s->itor = map_itor_new(map_s->map);
    return itor_s;
}

/* function to free map itor */
void map_s_itor_free(map_s_itor *itor_s)
{
    if (itor_s == NULL) {
        return;
    }

    map_itor_free(itor_s->itor);
    free(itor_s);
}

/* function to locate first map itor */
bool map_s_itor_first(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    bool ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_first(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate last map itor */
bool map_s_itor_last(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    bool ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_last(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate next itor */
bool map_s_itor_next(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    bool ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_next(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate prev itor */
bool map_s_itor_prev(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    bool ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_prev(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to check itor is valid */
bool map_s_itor_valid(const map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    bool ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_valid(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to check itor is valid */
void *map_s_itor_key(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    void *ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_key(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to check itor is valid */
void *map_s_itor_value(map_s_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return NULL;
    }

    void *ret = NULL;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_value(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to get size of map */
size_t map_s_size(const map_s *map_s)
{
    if (map_s == NULL) {
        ERROR("invalid parameter");
        return false;
    }

    size_t ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    ret = map_size(map_s->map);
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));

    return ret;
}

/* function to replace key value */
bool map_s_replace(const map_s *map_s, void *key, void *value)
{
    if (map_s == NULL) {
        ERROR("invalid parameter");
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    ret = map_replace(map_s->map, key, value);
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));

    return ret;
}

/* function to insert key value */
bool map_s_insert(map_s *map_s, void *key, void *value)
{
    if (map_s == NULL) {
        ERROR("invalid parameter");
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    ret = map_insert(map_s->map, key, value);
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));

    return ret;
}

/* function to remove element by key */
bool map_s_remove(map_s *map_s, void *key)
{
    if (map_s == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    ret = map_remove(map_s->map, key);
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));

    return ret;
}

/* function to search key */
void *map_s_search(const map_s *map_s, void *key)
{
    if (map_s == NULL) {
        return NULL;
    }

    void *ret = NULL;
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    ret = map_search(map_s->map, key);
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));

    return ret;
}

// malloc a new map by type
map_s *map_s_new(map_type_t kvtype, map_cmp_func comparator, map_kvfree_func kvfree)
{
    map_s *map_s = NULL;
    map_s = util_common_calloc_s(sizeof(struct map_s));
    if (map_s == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    map_s->map = map_new(kvtype, comparator, kvfree);
    pthread_mutex_init((pthread_mutex_t *) & (map_s->mutex), NULL);
    return map_s;
}

/* just clear all nodes */
void map_s_clear(map_s *map_s)
{
    pthread_mutex_lock((pthread_mutex_t *) & (map_s->mutex));
    if (map_s != NULL && map_s->map != NULL) {
        map_clear(map_s->map);
    }
    pthread_mutex_unlock((pthread_mutex_t *) & (map_s->mutex));
}

/* map free */
void map_s_free(map_s *map_s)
{
    if (map_s == NULL) {
        return;
    }

    map_free(map_s->map);
    free(map_s);
}
