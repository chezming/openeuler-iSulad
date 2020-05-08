/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
* iSulad licensed under the Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*     http://license.coscl.org.cn/MulanPSL2
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
* PURPOSE.
* See the Mulan PSL v2 for more details.
* Author: jikui
* Create: 2020-05-08
* Description: isula tag operator implement
*******************************************************************************/
#include "oci_tag.h"
#include "storage.h"

int oci_do_tag(const char *src_name, const char *dest_name, char **errmsg)
{
    if (src_name == NULL || dest_name == NULL || errmsg == NULL) {
        return -1;
    }

    if (storage_img_add_name(src_name, dest_name) != 0) {
        *errmsg = "add name failed";
        return -1;
    }
    return 0; 
}
