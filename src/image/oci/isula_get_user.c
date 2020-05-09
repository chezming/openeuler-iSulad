/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
* Author: liuhao
* Create: 2019-07-15
* Description: isula container fs usage operator implement
*******************************************************************************/
#include "isula_get_user.h"

#include "image.h"
#include "isula_image_connect.h"
#include "isula_helper.h"
#include "connect.h"
#include "utils.h"
#include "libisulad.h"
#include "log.h"

static int generate_isula_get_user_request(const char *container_id,
        const char *userstr,
        char **groupAdds,
        int groupAddsLen,
        struct isula_get_user_request **ireq)
{
    struct isula_get_user_request *tmp_req = NULL;

    tmp_req = (struct isula_get_user_request *)util_common_calloc_s(
                  sizeof(struct isula_get_user_request));
    if (tmp_req == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    tmp_req->container_id = util_strdup_s(container_id);
    tmp_req->user_str = util_strdup_s(userstr);
    if (groupAddsLen > 0) {
        tmp_req->group_adds = (char **)util_smart_calloc_s(sizeof(char *), groupAddsLen);
        for (int i = 0; i < groupAddsLen; i++) {
            tmp_req->group_adds[i] = util_strdup_s(groupAdds[i]);
        }
    }
    tmp_req->group_adds_len = groupAddsLen;
    *ireq = tmp_req;
    return 0;
}

static int is_valid_arguments(const char *id)
{
    if (id == NULL) {
        ERROR("Invalid container id or name");
        return -1;
    }

    return 0;
}


int isula_get_user(const char *id, const char *basefs, host_config *hc, const char *userstr, defs_process_user *puser) {
    int ret = -1;
    struct isula_get_user_request *ireq = NULL;
    struct isula_get_user_response *iresp = NULL;
    client_connect_config_t conf = { 0 };
    isula_image_ops *im_ops = NULL;

    if (userstr == NULL) {
        return 0;
    }

    if (is_valid_arguments(id) != 0) {
        return -1;
    }

    im_ops = get_isula_image_ops();
    if (im_ops == NULL) {
        ERROR("Don't init isula server grpc client");
        return -1;
    }

    if (im_ops->get_user == NULL) {
        ERROR("Umimplement container fs usage operator");
        return -1;
    }

    ret = generate_isula_get_user_request(id, userstr, hc->group_add, hc->group_add_len, &ireq);
    if (ret != 0) {
        goto out;
    }

    iresp = (struct isula_get_user_response *)util_common_calloc_s(sizeof(struct isula_get_user_response));
    if (iresp == NULL) {
        ERROR("Out of memory");
        goto out;
    }

    ret = get_isula_image_connect_config(&conf);
    if (ret != 0) {
        goto out;
    }

    ret = im_ops->get_user(ireq, iresp, &conf);
    if (ret != 0) {
        ERROR("Get container %s user failed: %s", id, iresp->errmsg);
        isulad_set_error_message("Failed to get container user with error: %s", iresp->errmsg);
        goto out;
    }
    puser->uid = (uid_t)iresp->uid;
    puser->gid = (gid_t)iresp->gid;
    size_t additional_gids_len = iresp->additional_gids_len;
    if (additional_gids_len > 0) {
        puser->additional_gids = (gid_t *) util_smart_calloc_s(sizeof(gid_t), additional_gids_len);
        for (size_t i = 0; i < additional_gids_len; i++) {
            puser->additional_gids[i] = (gid_t)iresp->additional_gids[i];
        }
    }

out:
    free_isula_get_user_request(ireq);
    free_isula_get_user_response(iresp);
    free_client_connect_config_value(&conf);
    return ret;
}
