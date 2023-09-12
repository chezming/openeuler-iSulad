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
 * Description: provide fuzz test for im_import_image
 ******************************************************************************/

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "image_api.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < sizeof(im_import_request))
        return 0;

    std::string testData(reinterpret_cast<const char *>(data), size);
    std::string fileName("temp.txt");

    std::ofstream outFile(fileName);
    outFile << testData;
    outFile.close();

    im_import_request *request = (im_import_request *)malloc(sizeof(im_import_request));
    ;
    char *id = NULL;

    request->file = "temp.txt";
    char *data_str = (char *)malloc(size + 1);
    memset(data_str, 0, size + 1);
    memcpy(data_str, data, size);

    request->tag = data_str;

    int result = im_import_image(request, &id);
    free(request);
    free(data_str);

    return 0;
}
