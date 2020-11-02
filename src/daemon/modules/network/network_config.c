/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: zhangxiaoyu
 * Create: 2020-10-31
 * Description: provide network config functions
 ********************************************************************************/

#include "network_config.h"

#include <ifaddrs.h>
#include <arpa/inet.h>

#include "utils.h"
#include "path.h"
#include "error.h"
#include "err_msg.h"
#include "isulad_config.h"
#include "isula_libutils/log.h"
#include "libcni_api.h"
#include "libcni_conf.h"
#include "libcni_types.h"
#include "libcni_utils.h"

// TODO: specify default subnet in daemon.json
const char *default_subnet = "192.201.1.0/24";
const char *network_config_exts[] = { ".conf", ".conflist", ".json" };
const char *file_prefix = "isulacni-";
const char *bridge_name_prefix = "isula-br";
const char *default_driver = "bridge";
const char *bridge_plugins[] = { "bridge", "portmap", "firewall", NULL };

static char *get_cni_conf_dir()
{
    char *dir = NULL;
    char *tmp = NULL;
    char cleaned[PATH_MAX] = { 0 };

    tmp = conf_get_cni_conf_dir();
    if (tmp == NULL) {
        return NULL;
    }

    if (util_clean_path(tmp, cleaned, sizeof(cleaned)) == NULL) {
        ERROR("Can not clean path: %s", tmp);
        goto out;
    }
    dir = util_strdup_s(cleaned);

out:
    free(tmp);
    return dir;
}

static char *get_cni_bin_dir()
{
    char *dir = NULL;
    char *tmp = NULL;
    char cleaned[PATH_MAX] = { 0 };

    tmp = conf_get_cni_bin_dir();
    if (tmp == NULL) {
        return NULL;
    }

    if (util_clean_path(tmp, cleaned, sizeof(cleaned)) == NULL) {
        ERROR("Can not clean path: %s", tmp);
        goto out;
    }
    dir = util_strdup_s(cleaned);

out:
    free(tmp);
    return dir;
}

static int load_cni_list_from_file(const char *file, cni_net_conf_list **list)
{
    int ret = 0;
    int nret = 0;
    struct network_config_list *li = NULL;
    struct network_config *conf = NULL;

    if (util_has_suffix(file, ".conflist")) {
        nret = conflist_from_file(file, &li);
        if (nret != 0) {
            WARN("Failed to load config list from file %s", file);
            goto out;
        }
        if (li == NULL || li->list == NULL) {
            goto out;
        }
    } else {
        nret = conf_from_file(file, &conf);
        if (nret != 0) {
            WARN("Failed to load config from file %s", file);
            goto out;
        }
        if (conf == NULL || conf->network == NULL) {
            goto out;
        }

        nret = conflist_from_conf(conf, &li);
        if (nret != 0) {
            ERROR("Failed to get conflist from conf");
            ret = -1;
            goto out;
        }
        if (li == NULL || li->list == NULL) {
            goto out;
        }
    }
    *list = li->list;
    li->list = NULL;

out:
    free_network_config_list(li);
    free_network_config(conf);
    return ret;
}

static int load_cni_list(const char *cni_conf_dir, cni_net_conf_list ***conflist_arr, size_t *conflist_arr_len)
{
    int ret = 0;
    size_t i, old_size, new_size;
    size_t tmp_conflist_arr_len = 0;
    size_t files_len = 0;
    char **files = NULL;
    cni_net_conf_list **tmp_conflist_arr = NULL;
    cni_net_conf_list *li = NULL;

    ret = conf_files(cni_conf_dir, network_config_exts, sizeof(network_config_exts) / sizeof(char *), &files);
    if (ret != 0) {
        ERROR("Failed to get conf files");
        ret = -1;
        goto out;
    }

    files_len = util_array_len((const char **)files);
    if (files_len == 0) {
        goto out;
    }

    tmp_conflist_arr = util_smart_calloc_s(sizeof(struct cni_net_conf_list *), files_len);
    if (tmp_conflist_arr == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    for (i = 0; i < files_len; i++) {
        ret = load_cni_list_from_file(files[i], &li);
        if (ret != 0) {
            goto out;
        }
        if (li == NULL) {
            continue;
        }
        tmp_conflist_arr[tmp_conflist_arr_len] = li;
        tmp_conflist_arr_len++;
        li = NULL;
    }

    if (files_len != tmp_conflist_arr_len) {
        if (tmp_conflist_arr_len == 0) {
            goto out;
        }

        old_size = files_len * sizeof(cni_net_conf_list *);
        new_size = tmp_conflist_arr_len * sizeof(cni_net_conf_list *);
        ret = util_mem_realloc((void **)&tmp_conflist_arr, new_size, tmp_conflist_arr, old_size);
        if (ret != 0) {
            ERROR("Out of memory");
            goto out;
        }
    }
    *conflist_arr = tmp_conflist_arr;
    tmp_conflist_arr = NULL;
    *conflist_arr_len = tmp_conflist_arr_len;
    tmp_conflist_arr_len = 0;

out:
    util_free_array_by_len(files, files_len);
    for (i = 0; i < tmp_conflist_arr_len; i++) {
        free_cni_net_conf_list(tmp_conflist_arr[i]);
    }
    free(tmp_conflist_arr);
    return ret;
}

typedef int (*get_config_callback)(const cni_net_conf_list *list, char ***arr);

static int get_config_net_name(const cni_net_conf_list *list, char ***arr)
{
    if (list->name == NULL) {
        return 0;
    }

    return util_array_append(arr, list->name);
}

static int get_config_bridge_name(const cni_net_conf_list *list, char ***arr)
{
    size_t i;
    int nret = 0;
    cni_net_conf *plugin = NULL;

    if (list->plugins == NULL) {
        return 0;
    }
    for (i = 0; i < list->plugins_len; i++) {
        plugin = list->plugins[i];
        if (plugin == NULL || strcmp(plugin->type, default_driver) != 0 || plugin->bridge == NULL) {
            continue;
        }
        nret = util_array_append(arr, plugin->bridge);
        if (nret != 0) {
            return -1;
        }
    }

    return 0;
}

static int get_config_subnet(const cni_net_conf_list *list, char ***arr)
{
    size_t i;
    int nret = 0;
    cni_net_conf *plugin = NULL;

    if (list->plugins == NULL) {
        return 0;
    }

    for (i = 0; i < list->plugins_len; i++) {
        plugin = list->plugins[i];
        if (plugin == NULL || plugin->ipam == NULL || plugin->ipam->subnet == NULL) {
            continue;
        }
        nret = util_array_append(arr, plugin->ipam->subnet);
        if (nret != 0) {
            return -1;
        }
    }

    return 0;
}

static int get_cni_config(const cni_net_conf_list **conflist_arr, const size_t conflist_arr_len,
                          get_config_callback cb, char ***arr)
{
    int nret = 0;
    size_t i;

    if (conflist_arr == NULL || conflist_arr_len == 0) {
        return 0;
    }

    for (i = 0; i < conflist_arr_len; i++) {
        if (conflist_arr[i] == NULL) {
            continue;
        }

        nret = cb(conflist_arr[i], arr);
        if (nret != 0) {
            util_free_array(*arr);
            *arr = NULL;
            return -1;
        }
    }

    return 0;
}

static int get_host_net_name(char ***host_net_names)
{
    int ret = 0;
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;

    ret = getifaddrs(&ifaddr);
    if (ret != 0) {
        ERROR("Failed to get if addr");
        return ret;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        // one AF_PACKET address per interface
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET) {
            continue;
        }
        ret = util_array_append(host_net_names, ifa->ifa_name);
        if (ret != 0) {
            goto out;
        }
    }

out:
    freeifaddrs(ifaddr);
    return ret;
}

static int get_host_net_ip(char ***host_net_ip)
{
    int ret = 0;
    char ipaddr[INET6_ADDRSTRLEN] = { 0 };
    struct ifaddrs *ifaddr = NULL;
    struct ifaddrs *ifa = NULL;

    ret = getifaddrs(&ifaddr);
    if (ret != 0) {
        ERROR("Failed to get if addr");
        return ret;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            if (inet_ntop(AF_INET, &(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr),
                          ipaddr, INET_ADDRSTRLEN) == NULL) {
                ERROR("Failed to get ipv4 addr");
                ret = ECOMM;
                goto out;
            }
            ret = util_array_append(host_net_ip, ipaddr);
            if (ret != 0) {
                goto out;
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            if (inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr),
                          ipaddr, INET6_ADDRSTRLEN) == NULL) {
                ERROR("Failed to ipv6 addr");
                ret = ECOMM;
                goto out;
            }
            ret = util_array_append(host_net_ip, ipaddr);
            if (ret != 0) {
                goto out;
            }
        }
    }

out:
    freeifaddrs(ifaddr);
    return ret;
}

/*
 * RETURN VALUE:
 * 0        : net not conflict
 * 1        : net conflict
 * others   : error
 */
static int net_conflict(const struct ipnet *net, const struct ipnet *ipnet)
{
    int ret = 0;
    size_t i = 0;
    uint8_t *first_net = NULL;
    uint8_t *first_ipnet = NULL;

    if (net == NULL || ipnet == NULL) {
        return 0;
    }

    if (net->ip_len != ipnet->ip_len || net->ip_mask_len != ipnet->ip_mask_len) {
        return 0;
    }

    first_net = util_smart_calloc_s(sizeof(uint8_t), net->ip_len);
    if (first_net == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    first_ipnet = util_smart_calloc_s(sizeof(uint8_t), ipnet->ip_len);
    if (first_ipnet == NULL) {
        free(first_net);
        ERROR("Out of memory");
        return -1;
    }

    for (i = 0; i < ipnet->ip_len; i++) {
        first_net[i] = net->ip[i] & net->ip_mask[i];
        first_ipnet[i] = ipnet->ip[i] & ipnet->ip_mask[i];
    }

    if (net_contain_ip(net, first_ipnet, ipnet->ip_len, true) || net_contain_ip(ipnet, first_net, net->ip_len, true)) {
        ret = 1;
    }

    free(first_net);
    free(first_ipnet);
    return ret;
}

/*
 * RETURN VALUE:
 * 0        : subnet available
 * 1        : subnet not avaliable
 * others   : error
 */
static int check_subnet_available(const char *subnet, const char **subnets, const char **hostIP)
{
    int ret = 0;
    size_t len = 0;
    size_t i = 0;
    uint8_t *ip = NULL;
    size_t ip_len = 0;
    struct ipnet *net = NULL;
    struct ipnet *tmp = NULL;

    ret = parse_cidr(subnet, &net);
    if (ret != 0 || net == NULL) {
        ERROR("Parse CIDR %s failed", subnet);
        return -1;
    }

    len = util_array_len(subnets);
    for (i = 0; i < len; i++) {
        ret = parse_cidr(subnets[i], &tmp);
        if (ret != 0 || tmp == NULL) {
            ERROR("Parse CIDR %s failed", subnets[i]);
            ret = -1;
            goto out;
        }
        ret = net_conflict(tmp, net);
        if (ret != 0) {
            goto out;
        }
        free_ipnet_type(tmp);
        tmp = NULL;
    }

    len = util_array_len(hostIP);
    for (i = 0; i < len; i++) {
        ret = parse_ip_from_str(hostIP[i], &ip, &ip_len);
        if (ret != 0 || ip == NULL || ip_len == 0) {
            ERROR("Parse IP %s failed", hostIP[i]);
            ret = -1;
            goto out;
        }
        if (net_contain_ip(net, ip, ip_len, true)) {
            ret = 1;
            goto out;
        }
        free(ip);
        ip = NULL;
        ip_len = 0;
    }

out:
    free(ip);
    free_ipnet_type(net);
    free_ipnet_type(tmp);
    return ret;
}

static int check_conflict(const network_create_request *request, const cni_net_conf_list **conflist_arr,
                          const size_t conflist_arr_len)
{
    int ret = 0;
    char **net_names = NULL;
    char **subnets = NULL;
    char **hostIP = NULL;

    if (request->name != NULL) {
        ret = get_cni_config(conflist_arr, conflist_arr_len, get_config_net_name, &net_names);
        if (ret != 0) {
            goto out;
        }
        if (util_array_contain((const char **)net_names, request->name)) {
            isulad_set_error_message("Network name \"%s\" has been used", request->name);
            ret = EINVALIDARGS;
            goto out;
        }
    }

    if (request->subnet == NULL) {
        goto out;
    }

    ret = get_cni_config(conflist_arr, conflist_arr_len, get_config_subnet, &subnets);
    if (ret != 0) {
        goto out;
    }

    ret = get_host_net_ip(&hostIP);
    if (ret != 0) {
        goto out;
    }

    ret = check_subnet_available(request->subnet, (const char **)subnets, (const char **)hostIP);
    if (ret == 1) {
        isulad_set_error_message("Subnet \"%s\" conflict with CNI config or host network", request->subnet);
        ret = EINVALIDARGS;
    }

out:
    util_free_array(net_names);
    util_free_array(subnets);
    util_free_array(hostIP);
    return ret;
}

static char *find_bridge_name(const cni_net_conf_list **conflist_arr, const size_t conflist_arr_len)
{
    int nret = 0;
    int i = 0;
    char *num = NULL;
    char *name = NULL;
    char **net_names = NULL;
    char **bridge_names = NULL;
    char **host_net_names = NULL;

    nret = get_cni_config(conflist_arr, conflist_arr_len, get_config_net_name, &net_names);
    if (nret != 0) {
        return NULL;
    }

    nret = get_cni_config(conflist_arr, conflist_arr_len, get_config_bridge_name, &bridge_names);
    if (nret != 0) {
        goto out;
    }

    nret = get_host_net_name(&host_net_names);
    if (nret != 0) {
        goto out;
    }

    for (i = 0; i < MAX_BRIDGE_ID; i++) {
        free(name);
        name = NULL;
        free(num);
        num = NULL;

        num = util_int_to_string(i);
        if (num == NULL) {
            goto out;
        }
        name = util_string_append(num, bridge_name_prefix);
        if (name == NULL) {
            goto out;
        }
        if (net_names != NULL && util_array_contain((const char **)net_names, name)) {
            continue;
        }
        if (bridge_names != NULL && util_array_contain((const char **)bridge_names, name)) {
            continue;
        }
        if (host_net_names != NULL && util_array_contain((const char **)host_net_names, name)) {
            continue;
        }
        goto out;
    }
    free(name);
    name = NULL;
    isulad_set_error_message("Too many network bridges");

out:
    free(num);
    util_free_array(net_names);
    util_free_array(bridge_names);
    util_free_array(host_net_names);
    return name;
}

static char *find_next_subnet(char *subnet)
{
    int nret = 0;
    int i = 0;
    uint32_t ip = 0;
    uint32_t mask = 0;
    struct ipnet *ipnet = NULL;
    char *nx = NULL;

    if (subnet == NULL) {
        return NULL;
    }

    nret = parse_cidr(subnet, &ipnet);
    if (nret != 0 || ipnet == NULL) {
        ERROR("Parse IP %s failed", subnet);
        return NULL;
    }
    for (i = 0; i < ipnet->ip_len; i++) {
        ip <<= 8;
        mask <<= 8;
        ip += (uint32_t)(ipnet->ip[i]);
        mask += (uint32_t)(ipnet->ip_mask[i]);
    }
    mask = ~mask + 1;
    ip += mask;
    mask = 0xff;
    for (i = ipnet->ip_len - 1; i >= 0 ; i--) {
        ipnet->ip[i] = (uint8_t)(ip & mask);
        ip >>= 8;
    }

    nx = ipnet_to_string(ipnet);
    free_ipnet_type(ipnet);

    return nx;
}

static char *find_subnet(const cni_net_conf_list **conflist_arr, const size_t conflist_arr_len)
{
    int nret = 0;
    size_t i;
    char *subnet = NULL;
    char *nx_subnet = NULL;
    char **config_subnet = NULL;
    char **hostIP = NULL;

    nret = get_cni_config(conflist_arr, conflist_arr_len, get_config_subnet, &config_subnet);
    if (nret != 0) {
        return NULL;
    }

    nret = get_host_net_ip(&hostIP);
    if (nret != 0) {
        goto out;
    }

    subnet = util_strdup_s(default_subnet);
    for (i = 0; i < MAX_SUBNET_INCREASE; i++) {
        nret = check_subnet_available(subnet, (const char **)config_subnet, (const char **)hostIP);
        if (nret == 0) {
            goto out;
        }
        if (nret != 1) {
            free(subnet);
            subnet = NULL;
            goto out;
        }

        nx_subnet = find_next_subnet(subnet);
        if (nx_subnet == NULL) {
            free(subnet);
            subnet = NULL;
            goto out;
        }
        free(subnet);
        subnet = nx_subnet;
        nx_subnet = NULL;
    }
    free(subnet);
    subnet = NULL;
    isulad_set_error_message("Cannot find avaliable subnet by default");

out:
    util_free_array(config_subnet);
    util_free_array(hostIP);
    return subnet;
}

static char *find_gateway(const char *subnet)
{
    int nret = 0;
    size_t i;
    uint8_t *first_ip = NULL;
    char *gateway = NULL;
    struct ipnet *ipnet = NULL;

    nret = parse_cidr(subnet, &ipnet);
    if (nret != 0 || ipnet == NULL) {
        ERROR("Parse IP %s failed", subnet);
        return NULL;
    }

    first_ip = util_smart_calloc_s(sizeof(uint8_t), ipnet->ip_len);
    if (first_ip == NULL) {
        ERROR("Out of memory");
        goto out;
    }

    if (ipnet->ip_mask[ipnet->ip_mask_len - 1] == 0xff) {
        isulad_set_error_message("No avaliable gateway in %s", subnet);
        goto out;
    }

    for (i = 0; i < ipnet->ip_len; i++) {
        first_ip[i] = ipnet->ip[i] & ipnet->ip_mask[i];
    }
    first_ip[ipnet->ip_len - 1] = first_ip[ipnet->ip_len - 1] | 0x01;
    gateway = ip_to_string(first_ip, ipnet->ip_len);

out:
    free_ipnet_type(ipnet);
    free(first_ip);
    return gateway;
}

static cni_net_conf_ipam *conf_bridge_ipam(const network_create_request *request,
                                           const cni_net_conf_list **conflist_arr,
                                           const size_t conflist_arr_len)
{
    cni_net_conf_ipam *ipam = NULL;

    ipam = util_common_calloc_s(sizeof(cni_net_conf_ipam));
    if (ipam == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    ipam->type = util_strdup_s("host-local");
    ipam->routes = util_common_calloc_s(sizeof(cni_network_route *));
    if (ipam->routes == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }
    ipam->routes[0] = util_common_calloc_s(sizeof(cni_network_route));
    if (ipam->routes[0] == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }
    ipam->routes_len++;
    ipam->routes[0]->dst = util_strdup_s("0.0.0.0/0");

    if (request->subnet != NULL) {
        ipam->subnet = util_strdup_s(request->subnet);
    } else {
        ipam->subnet = find_subnet(conflist_arr, conflist_arr_len);
        if (ipam->subnet == NULL) {
            ERROR("Failed to find available subnet");
            goto err_out;
        }
    }

    if (request->gateway != NULL) {
        ipam->gateway = util_strdup_s(request->gateway);
    } else {
        ipam->gateway = find_gateway(ipam->subnet);
        if (ipam->gateway == NULL) {
            ERROR("Failed to find gateway");
            goto err_out;
        }
    }

    return ipam;

err_out:
    free_cni_net_conf_ipam(ipam);
    return NULL;
}

static cni_net_conf *conf_bridge_plugin(const network_create_request *request, const cni_net_conf_list **conflist_arr,
                                        const size_t conflist_arr_len)
{
    cni_net_conf *plugin = NULL;

    plugin = util_common_calloc_s(sizeof(cni_net_conf));
    if (plugin == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    if (request->driver == NULL) {
        plugin->type = util_strdup_s(default_driver);
    } else {
        plugin->type = util_strdup_s(request->driver);
    }

    plugin->bridge = find_bridge_name(conflist_arr, conflist_arr_len);
    if (plugin->bridge == NULL) {
        ERROR("Failed to find avaliable bridge name");
        goto err_out;
    }

    if (request->internal) {
        plugin->is_gateway = false;
        plugin->ip_masq = false;
    } else {
        plugin->is_gateway = true;
        plugin->ip_masq = true;
    }
    plugin->hairpin_mode = true;

    plugin->ipam = conf_bridge_ipam(request, conflist_arr, conflist_arr_len);
    if (plugin->ipam == NULL) {
        ERROR("Failed to config bridge ipam");
        goto err_out;
    }

    return plugin;

err_out:
    free_cni_net_conf(plugin);
    return NULL;
}

static cni_net_conf *conf_portmap_plugin(const network_create_request *request)
{
    cni_net_conf *plugin = NULL;

    plugin = util_common_calloc_s(sizeof(cni_net_conf));
    if (plugin == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    plugin->type = util_strdup_s("portmap");
    plugin->capabilities = util_common_calloc_s(sizeof(json_map_string_bool));
    if (plugin->capabilities == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }
    plugin->capabilities->keys = util_common_calloc_s(sizeof(char *));
    if (plugin->capabilities->keys == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }
    plugin->capabilities->keys[0] = util_strdup_s("portMappings");
    plugin->capabilities->values = util_common_calloc_s(sizeof(bool));
    if (plugin->capabilities->values == NULL) {
        free(plugin->capabilities->keys[0]);
        ERROR("Out of memory");
        goto err_out;
    }
    plugin->capabilities->values[0] = true;
    plugin->capabilities->len++;

    return plugin;

err_out:
    free_cni_net_conf(plugin);
    return NULL;
}

static cni_net_conf *conf_firewall_plugin(const network_create_request *request)
{
    cni_net_conf *plugin = NULL;

    plugin = util_common_calloc_s(sizeof(cni_net_conf));
    if (plugin == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    plugin->type = util_strdup_s("firewall");

    return plugin;
}

static cni_net_conf_list *conf_bridge_conflist(const network_create_request *request,
                                               const cni_net_conf_list **conflist_arr,
                                               const size_t conflist_arr_len)
{
    size_t len;
    cni_net_conf *plugin = NULL;
    cni_net_conf_list *list = NULL;

    list = util_common_calloc_s(sizeof(cni_net_conf_list));
    if (list == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    len = util_array_len(bridge_plugins);
    list->plugins = (cni_net_conf **)util_smart_calloc_s(sizeof(cni_net_conf *), len);
    if (list->plugins == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }

    list->plugins_len = 0;
    plugin = conf_bridge_plugin(request, conflist_arr, conflist_arr_len);
    if (plugin == NULL) {
        ERROR("Failed to config bridge plugin");
        goto err_out;
    }
    list->plugins[list->plugins_len] = plugin;
    list->plugins_len++;

    plugin = conf_portmap_plugin(request);
    if (plugin == NULL) {
        ERROR("Failed to config portmap plugin");
        goto err_out;
    }
    list->plugins[list->plugins_len] = plugin;
    list->plugins_len++;

    plugin = conf_firewall_plugin(request);
    if (plugin == NULL) {
        ERROR("Failed to config firewall plugin");
        goto err_out;
    }
    list->plugins[list->plugins_len] = plugin;
    list->plugins_len++;

    list->cni_version = util_strdup_s(CURRENT_VERSION);
    if (request->name != NULL) {
        list->name = util_strdup_s(request->name);
    } else {
        // config bridge as conflist name
        list->name = util_strdup_s(list->plugins[0]->bridge);
    }

    return list;

err_out:
    free_cni_net_conf_list(list);
    return NULL;
}

static int do_create_conflist_file(const char *cni_conf_dir, cni_net_conf_list *list, char **path)
{
    int ret = 0;
    int nret = 0;
    char conflist_file[PATH_MAX] = { 0x00 };
    char *conflist_json = NULL;
    parser_error err = NULL;

    if (!util_dir_exists(cni_conf_dir)) {
        ret = util_mkdir_p(cni_conf_dir, CONFIG_DIRECTORY_MODE);
        if (ret != 0) {
            ERROR("Failed to create network config directory %s", cni_conf_dir);
            return -1;
        }
    }

    nret = snprintf(conflist_file, sizeof(conflist_file), "%s/%s%s.conflist", cni_conf_dir, file_prefix, list->name);
    if ((size_t)nret >= sizeof(conflist_file) || nret < 0) {
        ERROR("Failed to snprintf conflist_file");
        return -1;
    }

    conflist_json = cni_net_conf_list_generate_json(list, NULL, &err);
    if (conflist_json == NULL) {
        ERROR("Failed to generate conf list json: %s", err);
        ret = -1;
        goto out;
    }

    if (util_file_exists(conflist_file)) {
        ERROR("File %s exist", conflist_file);
        isulad_set_error_message("File %s exist", conflist_file);
        ret = -1;
        goto out;
    }

    if (util_atomic_write_file(conflist_file, conflist_json, strlen(conflist_json), CONFIG_FILE_MODE) != 0) {
        ERROR("Failed write %s", conflist_file);
        ret = -1;
        goto out;
    }

    EVENT("Event: {Object: network %s, Type: create}", list->name);
    *path = util_strdup_s(conflist_file);

out:
    free(conflist_json);
    free(err);
    return ret;
}

static int cni_bin_detect(const char *cni_bin_dir)
{
    int ret = 0;
    size_t i, len;
    char *plugin_file = NULL;
    char **missing_file = NULL;
    char *str = NULL;

    if (!util_dir_exists(cni_bin_dir)) {
        isulad_set_error_message("WARN:cni plugin dir \"%s\" doesn't exist", cni_bin_dir);
        return 0;
    }

    len = util_array_len(bridge_plugins);
    for (i = 0; i < len; i++) {
        plugin_file = util_path_join(cni_bin_dir, bridge_plugins[i]);
        if (plugin_file == NULL) {
            ret = -1;
            goto out;
        }
        if (!util_file_exists(plugin_file)) {
            ret = util_array_append(&missing_file, bridge_plugins[i]);
            if (ret != 0) {
                free(plugin_file);
                ERROR("Out of memory");
                goto out;
            }
        }
        free(plugin_file);
        plugin_file = NULL;
    }

    if (missing_file == NULL) {
        return ret;
    }

    len = util_array_len((const char **)missing_file);
    str = util_string_join(", ", (const char **)missing_file, len);
    if (str == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    isulad_set_error_message("WARN:missing cni plugin \"%s\" in dir %s", str, cni_bin_dir);
    free(str);

out:
    util_free_array(missing_file);
    return ret;
}

static void free_cni_list_arr(cni_net_conf_list **list_arr, size_t list_arr_len)
{
    size_t i;

    if (list_arr == NULL) {
        return;
    }
    for (i = 0; i < list_arr_len; i++) {
        free_cni_net_conf_list(list_arr[i]);
    }
    free(list_arr);
}

int bridge_network_config_create(const network_create_request *request, network_create_response **response)
{
    int ret = 0;
    size_t conflist_arr_len = 0;
    uint32_t cc = ISULAD_SUCCESS;
    char *cni_conf_dir = NULL;
    char *cni_bin_dir = NULL;
    cni_net_conf_list *bridge_list = NULL;
    cni_net_conf_list **conflist_arr = NULL;

    cni_conf_dir = get_cni_conf_dir();
    if (cni_conf_dir == NULL) {
        ERROR("Failed to get cni conf dir");
        cc = ISULAD_ERR_EXEC;
        ret = -1;
        goto out;
    }

    ret = load_cni_list(cni_conf_dir, &conflist_arr, &conflist_arr_len);
    if (ret != 0) {
        isulad_set_error_message("Failed to load cni list, maybe the count of network config files is above 200");
        cc = ISULAD_ERR_EXEC;
        goto out;
    }

    ret = check_conflict(request, (const cni_net_conf_list **)conflist_arr, conflist_arr_len);
    if (ret != 0) {
        cc = ISULAD_ERR_INPUT;
        goto out;
    }

    bridge_list = conf_bridge_conflist(request, (const cni_net_conf_list **)conflist_arr, conflist_arr_len);
    if (bridge_list == NULL) {
        cc = ISULAD_ERR_EXEC;
        ret = -1;
        goto out;
    }

    ret = do_create_conflist_file(cni_conf_dir, bridge_list, &(*response)->path);
    if (ret != 0) {
        cc = ISULAD_ERR_EXEC;
        goto out;
    }

    cni_bin_dir = get_cni_bin_dir();
    if (cni_bin_dir == NULL) {
        ERROR("Failed to get cni bin dir");
        cc = ISULAD_ERR_EXEC;
        ret = -1;
        goto out;
    }

    ret = cni_bin_detect(cni_bin_dir);
    if (ret != 0) {
        cc = ISULAD_ERR_EXEC;
    }

out:
    free(cni_bin_dir);
    free(cni_conf_dir);
    free_cni_net_conf_list(bridge_list);
    free_cni_list_arr(conflist_arr, conflist_arr_len);

    (*response)->cc = cc;
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
        DAEMON_CLEAR_ERRMSG();
    }

    return ret;
}