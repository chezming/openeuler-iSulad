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
 * Description: provide fuzz test for im_pull_image
 ******************************************************************************/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "image_api.h"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    im_pull_request *request = (im_pull_request *)malloc(sizeof(im_pull_request));
    memset(request, 0, sizeof(im_pull_request));
    im_pull_response *response = NULL;

    char *data_str = (char *)malloc(size + 1);
    memset(data_str, 0, size + 1);
    memcpy(data_str, data, size);

    request->image = data_str;
    char *ty = "oci";
    request->type = ty;
    int result = im_pull_image(request, &response);
    free(request);
    free(data_str);
    if (response == NULL)
        return 0;
    if (response->image_ref)
        free(response->image_ref);
    if (response->errmsg)
        free(response->errmsg);
    free(response);
    return 0;
}
