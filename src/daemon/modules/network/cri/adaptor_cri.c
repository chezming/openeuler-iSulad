/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * clibcni licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: haozi007
 * Create: 2020-12-05
 * Description: provide cni network functions
 *********************************************************************************/
#include "network_api.h"

#include <isula_libutils/log.h>
#include "cni_operate.h"
#include "utils.h"
#include "map.h"
#include "utils_network.h"
#include "err_msg.h"

// do not need lock;
// because cri can make sure do not concurrent to call these apis
typedef struct network_store_t {
    struct cni_network_list_conf **conflist;
    size_t conflist_len;
    map_t *g_net_index_map;
} network_store;

#define DEFAULT_NETWORK_INTERFACE "eth0"

static network_store g_net_store = { 0 };

bool adaptor_cni_check_inited()
{
    return g_net_store.conflist_len > 0;
}

static bool is_cri_config_file(const char *filename)
{
    if (filename == NULL) {
        return false;
    }

    return strncmp(ISULAD_CNI_NETWORK_CONF_FILE_PRE, filename, strlen(ISULAD_CNI_NETWORK_CONF_FILE_PRE)) != 0;
}

static void do_update_cni_stores(map_t *work, struct cni_network_list_conf **new_list, size_t new_list_len)
{
    size_t i;

    map_free(g_net_store.g_net_index_map);
    g_net_store.g_net_index_map = work;

    for (i = 0; i < g_net_store.conflist_len; i++) {
        free_cni_network_list_conf(g_net_store.conflist[i]);
    }
    free(g_net_store.conflist);
    g_net_store.conflist = new_list;
    g_net_store.conflist_len = new_list_len;
}

int adaptor_cni_update_confs()
{
    int ret = 0;
    map_t *work = NULL;
    struct cni_network_list_conf **tmp_net_list = NULL;
    size_t tmp_net_list_len = 0;
    size_t i;
    char message[MAX_BUFFER_SIZE] = { 0 };
    int pos = 0;

    work = map_new(MAP_STR_INT, MAP_DEFAULT_CMP_FUNC, MAP_DEFAULT_FREE_FUNC);
    if (work == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    // get new conflist data
    ret = get_net_conflist_from_dir(&tmp_net_list, &tmp_net_list_len, is_cri_config_file);
    if (ret != 0) {
        ERROR("Update new config list failed");
        goto out;
    }
    if (tmp_net_list_len == 0) {
        WARN("No cni config list found");
        goto out;
    }

    for (i = 0; i < tmp_net_list_len; i++) {
        struct cni_network_list_conf *iter = tmp_net_list[i];
        if (iter == NULL) {
            continue;
        }
        if (map_search(work, (void *)iter->list->name) != NULL) {
            INFO("Ignore CNI network: %s, because already exist", iter->list->name);
            continue;
        }

        if (!map_replace(work, (void *)iter->list->name, (void *)&i)) {
            ERROR("add net failed: %s", iter->list->name);
            ret = -1;
            goto out;
        }
        if (strlen(iter->list->name) + 1 < MAX_BUFFER_SIZE - pos) {
            sprintf(message + pos, "%s,", iter->list->name);
            pos += strlen(iter->list->name) + 1;
        }
    }

    // update current conflist data
    do_update_cni_stores(work, tmp_net_list, tmp_net_list_len);
    work = NULL;
    tmp_net_list_len = 0;
    tmp_net_list = NULL;

    if (pos > 0) {
        message[pos - 1] = '\0';
    }
    INFO("Loaded cni plugins successfully, [ %s ]", message);
out:
    for (i = 0; i < tmp_net_list_len; i++) {
        free_cni_network_list_conf(tmp_net_list[i]);
    }
    free(tmp_net_list);
    map_free(work);
    return ret;
}

int adaptor_cni_init_confs(const char *conf_dir, const char **bin_paths, const size_t bin_paths_len)
{
    return adaptor_cni_update_confs();
}

//int attach_network_plane(struct cni_manager *manager, const char *net_list_conf_str);
typedef int (*net_op_t)(const struct cni_manager *manager, const struct cni_network_list_conf *list,
                        struct cni_opt_result **result);

static void prepare_cni_manager(const network_api_conf *conf, struct cni_manager *manager)
{
    manager->annotations = conf->annotations;
    manager->id = conf->pod_id;
    manager->netns_path = conf->netns_path;
    manager->cni_args = conf->args;
}

static int do_append_cni_result(const char *name, const char *interface, const struct cni_opt_result *cni_result,
                                network_api_result_list *result)
{
    struct network_api_result *work = NULL;
    int ret = 0;

    if (cni_result == NULL || result == NULL) {
        return 0;
    }

    if (result->len == result->cap) {
        ERROR("Out of capability of result");
        return -1;
    }

    work = util_common_calloc_s(sizeof(struct network_api_result));
    if (work == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    if (cni_result->ips_len > 0) {
        size_t i;
        work->ips = util_smart_calloc_s(sizeof(char *), cni_result->ips_len);
        if (work->ips == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }
        for (i = 0; i < cni_result->ips_len; i++) {
            work->ips[work->ips_len] = util_ipnet_to_string(cni_result->ips[i]->address);
            if (work->ips[work->ips_len] == NULL) {
                WARN("parse cni result ip: %zu failed", i);
                continue;
            }
            work->ips_len += 1;
        }
    }

    work->name = util_strdup_s(name);
    work->interface = util_strdup_s(interface);
    if (cni_result->interfaces_len > 0) {
        work->mac = util_strdup_s(cni_result->interfaces[0]->mac);
    }

    result->items[result->len] = work;
    result->len += 1;
    work = NULL;
out:
    free_network_api_result(work);
    return ret;
}

static int do_foreach_network_op(const network_api_conf *conf, bool ignore_nofound, net_op_t op,
                                 network_api_result_list *result)
{
    int ret = 0;
    size_t i;
    int default_idx = 0;
    struct cni_manager manager = { 0 };
    const char *default_interface = DEFAULT_NETWORK_INTERFACE;
    struct cni_opt_result *cni_result = NULL;

    if (conf->default_interface != NULL) {
        default_interface = conf->default_interface;
    }

    // Step1, build cni manager config
    prepare_cni_manager(conf, &manager);

    // Step 2, foreach operator for all network plane
    for (i = 0; i < conf->extral_nets_len; i++) {
        int *tmp_idx = NULL;
        if (conf->extral_nets[i] == NULL || conf->extral_nets[i]->name == NULL ||
            conf->extral_nets[i]->interface == NULL) {
            WARN("ignore net idx: %zu", i);
            continue;
        }
        tmp_idx = map_search(g_net_store.g_net_index_map, (void *)conf->extral_nets[i]->name);
        // if user defined network is default network, return error
        if (tmp_idx == NULL || *tmp_idx == 0) {
            ERROR("Cannot found user defined net: %s", conf->extral_nets[i]->name);
            // do best to detach network plane of container
            if (ignore_nofound) {
                continue;
            }
            isulad_set_error_message("Cannot found user defined net: %s", conf->extral_nets[i]->name);
            ret = -1;
            goto out;
        }
        // update interface
        manager.ifname = conf->extral_nets[i]->interface;
        if (strcmp(default_interface, manager.ifname) == 0) {
            default_idx = *tmp_idx;
            continue;
        }

        // clear cni result
        free_cni_opt_result(cni_result);
        cni_result = NULL;

        if (op(&manager, g_net_store.conflist[*tmp_idx], &cni_result) != 0) {
            ERROR("Do op on net: %s failed", conf->extral_nets[i]->name);
            ret = -1;
            goto out;
        }
        if (do_append_cni_result(conf->extral_nets[i]->name, conf->extral_nets[i]->interface, cni_result, result) !=
            0) {
            isulad_set_error_message("parse cni result for net: '%s' failed", conf->extral_nets[i]->name);
            ERROR("parse cni result for net: '%s' failed", conf->extral_nets[i]->name);
            ret = -1;
            goto out;
        }
    }

    if (g_net_store.conflist_len > 0 && default_idx < g_net_store.conflist_len) {
        free_cni_opt_result(cni_result);
        cni_result = NULL;

        manager.ifname = (char *)default_interface;
        ret = op(&manager, g_net_store.conflist[default_idx], &cni_result);
        if (ret != 0) {
            ERROR("Do op on default net: %s failed", g_net_store.conflist[default_idx]->list->name);
            goto out;
        }

        if (do_append_cni_result(g_net_store.conflist[default_idx]->list->name, manager.ifname, cni_result, result) != 0) {
            ERROR("parse cni result failed");
            ret = -1;
            goto out;
        }
    }

out:
    free_cni_opt_result(cni_result);
    return ret;
}

int adaptor_cni_setup(const network_api_conf *conf, network_api_result_list *result)
{
    int ret = 0;

    if (conf == NULL) {
        ERROR("Invalid argument");
        return -1;
    }
    if (g_net_store.conflist_len == 0) {
        ERROR("Not found cni networks");
        return -1;
    }

    // first, attach to loopback network
    ret = attach_loopback(conf->pod_id, conf->netns_path);
    if (ret != 0) {
        ERROR("Attach to loop net failed");
        return -1;
    }

    ret = do_foreach_network_op(conf, false, attach_network_plane, result);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int adaptor_cni_teardown(const network_api_conf *conf, network_api_result_list *result)
{
    int ret = 0;

    if (conf == NULL) {
        ERROR("Invalid argument");
        return -1;
    }
    if (g_net_store.conflist_len == 0) {
        ERROR("Not found cni networks");
        return -1;
    }

    // first, detach to loopback network
    ret = detach_loopback(conf->pod_id, conf->netns_path);
    if (ret != 0) {
        ERROR("Deatch to loop net failed");
        return -1;
    }

    ret = do_foreach_network_op(conf, true, detach_network_plane, result);
    if (ret != 0) {
        return -1;
    }

    return 0;
}
