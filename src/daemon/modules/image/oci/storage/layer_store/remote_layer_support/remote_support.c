#include "remote_support.h"
#include "layer_store.h"
#include "image_store.h"
#include "driver_overlay2.h"
#include "utils.h"
#include "isula_libutils/log.h"

RemoteSupporter *create_layer_supporter(const char *remote_home, const char *remote_ro)
{
    RemoteSupport *handlers = layer_store_impl_remote_support();
    if (handlers == NULL || handlers->create == NULL) {
        return NULL;
    }

    RemoteSupporter *supporter = (RemoteSupporter *)util_common_calloc_s(sizeof(RemoteSupporter));
    if (supporter == NULL) {
        goto err_out;
    }

    supporter->handlers = handlers;
    supporter->data = handlers->create(remote_home, remote_ro);

    return supporter;

err_out:
    free(handlers);
    free(supporter);
    return NULL;
}

RemoteSupporter *create_image_supporter(const char *remote_home, const char *remote_ro)
{
    RemoteSupport *handlers = image_store_impl_remote_support();
    if (handlers == NULL || handlers->create == NULL) {
        return NULL;
    }

    RemoteSupporter *supporter = (RemoteSupporter *)util_common_calloc_s(sizeof(RemoteSupporter));
    if (supporter == NULL) {
        goto err_out;
    }

    supporter->handlers = handlers;
    supporter->data = handlers->create(remote_home, remote_ro);

    return supporter;

err_out:
    free(handlers);
    free(supporter);
    return NULL;
}

RemoteSupporter *create_overlay_supporter(const char *remote_home, const char *remote_ro)
{
    RemoteSupport *handlers = overlay_driver_impl_remote_support();
    if (handlers == NULL || handlers->create == NULL) {
        return NULL;
    }

    RemoteSupporter *supporter = (RemoteSupporter *)util_common_calloc_s(sizeof(RemoteSupporter));
    if (supporter == NULL) {
        goto err_out;
    }

    supporter->handlers = handlers;
    supporter->data = handlers->create(remote_home, remote_ro);

    return supporter;

err_out:
    free(handlers);
    free(supporter);
    return NULL;

}

void destroy_suppoter(RemoteSupporter *supporter)
{
    if (supporter->handlers->destroy == NULL) {
        return;
    }

    supporter->handlers->destroy(supporter->data);
    free(supporter->handlers);
    free(supporter);
}

int scan_remote_dir(RemoteSupporter *supporter)
{
    if (supporter->handlers->scan_remote_dir == NULL) {
        // ERROR("operation not supported for type: %s", supporter->handlers->description(supporter->data));
        return -1;
    }
    return supporter->handlers->scan_remote_dir(supporter->data);
}

int load_item(RemoteSupporter *supporter)
{
    if (supporter->handlers->scan_remote_dir == NULL) {
        // ERROR("operation not supported for type: %s", supporter->handlers->description(supporter->data));
        return -1;
    }
    return supporter->handlers->load_item(supporter->data);
}

int delete_item(RemoteSupporter *supporter)
{
    if (supporter->handlers->scan_remote_dir == NULL) {
        // ERROR("operation not supported for type: %s", supporter->handlers->description(supporter->data));
        return -1;
    }
    return supporter->handlers->delete_item(supporter->data);
}
