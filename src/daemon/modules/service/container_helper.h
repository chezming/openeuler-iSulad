/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2023. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: xuxuepeng
 * Create: 2023-06-08
 * Description: provide helper function for container service
 ******************************************************************************/

#include <isula_libutils/defs_process.h>
#include <isula_libutils/oci_runtime_spec.h>
#include <isula_libutils/defs_process.h>

#include "container_api.h"

int sandbox_prepare_container(const container_t *cont, const oci_runtime_spec *oci_spec,
                              const char *console_fifos[], bool tty);

int sandbox_prepare_exec(const container_t *cont, const char *exec_id, defs_process *process_spec,
                         const char *console_fifos[], bool tty);

int sandbox_purge_container(const container_t *cont);

int sandbox_purge_exec(const container_t *cont, const char *exec_id);
