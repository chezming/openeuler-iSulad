#ifndef DAEMON_MODULES_IMAGE_OCI_STORAGE_LAYER_STORE_REMOTE_LAYER_SUPPORT_RO_SYMLINK_MAINTAIN_H
#define DAEMON_MODULES_IMAGE_OCI_STORAGE_LAYER_STORE_REMOTE_LAYER_SUPPORT_RO_SYMLINK_MAINTAIN_H

#ifdef __cplusplus
extern "C" {
#endif

int remote_layer_init(const char *root_dir);

int remote_overlay_init(const char *driver_home);

void remote_maintain_cleanup();

int start_refresh_thread(void);

int remote_layer_build_ro_dir(const char *id);

int remote_overlay_build_ro_dir(const char *id);

int remote_layer_remove_ro_dir(const char *id);

int remote_overlay_remove_ro_dir(const char *id);

#ifdef __cplusplus
}
#endif

#endif
