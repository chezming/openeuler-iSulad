#include "manager.h"
#include "isula_libutils/utils.h"
#include "isula_libutils/log.h"
#include "utils.h"
#include "libcni_utils.h"

#define LO_IFNAME "lo"
#define LO_NETNAME "cni-loopback"

typedef struct cni_manager_t {
    char *default_name;
    char *conf_path;
    char **bin_paths;
    size_t bin_paths_len;
    char *cache_dir;
} cni_manager_t;

typedef struct cni_manager_network_conf_list_t {
    struct cni_network_list_conf **conflist;
    size_t conflist_len;
} cni_manager_network_conf_list_t;

static cni_manager_t g_cni_manager;
static cni_manager_network_conf_list_t g_conflists;

int cni_manager_init(const char *cache_dir, const char *conf_path, const char* const *bin_paths, size_t bin_paths_len, const char *default_name)
{
    int ret = 0;
    if (cache_dir == NULL || conf_path == NULL || default_name == NULL) {
        ERROR("Invalid input params");
        return -1;
    }

    if (!cni_module_init(cache_dir, bin_paths, bin_paths_len)) {
        ERROR("Init libcni module failed");
        ret = -1;
        goto out;
    }

    g_cni_manager.conf_path = util_strdup_s(conf_path);
    g_cni_manager.cache_dir = util_strdup_s(cache_dir);
    g_cni_manager.default_name = util_strdup_s(default_name);

out:
    return ret;
}

static int load_cni_config_file_list(const char *fname, struct cni_network_list_conf **n_list)
{
    int ret = 0;
    struct cni_network_conf *n_conf = NULL;

    if (fname == NULL || n_list == NULL) {
        ERROR("Invalid NULL params");
        return -1;
    }

    if (util_has_suffix(fname, ".conflist")) {
        if (cni_conflist_from_file(fname, n_list)) {
            ERROR("Error loading CNI config list file %s", fname);
            ret = -1;
            goto out;
        }
    } else { 
        if (cni_conf_from_file(fname, &n_conf)) {
            ERROR("Error loading CNI config file %s", fname);
            ret = -1;
            goto out;
        }
            
        if (!util_valid_str(n_conf->type)) {
            WARN("Error loading CNI config file %s: no 'type'; perhaps this is a .conflist?", fname);
            ret = -1;
            goto out;
        }

        if (cni_conflist_from_conf(n_conf, n_list) != 0) {
            WARN("Error converting CNI config file %s to list", fname);
            ret = -1;
            goto out;
        }
    }

out:
    if (n_conf != NULL) {
        free_cni_network_conf(n_conf);
    }

    return ret;
}

static int cni_manager_update_conflist_from_files(cni_manager_network_conf_list_t *store, const char **files)
{
    int ret = 0;
    size_t i = 0;

    if (g_cni_manager.conf_path == NULL) {
        ERROR("CNI conf path is null");
        return -1;
    }

    for (i = 0; i < store->conflist_len; i++) {
        struct cni_network_list_conf *n_list = NULL;
        char *fpath = NULL;

        fpath = util_path_join(g_cni_manager.conf_path, files[i]);
        if (fpath == NULL) {
            ERROR("Failed to get CNI conf file:%s full path", files[i]);
            ret = -1;
            goto out;
        }

        if (load_cni_config_file_list(fpath, &n_list) != 0) {
            free(fpath);
            continue;
        }

        free(fpath);
        if (n_list == NULL || n_list->plugin_len == 0) {
            WARN("CNI config list %s has no networks, skipping", files[i]);
            free_cni_network_list_conf(n_list);
            n_list = NULL;
            continue;
        }

        store->conflist[i] = n_list;
        n_list = NULL;
    }

out:
    return ret;
}

const struct cni_network_list_conf **cni_update_confist_from_dir(const char *conf_dir, size_t *len)
{
    size_t i = 0;
    int ret = 0;
    size_t length = 0;
    const char *exts[] = { ".conf", ".conflist", ".json" };
    char **files = NULL;

    if (conf_dir == NULL || len == NULL) {
        ERROR("Invalid input param, conflist dir is NULL");
        return NULL;
    }

    if (cni_conf_files(conf_dir, exts, sizeof(exts) / sizeof(char *), &files) != 0) {
        ERROR("Get conf files from dir:%s failed", conf_dir);
        ret = -1;
        goto out;
    }

    length = util_array_len((const char **)files);
    if (length == 0) {
        ERROR("No network conf files found");
        ret = -1;
        goto out;
    }

    for (i = 0; i < g_conflists.conflist_len; i++) {
        free_cni_network_list_conf(g_conflists.conflist[i]);
    }
    free(g_conflists.conflist);
    g_conflists.conflist = NULL;

    g_conflists.conflist_len = length;
    g_conflists.conflist = (struct cni_network_list_conf **)util_common_calloc_s(sizeof(struct cni_network_list_conf *) * g_conflists.conflist_len);
    if (g_conflists.conflist == NULL) {
        ERROR("Out of memory, cannot allocate mem to store conflists");
        ret = -1;
        goto out;
    }

    if (cni_manager_update_conflist_from_files(&g_conflists, (const char **)files) != 0) {
        ERROR("Update conflist from files failed");
        ret = -1;
        goto out;
    }

    *len = length;

out:
    util_free_array(files);
    if (ret != 0) {
        return NULL;
    }

    return (const struct cni_network_list_conf **)g_conflists.conflist;
}

static struct runtime_conf *build_loopback_runtime_conf(const char *cid, const char *netns_path)
{
    struct runtime_conf *rt = NULL;

    if (cid == NULL || netns_path == NULL) {
        ERROR("Invalid input params");
        return NULL;
    }

    rt = util_common_calloc_s(sizeof(struct runtime_conf));
    if (rt == NULL) {
        ERROR("Out of memory");
        goto out;
    }

    rt->container_id = util_strdup_s(cid);
    rt->netns = util_strdup_s(netns_path);
    rt->ifname = util_strdup_s(LO_IFNAME);

out:
    return rt;
}

static char *get_lo_network()
{
    cni_net_conf_list *conf = NULL;
    parser_error err = NULL;
    char *json = NULL;

    conf = util_common_calloc_s(sizeof(cni_net_conf_list));
    if (conf == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    conf->cni_version = util_strdup_s("0.3.1");
    conf->name = util_strdup_s(LO_NETNAME);
    conf->plugins = util_common_calloc_s(sizeof(cni_net_conf*));
    conf->plugins[0] = util_common_calloc_s(sizeof(cni_net_conf));
    conf->plugins[0]->type = util_strdup_s("loopback");
    conf->plugins_len = 1;

    json = cni_net_conf_list_generate_json(conf, NULL, &err);
    if (json == NULL) {
        ERROR("Generate conf list json failed: %s", err);
        goto out;
    }

out:
    free_cni_net_conf_list(conf);
    free(err);
    return json;
}

static bool validate_ip(const char *ip)
{
    return true;
}

static bool validate_mac(const char *mac)
{
    return true;
}
static struct runtime_conf *build_cni_runtime_conf(struct cni_manager *manager)
{
    struct runtime_conf *rt = NULL;
    const size_t default_len = 6;

    rt = (struct runtime_conf *)util_common_calloc_s(sizeof(struct runtime_conf));
    if (rt == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    rt->container_id = util_strdup_s(manager->id);
    rt->netns = util_strdup_s(manager->netns_path);
    rt->ifname = util_strdup_s(manager->ifname);

    rt->args = (char *(*)[2])util_common_calloc_s(sizeof(char *) * 2 * default_len);
    if (rt->args == NULL) {
        ERROR("Out of memory");
        goto free_out;
    }

    rt->args_len = default_len;
    rt->args[0][0] = util_strdup_s("IgnoreUnknown");
    rt->args[0][1] = util_strdup_s("1");
    rt->args[1][0] = util_strdup_s("K8S_POD_NAMESPACE");
    rt->args[1][1] = util_strdup_s(manager->namespace);
    rt->args[2][0] = util_strdup_s("K8S_POD_NAME");
    rt->args[2][1] = util_strdup_s(manager->name);
    rt->args[3][0] = util_strdup_s("K8S_POD_INFRA_CONTAINER_ID");
    rt->args[3][1] = util_strdup_s(manager->id);

    if (manager->ip != NULL) {
        if (validate_ip(manager->ip)) {
            rt->args[4][0] = util_strdup_s("IP");
            rt->args[4][1] = util_strdup_s(manager->ip);
        } else {
            ERROR("Unable to parse IP address:%s", manager->ip);
            goto free_out;
        }

    }

    if (manager->mac != NULL) {
        if (validate_mac(manager->mac)) {
            rt->args[5][0] = util_strdup_s("MAC");
            rt->args[5][1] = util_strdup_s(manager->mac);
        } else {
            ERROR("Unable to parse MAC address:%s", manager->mac);
            goto free_out;
        }
    }

    rt->p_mapping = manager->p_mapping;
    rt->p_mapping_len = manager->p_mapping_len;
    rt->bandwidth = manager->bandwidth;

    return rt;

free_out:
    free_runtime_conf(rt);
    return NULL;

}

static struct result *add_to_network(const char *net_list_conf_str, struct runtime_conf *rt)
{
    struct result *presult = NULL;
    
    if (cni_add_network_list(net_list_conf_str, rt, &presult) != 0) {
        ERROR("Error adding network");
        presult = NULL;
    }

    return presult;
}

int setup_pod(struct cni_manager *manager, struct result **results)
{
    int ret = 0;
    struct runtime_conf *lo_rt = NULL;
    struct runtime_conf *net_rt = NULL;
    char *lo_network_str = NULL;
    struct result *lo_result = NULL;
    struct result *net_result = NULL;

    if (manager == NULL || results == NULL) {
        ERROR("Invalid input params");
        return -1;
    }

    lo_rt = build_loopback_runtime_conf(manager->id, manager->netns_path);
    if (lo_rt == NULL) {
        ERROR("Error while adding to cni lo network");
        ret = -1;
        goto out;
    }

    lo_network_str = get_lo_network();
    if (lo_network_str == NULL) {
        ERROR("Get loopback cni network conflist str failed");
        ret = -1;
        goto out;
    }

    lo_result = add_to_network(lo_network_str, lo_rt);
    if (lo_result == NULL) {
        ERROR("Add loopback network failed");
        ret = -1;
        goto out;
    }

    net_rt = build_cni_runtime_conf(manager);
    if (net_rt == NULL) {
        ERROR("Error building CNI runtime config");
        ret = -1;
        goto out;
    }

    net_result = add_to_network(manager->net_list_conf_str, net_rt);
    if (net_result == NULL) {
        ERROR("Add CNI network failed");
        ret = -1;
        goto out;
    }

    *results = net_result;
    net_result = NULL;

out:
    free_runtime_conf(lo_rt);
    free_runtime_conf(net_rt);
    free(lo_network_str);
    free_result(lo_result);
    return ret;
}

int tear_down_pod(struct cni_manager *manager)
{
    int ret = 0;
    struct runtime_conf *net_rt = NULL;


    if (manager == NULL) {
        ERROR("Invalid input param");
        return -1;
    }

    net_rt = build_cni_runtime_conf(manager);
    if (net_rt == NULL) {
        ERROR("Error building CNI runtime config");
        ret = -1;
        goto out;
    }

    if (cni_del_network_list(manager->net_list_conf_str, net_rt) != 0) {
        ERROR("Error deleting network: %s", manager->name);
        ret =-1;
        goto out;
    }

out:
    free_runtime_conf(net_rt);
    return ret;
}

int get_pod_network_status(struct cni_manager *magager, struct result **res)
{
    // TODO
    return 0;
}


void free_cni_manager(struct cni_manager *manager)
{
    size_t i = 0;

    if (manager == NULL) {
        return;
    }

    UTIL_FREE_AND_SET_NULL(manager->id);
    UTIL_FREE_AND_SET_NULL(manager->ifname);
    UTIL_FREE_AND_SET_NULL(manager->name);
    UTIL_FREE_AND_SET_NULL(manager->namespace);
    UTIL_FREE_AND_SET_NULL(manager->netns_path);
    UTIL_FREE_AND_SET_NULL(manager->net_list_conf_str);
    UTIL_FREE_AND_SET_NULL(manager->mac);
    UTIL_FREE_AND_SET_NULL(manager->ip);

    for (i = 0; i < manager->p_mapping_len; i++) {
        free_cni_port_mapping(manager->p_mapping[i]);
    }
    free(manager->p_mapping);
    manager->p_mapping = NULL;

    free_cni_bandwidth_entry(manager->bandwidth);
    manager->bandwidth = NULL;

    free(manager);
    manager = NULL;
}