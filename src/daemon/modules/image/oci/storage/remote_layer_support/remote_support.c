/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wangrunze
 * Create: 2023-03-03
 * Description: provide image store functions
 ******************************************************************************/

#include "remote_support.h"

#include <pthread.h>

#include "isula_libutils/log.h"
#include "utils.h"

struct supporters {
    struct remote_image_data *image_data;
    struct remote_layer_data *layer_data;
    struct remote_overlay_data *overlay_data;
    pthread_rwlock_t *remote_lock;
};

static struct supporters supporters;

static inline bool remote_refresh_lock(pthread_rwlock_t *remote_lock, bool writable)
{
    int nret = 0;

    if (writable) {
        nret = pthread_rwlock_wrlock(remote_lock);
    } else {
        nret = pthread_rwlock_rdlock(remote_lock);
    }
    if (nret != 0) {
        ERROR("Lock memory store failed: %s", strerror(nret));
        return false;
    }

    return true;
}

static inline void remote_refresh_unlock(pthread_rwlock_t *remote_lock)
{
    int nret = 0;

    nret = pthread_rwlock_unlock(remote_lock);
    if (nret != 0) {
        FATAL("Unlock memory store failed: %s", strerror(nret));
    }
}

static void *remote_refresh_ro_symbol_link(void *arg)
{
    struct supporters *refresh_supporters = (struct supporters *)arg;
    prctl(PR_SET_NAME, "RoLayerRefresh");

    while (true) {
        util_usleep_nointerupt(5 * 1000 * 1000);
        DEBUG("remote refresh start\n");

        remote_refresh_lock(supporters.remote_lock, true);
        remote_overlay_refresh(refresh_supporters->overlay_data);
        remote_layer_refresh(refresh_supporters->layer_data);
        remote_image_refresh(refresh_supporters->image_data);
        remote_refresh_unlock(supporters.remote_lock);

        DEBUG("remote refresh end\n");
    }
    return NULL;
}

int remote_start_refresh_thread(pthread_rwlock_t *remote_lock)
{
    int res = 0;
    pthread_t a_thread;
    maintain_context ctx = get_maintain_context();

    supporters.image_data = remote_image_create(ctx.image_home, NULL);
    if (supporters.image_data == NULL) {
        goto free_out;
    }

    supporters.layer_data = remote_layer_create(ctx.layer_home, ctx.layer_ro_dir);
    if (supporters.layer_data == NULL) {
        goto free_out;
    }

    supporters.overlay_data = remote_overlay_create(ctx.overlay_home, ctx.overlay_ro_dir);
    if (supporters.overlay_data == NULL) {
        goto free_out;
    }

    supporters.remote_lock = remote_lock;

    res = pthread_create(&a_thread, NULL, remote_refresh_ro_symbol_link, (void *)&supporters);
    if (res != 0) {
        CRIT("Thread creation failed");
        return -1;
    }

    if (pthread_detach(a_thread) != 0) {
        SYSERROR("Failed to detach 0x%lx", a_thread);
        return -1;
    }

    return 0;

free_out:
    remote_image_destroy(supporters.image_data);
    remote_layer_destroy(supporters.layer_data);
    remote_overlay_destroy(supporters.overlay_data);

    return -1;
}

// this function calculate map_a - map_b => diff_list
// diff_list contains keys inside map_a but not inside map_b
static char **map_diff(const map_t *map_a, const map_t *map_b)
{
    char **array = NULL;
    map_itor *itor = map_itor_new(map_a);
    bool *found = NULL;

    // iter new_map, every item not in old, append them to new_layers
    for (; map_itor_valid(itor); map_itor_next(itor)) {
        char *id = map_itor_key(itor);
        found = map_search(map_b, id);
        if (found == NULL) {
            util_array_append(&array, util_strdup_s(id));
        }
    }

    map_itor_free(itor);

    return array;
}

char **remote_deleted_layers(const map_t *old, const map_t *new)
{
    return map_diff(old, new);
}

char **remote_added_layers(const map_t *old, const map_t *new)
{
    return map_diff(new, old);
}