#include "isula_libutils/cni_net_conf.h"
#include "isula_libutils/cni_net_conf_list.h"
#include "map.h"
#include "libcni_types.h"
#include "libcni_api.h"

// cni_manager holds cniNetworkPlugin and podNetwork infos
struct cni_manager {
    // The name of the sandbox
    char *name;
    // The namespace of the sandbox
    char *namespace;
    // id of the sandbox container
    char *id;
    // The network namespace path of the sandbox
    char *netns_path;

    char *ifname;
    char *net_list_conf_str;
    // ip is a static ip to be specified in the network. Can only be used with the hostlocal ip allocator. If left unset, an ip will be
    // dynamically allocated.
    char *ip;
    // A static MAC address to be assigned to the network interface. If left unset, a MAC will be dynamically allocated.
    char *mac;

    cni_bandwidth_entry *bandwidth;
    struct cni_port_mapping **p_mapping;
    size_t p_mapping_len;
};

const struct cni_network_list_conf **cni_update_confist_from_dir(const char *conf_dir, size_t *len);

int cni_manager_init(const char *cache_dir, const char *conf_path, const char* const *bin_paths, size_t bin_paths_len, const char *default_name);

int setup_pod(struct cni_manager *magager, struct result **results);

int tear_down_pod(struct cni_manager *magager);

int get_pod_network_status(struct cni_manager *magager, struct result **results);

void free_cni_manager(struct cni_manager *manager);