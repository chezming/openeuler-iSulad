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
#include "progress.h"
#include <isula_libutils/log.h>
#include <stdlib.h>

#include "utils.h"

/* function to return map itor */
progress_status_map_itor *progress_status_map_itor_new(const progress_status_map *progress_status_map)
{
    if (progress_status_map == NULL) {
        ERROR("invalid parameter");
        return NULL;
    }

    progress_status_map_itor *itor_s = util_common_calloc_s(sizeof(struct progress_status_map_itor));
    if (itor_s == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    itor_s->mutex = (pthread_mutex_t *) & (progress_status_map->mutex);
    itor_s->itor = map_itor_new(progress_status_map->map);
    return itor_s;
}

/* function to free map itor */
void progress_status_map_itor_free(progress_status_map_itor *itor_s)
{
    if (itor_s == NULL) {
        return;
    }

    map_itor_free(itor_s->itor);
    free(itor_s);
}

/* function to locate first map itor */
bool progress_status_map_itor_first(progress_status_map_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_first(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate last map itor */
bool progress_status_map_itor_last(progress_status_map_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_last(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate next itor */
bool progress_status_map_itor_next(progress_status_map_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_next(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to locate prev itor */
bool progress_status_map_itor_prev(progress_status_map_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_prev(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to check itor is valid */
bool progress_status_map_itor_valid(const progress_status_map_itor *itor_s)
{
    if (itor_s == NULL || itor_s->mutex == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock(itor_s->mutex);
    ret = map_itor_valid(itor_s->itor);
    pthread_mutex_unlock(itor_s->mutex);

    return ret;
}

/* function to check itor is valid */
void *progress_status_map_itor_key(progress_status_map_itor *itor_s)
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
void *progress_status_map_itor_value(progress_status_map_itor *itor_s)
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
size_t progress_status_map_size(const progress_status_map *progress_status_map)
{
    if (progress_status_map == NULL) {
        ERROR("invalid parameter");
        return 0;
    }

    size_t ret = 0;
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    ret = map_size(progress_status_map->map);
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));

    return ret;
}

/* function to replace key value */
bool progress_status_map_replace(const progress_status_map *progress_status_map, void *key, void *value)
{
    if (progress_status_map == NULL || key == NULL || value == NULL) {
        ERROR("invalid parameter");
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    ret = map_replace(progress_status_map->map, key, value);
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));

    return ret;
}

/* function to insert key value */
bool progress_status_map_insert(progress_status_map *progress_status_map, void *key, void *value)
{
    if (progress_status_map == NULL || key == NULL || value == NULL) {
        ERROR("invalid parameter");
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    ret = map_insert(progress_status_map->map, key, value);
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));

    return ret;
}

/* function to remove element by key */
bool progress_status_map_remove(progress_status_map *progress_status_map, void *key)
{
    if (progress_status_map == NULL || key == NULL) {
        return false;
    }

    bool ret = false;
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    ret = map_remove(progress_status_map->map, key);
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));

    return ret;
}

/* function to search key */
void *progress_status_map_search(const progress_status_map *progress_status_map, void *key)
{
    if (progress_status_map == NULL || key == NULL) {
        return NULL;
    }

    void *ret = NULL;
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    ret = map_search(progress_status_map->map, key);
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));

    return ret;
}

// malloc a new map by type
progress_status_map *progress_status_map_new(map_type_t kvtype, map_cmp_func comparator, map_kvfree_func kvfree)
{
    progress_status_map *progress_status_map = NULL;
    progress_status_map = util_common_calloc_s(sizeof(struct progress_status_map));
    if (progress_status_map == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    progress_status_map->map = map_new(kvtype, comparator, kvfree);
    if (pthread_mutex_init((pthread_mutex_t *) & (progress_status_map->mutex), NULL) != 0) {
        ERROR("New map failed for mutex init");
        return NULL;
    }
    return progress_status_map;
}

/* just clear all nodes */
void progress_status_map_clear(progress_status_map *progress_status_map)
{
    pthread_mutex_lock((pthread_mutex_t *) & (progress_status_map->mutex));
    if (progress_status_map != NULL && progress_status_map->map != NULL) {
        map_clear(progress_status_map->map);
    }
    pthread_mutex_unlock((pthread_mutex_t *) & (progress_status_map->mutex));
}

/* map free */
void progress_status_map_free(progress_status_map *progress_status_map)
{
    if (progress_status_map == NULL) {
        return;
    }

    map_free(progress_status_map->map);
    free(progress_status_map);
}
