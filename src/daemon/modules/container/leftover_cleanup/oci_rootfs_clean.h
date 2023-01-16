/******************************************************************************
<<<<<<<< HEAD:src/daemon/entry/connect/rest/rest_metrics_service.c
 * Copyright (c) KylinSoft  Co., Ltd. 2021. All rights reserved.
========
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2022. All rights reserved.
>>>>>>>> origin/cri-1.25_support:src/daemon/modules/container/leftover_cleanup/oci_rootfs_clean.h
 * iSulad licensed under the Mulan PSL v2.

 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
<<<<<<<< HEAD:src/daemon/entry/connect/rest/rest_metrics_service.c
 * Author: xiapin
 * Create: 2021-08-17
 * Description: provide metric restful service function
 ******************************************************************************/
#ifdef ENABLE_METRICS
#include "rest_metrics_service.h"
#include "metrics_service.h"
#include "isula_libutils/log.h"

#include "callback.h"

int rest_register_metrics_handler(evhtp_t *htp)
{
    if (evhtp_set_cb(htp, METRIC_GET_BY_TYPE, metrics_get_by_type_cb, NULL) == NULL) {
        ERROR("Failed to register metrics get callback");
        return -1;
    }

    return 0;
}
#endif
========
 * Author: wangrunze
 * Create: 2022-10-31
 * Description: provide rootfs cleaner definition
 *********************************************************************************/
#ifndef DAEMON_MODULES_CONTAINER_ROOTFS_CLEAN_H
#define DAEMON_MODULES_CONTAINER_ROOTFS_CLEAN_H

#include "cleanup.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

int oci_rootfs_cleaner(struct clean_ctx *ctx);

int oci_broken_rootfs_cleaner(struct clean_ctx *ctx);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
>>>>>>>> origin/cri-1.25_support:src/daemon/modules/container/leftover_cleanup/oci_rootfs_clean.h
