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
 * Author: lijiaming
 * Create: 2023-09-07
 * Description: provide fuzz test for im_container_export
 ******************************************************************************/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "image_api.h"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    im_export_request *request = (im_export_request *)malloc(sizeof(im_export_request));

    char *data_str = (char *)malloc(size + 1);
    memset(data_str, 0, size + 1);
    memcpy(data_str, data, size);
    // replace "ubuntu" with valid id or name
    request->name_id = "ubuntu";
    request->file = data_str;
    request->type = "oci";

    int result = im_container_export(request);
    free(request);
    free(data_str);

    return 0;
}
