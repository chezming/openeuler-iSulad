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
 * Description: provide fuzz test for im_login
 ******************************************************************************/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "image_api.h"
#include "utils.h"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < sizeof(im_login_request))
        return 0;
    im_login_request *request = (im_login_request *)malloc(sizeof(im_login_request));
    im_login_response *response = NULL;

    char *ty;
    switch (data[0] % 4) {
        case 0:
            ty = "oci";
            break;
        case 1:
            ty = "embedded";
            break;
        case 2:
            ty = "external";
            break;
        default:
            ty = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                 "AAAAAAAAAAAAAAAAAAAAAAAAAA";
            break;
    }

    request->type = ty;
    char *data_str = (char *)malloc(size + 1);
    memset(data_str, 0, size + 1);
    memcpy(data_str, data, size);

    request->username = data_str;
    request->password = data_str;
    request->server = data_str;

    int result = im_login(request, &response);

    free(request);
    free(data_str);
    if (response == NULL)
        return 0;
    if (response->errmsg)
        free(response->errmsg);
    free(response);
    return 0;
}
