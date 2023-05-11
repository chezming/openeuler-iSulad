/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wangfengtu
 * Create: 2020-02-19
 * Description: provide image mock
 ******************************************************************************/

#include "image_mock.h"

namespace {
MockImage *g_image_mock = nullptr;
}

void MockImage_SetMock(MockImage* mock)
{
    g_image_mock = mock;
}

extern "C" {
int im_container_export(const im_export_request *request)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->ImContainerExport(request);
    }
    return 0;
}

void free_im_export_request(im_export_request *ptr)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->FreeImExportRequest(ptr);
    }
}

int im_mount_container_rootfs(const char *image_type, const char *image_name, const char *container_id)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->ImMountContainerRootfs(image_type, image_name, container_id);
    }
    return 0;
}

int im_umount_container_rootfs(const char *image_type, const char *image_name, const char *container_id)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->ImUmountContainerRootfs(image_type, image_name, container_id);
    }
    return 0;
}

void list_images_all_types(const im_list_request *ctx, imagetool_images_list **images)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->ListImagesAllTypes(ctx, images);
    }
}

int im_if_image_inuse(const char *img_id)
{
    if (g_image_mock != nullptr) {
        return g_image_mock->ImIfImageInuse(img_id);
    }
	return 0;
}

int delete_image(const char *image_ref, bool force)
{
	if (g_image_mock != nullptr) {
		return g_image_mock->DeleteImage(image_ref, force);
	}
	return 0;
}

// Stub for the functions which are not used in image.c 
void free_im_remove_request(im_rmi_request *ptr)
{
}

void free_im_remove_response(im_remove_response *ptr)
{
}

void free_im_search_request(im_search_request *req)
{
}

void free_im_search_response(im_search_response *ptr)
{
}

void free_im_tag_request(im_tag_request *ptr)
{
}

void free_im_tag_response(im_tag_response *ptr)
{
}

void free_im_login_request(im_login_request *ptr)
{
}

void free_im_login_response(im_login_response *ptr)
{
}

void free_im_list_request(im_list_request *ptr)
{
}

void free_im_list_response(im_list_response *ptr)
{
}

void free_im_load_request(im_load_request *ptr)
{
}

void free_im_load_response(im_load_response *ptr)
{
}

void free_im_pull_request(im_pull_request *req)
{
}

void free_im_pull_response(im_pull_response *resp)
{
}

void free_im_logout_request(im_logout_request *ptr)
{
}

void free_im_logout_response(im_logout_response *ptr)
{
}

void free_im_import_request(im_import_request *ptr)
{
}

void free_im_import_response(im_import_response *ptr)
{
}

void free_im_inspect_request(im_inspect_request *ptr)
{
}

void free_im_inspect_response(im_inspect_response *ptr)
{
}

void free_im_status_request(im_status_request *req)
{
}

void free_im_fs_info_response(im_fs_info_response *ptr)
{
}

void free_im_status_response(im_status_response *req)
{
}

int im_search_images(im_search_request *request, im_search_response **response)
{
	return 0;
}

int im_tag_image(const im_tag_request *request, im_tag_response **response)
{
	return 0;
}

int im_login(const im_login_request *request, im_login_response **response)
{
	return 0;
}

int im_logout(const im_logout_request *request, im_logout_response **response)
{
	return 0;
}

int im_list_images(const im_list_request *ctx, im_list_response **response)
{
	return 0;
}

int im_pull_image(const im_pull_request *request, im_pull_response **response)
{
	return 0;
}

int im_get_user_conf(const char *image_type, const char *basefs, host_config *hc, const char *userstr,
                     defs_process_user *puser)
{
	return 0;
}

void im_free_graphdriver_status(struct graphdriver_status *status)
{
}

int im_import_image(const im_import_request *request, char **id)
{
	return 0;
}

int im_load_image(const im_load_request *request, im_load_response **response)
{
	return 0;
}

char *im_get_image_type(const char *image, const char *external_rootfs)
{
	return nullptr;
}

bool im_oci_image_exist(const char *name)
{
	return false;
}

int im_remove_container_rootfs(const char *image_type, const char *container_id)
{
	return 0;
}

char *im_get_rootfs_dir(const im_get_rf_dir_request *request)
{
	return nullptr;
}

int im_remove_broken_rootfs(const char *image_type, const char *container_id)
{
	return 0;
}

int im_inspect_image(const im_inspect_request *request, im_inspect_response **response)
{
	return 0;
}

container_inspect_graph_driver *im_graphdriver_get_metadata_by_container_id(const char *id)
{
	return nullptr;
}

int im_rm_image(const im_rmi_request *request, im_remove_response **response)
{
	return 0;
}

struct graphdriver_status *im_graphdriver_get_status(void)
{
	return nullptr;
}

} // end of extern "C"
