/******************************************************************************
 * Copyright (c) China Unicom Technologies Co., Ltd. 2023-2033. All rights reserved.
 *
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 *
 * Author: Chen Wei
 * Create: 2023-08-11
 * Description: unit test for image functions.
 ******************************************************************************/

#include <fcntl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "image_cb.h"
#include "image_mock.h"
#include "error.h"
#include "path.h"

using ::testing::NiceMock;
using ::testing::Invoke;
using ::testing::_;

class ImagesUnitTest: public testing::Test {
protected:
    void SetUp() override
    {
        MockImage_SetMock(&m_image_mock);
	    image_callback_init(&image_callback);
    }

    void TearDown() override
    {
        MockImage_SetMock(nullptr);
    }

    NiceMock<MockImage> m_image_mock;
	service_image_callback_t image_callback;
};

void invokeListImagesAllTypes(const im_list_request *ctx, imagetool_images_list **images_list)
{
	*images_list = (imagetool_images_list*)util_common_calloc_s(sizeof(imagetool_images_list));
    if (*images_list == NULL) {
        std::cout << "Out of memory" << std::endl;
        return;
    }

	(*images_list)->images_len = 1;

	(*images_list)->images = (imagetool_image_summary **)util_smart_calloc_s(1, sizeof(imagetool_image_summary *));
	imagetool_image_summary *info = (imagetool_image_summary *)util_common_calloc_s(sizeof(imagetool_image_summary));
	info->id = util_strdup_s("12345678");
	info->created = util_strdup_s("2023-01-30T00:00:00");
	info->loaded = util_strdup_s("2023-01-30T00:00:00");
	info->size = 20;

	(*images_list)->images[0] = info;
}

TEST_F(ImagesUnitTest, test_image_prune_null_input)
{
	int ret = 0;

	ret = image_callback.prune(NULL, NULL);
	EXPECT_EQ(ret, -1);
}

TEST_F(ImagesUnitTest, test_image_prune_empty)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
    EXPECT_EQ(prune_response->images_len, 0);
}

TEST_F(ImagesUnitTest, test_image_prune_all)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	EXPECT_CALL(m_image_mock, ListImagesAllTypes(_, _))
    .WillRepeatedly(Invoke(invokeListImagesAllTypes));
	EXPECT_CALL(m_image_mock, ImIfImageInuse(_))
    .WillOnce(testing::Return(0));
	EXPECT_CALL(m_image_mock, DeleteImage(_, _))
	.WillOnce(testing::Return(0));

	prune_request.all = true;
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
	std::cout << prune_response << std::endl;
    EXPECT_EQ(prune_response->images_len, 1);
	EXPECT_EQ(prune_response->space_reclaimed, 20);
}

TEST_F(ImagesUnitTest, test_image_prune_dangling)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	EXPECT_CALL(m_image_mock, ListImagesAllTypes(_, _))
    .WillRepeatedly(Invoke(invokeListImagesAllTypes));
	EXPECT_CALL(m_image_mock, ImIfImageInuse(_))
    .WillOnce(testing::Return(0));

	prune_request.all = false;
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
	std::cout << prune_response << std::endl;
    EXPECT_EQ(prune_response->images_len, 1);
	EXPECT_EQ(prune_response->space_reclaimed, 20);
}

TEST_F(ImagesUnitTest, test_image_prune_inuse)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	EXPECT_CALL(m_image_mock, ListImagesAllTypes(_, _))
    .WillRepeatedly(Invoke(invokeListImagesAllTypes));
	EXPECT_CALL(m_image_mock, ImIfImageInuse(_))
    .WillOnce(testing::Return(-1));

	prune_request.all = true;
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
	std::cout << prune_response << std::endl;
    EXPECT_EQ(prune_response->images_len, 0);
	EXPECT_EQ(prune_response->space_reclaimed, 0);
}

TEST_F(ImagesUnitTest, test_image_prune_filter_before)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	EXPECT_CALL(m_image_mock, ListImagesAllTypes(_, _))
    .WillRepeatedly(Invoke(invokeListImagesAllTypes));
	EXPECT_CALL(m_image_mock, ImIfImageInuse(_))
    .WillOnce(testing::Return(0));
    EXPECT_CALL(m_image_mock, DeleteImage(_, _))
	.WillOnce(testing::Return(0));

	prune_request.all = true;
	prune_request.filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
	prune_request.filters->len = 1;
	prune_request.filters->keys = (char **)util_smart_calloc_s(1, sizeof(char *));
	prune_request.filters->values = (json_map_string_bool **)util_smart_calloc_s(1, sizeof(json_map_string_bool *));
	prune_request.filters->keys[0] = util_strdup_s("until");
	prune_request.filters->values[0] = (json_map_string_bool *)util_common_calloc_s(sizeof(json_map_string_bool));
	append_json_map_string_bool(prune_request.filters->values[0], "2022-01-30T00:00:00", true);
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
	std::cout << prune_response << std::endl;
    EXPECT_EQ(prune_response->images_len, 1);
	EXPECT_EQ(prune_response->space_reclaimed, 20);
}

TEST_F(ImagesUnitTest, test_image_prune_filter_after)
{
	int ret = 0;
	image_prune_request prune_request;
	image_prune_response *prune_response;
	memset(&prune_request, 0, sizeof(image_prune_request));
	
	EXPECT_CALL(m_image_mock, ListImagesAllTypes(_, _))
    .WillRepeatedly(Invoke(invokeListImagesAllTypes));
	EXPECT_CALL(m_image_mock, ImIfImageInuse(_))
    .WillOnce(testing::Return(0));

	prune_request.all = true;
	prune_request.filters = (defs_filters *)util_common_calloc_s(sizeof(defs_filters));
	prune_request.filters->len = 1;
	prune_request.filters->keys = (char **)util_smart_calloc_s(1, sizeof(char *));
	prune_request.filters->values = (json_map_string_bool **)util_smart_calloc_s(1, sizeof(json_map_string_bool *));
	prune_request.filters->keys[0] = util_strdup_s("until");
	prune_request.filters->values[0] = (json_map_string_bool *)util_common_calloc_s(sizeof(json_map_string_bool));
	append_json_map_string_bool(prune_request.filters->values[0], "2024-01-30T00:00:00", true);
	ret = image_callback.prune(&prune_request, &prune_response);
	EXPECT_EQ(ret, 0);
	std::cout << prune_response << std::endl;
    EXPECT_EQ(prune_response->images_len, 0);
	EXPECT_EQ(prune_response->space_reclaimed, 0);
}
