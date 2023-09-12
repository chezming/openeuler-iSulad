/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2033. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Description: provide image filter functions
 ******************************************************************************/
#include "image_filter.h"
#include <isula_libutils/log.h>
#include "map.h"

bool image_meet_dangling_filter(const imagetool_image_summary *src, const struct filters_args *filters)
{
    bool ret = false;
    map_t *field_values_map = NULL;
    const char *field = "dangling";
    map_itor *itor = NULL;
    bool dangling_value = false;

    field_values_map = map_search(filters->fields, (void *)field);
    if (field_values_map == NULL) {
        return true;
    }

    itor = map_itor_new(field_values_map);
    if (itor == NULL) {
        ERROR("Out of memory");
        return false;
    }

    for (; map_itor_valid(itor); map_itor_next(itor)) {
        if (strcmp(map_itor_key(itor), "true") == 0) {
            dangling_value = true;
            break;
        }
    }

    if (dangling_value) {
        ret = src->repo_tags_len == 0;
    } else {
        ret = src->repo_tags_len != 0;
    }

    map_itor_free(itor);
    return ret;
}
