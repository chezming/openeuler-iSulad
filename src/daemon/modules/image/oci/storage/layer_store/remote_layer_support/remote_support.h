#ifndef DAEMON_MODULES_IMAGE_OCI_STORAGE_LAYER_STORE_REMOTE_LAYER_SUPPORT_REMOTE_SUPPORT_H
#define DAEMON_MODULES_IMAGE_OCI_STORAGE_LAYER_STORE_REMOTE_LAYER_SUPPORT_REMOTE_SUPPORT_H

#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

// typedef int(*layers_handler)(struct linked_list *data);

// typedef struct {
//     struct linked_list pending_data;
//     layers_handler handlers[3];
// } LayersChainHandler;

// LayersChainHandler *create_chain_handler(layers_handler image_scan, layers_handler layer_load, layers_handler overlay_load);
// void destory_chain_handlers(LayersChainHandler *handlers);


typedef struct {
    void *(*create)(const char *remote_home, const char *remote_ro);
    void (*destroy)(void *data);
    // populate the list contains all dirs
    int (*scan_remote_dir)(void *data);
    // consume the list contains all dirs
    int (*load_item)(void *data);
    int (*delete_item)(void *data);
} RemoteSupport;

typedef struct {
    void *data;
    RemoteSupport *handlers;
} RemoteSupporter;

// RemoteSupport *impl_remote_support();
RemoteSupporter *create_image_supporter(const char *remote_home, const char *remote_ro);

RemoteSupporter *create_layer_supporter(const char *remote_home, const char *remote_ro);

RemoteSupporter *create_overlay_supporter(const char *remote_home, const char *remote_ro);

void destroy_suppoter(RemoteSupporter *supporter);

int scan_remote_dir(RemoteSupporter *supporter);

int load_item(RemoteSupporter *supporter);

int delete_item(RemoteSupporter *supporter);

const char *description(RemoteSupporter *supporter);

#ifdef __cplusplus
}
#endif

#endif
