/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: wujing
 * Create: 2020-03-30
 * Description: provide oci storage images unit test
 ******************************************************************************/

#include "image_store.h"
#include <iostream>
#include <algorithm>
#include <tuple>
#include <fstream>
#include <climits>
#include <gtest/gtest.h>
#include "path.h"

std::string GetDirectory()
{
    char abs_path[PATH_MAX];
    int ret = readlink("/proc/self/exe", abs_path, sizeof(abs_path));
    if (ret < 0 || (size_t)ret >= sizeof(abs_path)) {
        return "";
    }

    for (int i { ret }; i >= 0; --i) {
        if (abs_path[i] == '/') {
            abs_path[i + 1] = '\0';
            break;
        }
    }

    return static_cast<std::string>(abs_path);
}

/********************************test data 1: image.json**************************************
  {
  "id": "39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10",
  "digest": "sha256:94192fe835d92cba5513297aad1cbcb32c9af455fb575e926ee5ec683a95e586",
  "names": [
  "rnd-dockerhub.huawei.com/official/centos:latest"
  ],
  "layer": "edd34c086208711c693a7b7a3ade23e24e6170ae24d8d2dab7c4f3efca61d509",
  "metadata": "{}",
  "big-data-names": [
  "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10",
  "manifest"
  ],
  "big-data-sizes": {
  "manifest": 741,
  "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10": 2235
  },
  "big-data-digests": {
  "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10": "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10",
  "manifest": "sha256:94192fe835d92cba5513297aad1cbcb32c9af455fb575e926ee5ec683a95e586"
  },
  "created": "2017-07-10T12:46:57.770791248Z",
  "Loaded": "2020-03-16T03:46:12.172621513Z"
  }
 ******************************************************************************************/

/********************************test data 2: image.json**************************************
  {
  "id": "e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b",
  "digest": "sha256:64da743694ece2ca88df34bf4c5378fdfc44a1a5b50478722e2ff98b82e4a5c9",
  "names": [
  "rnd-dockerhub.huawei.com/official/busybox:latest"
  ],
  "layer": "6194458b07fcf01f1483d96cd6c34302ffff7f382bb151a6d023c4e80ba3050a",
  "metadata": "{}",
  "big-data-names": [
  "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b",
  "manifest"
  ],
  "big-data-sizes": {
  "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b": 1497,
  "manifest": 527
  },
  "big-data-digests": {
  "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b": "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b",
  "manifest": "sha256:64da743694ece2ca88df34bf4c5378fdfc44a1a5b50478722e2ff98b82e4a5c9"
  },
  "created": "2019-06-15T00:19:54.402459069Z",
  "Loaded": "2020-03-16T03:46:17.439778957Z"
  }
 ******************************************************************************************/

class StorageImagesUnitTest : public testing::Test {
protected:
    void SetUp() override
    {
        std::string dir = GetDirectory() + "/data";

        ASSERT_STRNE(cleanpath(dir.c_str(), real_path, sizeof(real_path)), nullptr);
        ASSERT_EQ(new_image_store(real_path), 0);
    }
    void TearDown() override
    {
    }
    std::vector<std::string> ids {
        "39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10",
        "e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b"
    };
    char real_path[PATH_MAX] = {0x00};
};

TEST_F(StorageImagesUnitTest, test_images_load)
{
    auto image = image_store_get_image(ids.at(0).c_str());
    ASSERT_NE(image, nullptr);
    ASSERT_STREQ(image->digest, "sha256:94192fe835d92cba5513297aad1cbcb32c9af455fb575e926ee5ec683a95e586");
    ASSERT_EQ(image->names_len, 1);
    ASSERT_STREQ(image->names[0], "rnd-dockerhub.huawei.com/official/centos:latest");
    ASSERT_STREQ(image->layer, "edd34c086208711c693a7b7a3ade23e24e6170ae24d8d2dab7c4f3efca61d509");
    ASSERT_STREQ(image->metadata, "{}");
    ASSERT_EQ(image->big_data_names_len, 2);
    ASSERT_STREQ(image->big_data_names[0], "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10");
    ASSERT_STREQ(image->big_data_names[1], "manifest");
    ASSERT_EQ(image->big_data_sizes->len, 2);
    ASSERT_STREQ(image->big_data_sizes->keys[0], "manifest");
    ASSERT_EQ(image->big_data_sizes->values[0], 741);
    ASSERT_STREQ(image->big_data_sizes->keys[1], "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10");
    ASSERT_EQ(image->big_data_sizes->values[1], 2235);
    ASSERT_EQ(image->big_data_digests->len, 2);
    ASSERT_STREQ(image->big_data_digests->keys[0],
                 "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10");
    ASSERT_STREQ(image->big_data_digests->values[0],
                 "sha256:39891ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10");
    ASSERT_STREQ(image->big_data_digests->keys[1], "manifest");
    ASSERT_STREQ(image->big_data_digests->values[1],
                 "sha256:94192fe835d92cba5513297aad1cbcb32c9af455fb575e926ee5ec683a95e586");
    ASSERT_STREQ(image->created, "2017-07-10T12:46:57.770791248Z");
    ASSERT_STREQ(image->loaded, "2020-03-16T03:46:12.172621513Z");

    image = image_store_get_image(ids.at(1).c_str());
    ASSERT_NE(image, nullptr);
    ASSERT_STREQ(image->digest, "sha256:64da743694ece2ca88df34bf4c5378fdfc44a1a5b50478722e2ff98b82e4a5c9");
    ASSERT_EQ(image->names_len, 1);
    ASSERT_STREQ(image->names[0], "rnd-dockerhub.huawei.com/official/busybox:latest");
    ASSERT_STREQ(image->layer, "6194458b07fcf01f1483d96cd6c34302ffff7f382bb151a6d023c4e80ba3050a");
    ASSERT_STREQ(image->metadata, "{}");
    ASSERT_EQ(image->big_data_names_len, 2);
    ASSERT_STREQ(image->big_data_names[0], "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b");
    ASSERT_STREQ(image->big_data_names[1], "manifest");
    ASSERT_EQ(image->big_data_sizes->len, 2);
    ASSERT_STREQ(image->big_data_sizes->keys[0], "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b");
    ASSERT_EQ(image->big_data_sizes->values[0], 1497);
    ASSERT_STREQ(image->big_data_sizes->keys[1], "manifest");
    ASSERT_EQ(image->big_data_sizes->values[1], 527);
    ASSERT_EQ(image->big_data_digests->len, 2);
    ASSERT_STREQ(image->big_data_digests->keys[0],
                 "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b");
    ASSERT_STREQ(image->big_data_digests->values[0],
                 "sha256:e4db68de4ff27c2adfea0c54bbb73a61a42f5b667c326de4d7d5b19ab71c6a3b");
    ASSERT_STREQ(image->big_data_digests->keys[1], "manifest");
    ASSERT_STREQ(image->big_data_digests->values[1],
                 "sha256:64da743694ece2ca88df34bf4c5378fdfc44a1a5b50478722e2ff98b82e4a5c9");
    ASSERT_STREQ(image->created, "2019-06-15T00:19:54.402459069Z");
    ASSERT_STREQ(image->loaded, "2020-03-16T03:46:17.439778957Z");
}

/*TEST_F(StorageImagesUnitTest, test_images_create)
{
    std::string id {"50551ff67da98ab8540d71320915f33d2eb80ab42908e398472cab3c1ce7ac10"};
    const char *names[2] = {"hello_world:latest", "euleros:3.1"};
    std::string layer {"9994458b07fcf01f1483d96cd6c34302ffff7f382bb151a6d023c4e80ba3050a"};
    std::string metadata {"{}"};
    types_timestamp_t time {0x00};
    std::string searchableDigest {"manifest"};
    auto image = create(image_store, id.c_str(), names, sizeof(names)/sizeof(names[0]),
            layer.c_str(), metadata.c_str(), &time, searchableDigest.c_str());
    ASSERT_NE(image, nullptr);
}*/
