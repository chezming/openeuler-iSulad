/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020-2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wangfengtu
 * Create: 2020-06-30
 * Description: provide oci registry images unit test
 ******************************************************************************/
#include <cstring>
#include <iostream>
#include <algorithm>
#include <tuple>
#include <fstream>
#include <string>
#include <fstream>
#include <streambuf>
#include <climits>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "utils.h"
#include "utils_array.h"
#include "path.h"
#include "isula_libutils/imagetool_images_list.h"
#include "isula_libutils/imagetool_image.h"
#include "isula_libutils/log.h"
#include "http_request.h"
#include "registry.h"
#include "registry_type.h"
#include "http_mock.h"
#include "storage_mock.h"
#include "buffer.h"

using ::testing::Args;
using ::testing::ByRef;
using ::testing::SetArgPointee;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::NotNull;
using ::testing::AtLeast;
using ::testing::Invoke;

std::string get_dir()
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

    return static_cast<std::string>(abs_path) +  "../../../../../test/image/oci/registry";
    return static_cast<std::string>(abs_path);
}

class PullUnitTest : public testing::Test {
protected:
    void SetUp() override
    {
        MockHttp_SetMock(&m_http_mock);
        MockStorage_SetMock(&m_storage_mock);
    }

    void TearDown() override
    {
        MockHttp_SetMock(nullptr);
        MockStorage_SetMock(nullptr);
    }

    NiceMock<MockHttp> m_http_mock;
    NiceMock<MockStorage> m_storage_mock;
};

int invokeHttpRequestBuf(pull_descriptor *desc, const char *url, const char **custom_headers, char **output,
                     	    resp_data_type type)
{
    std::string file;

    std::string data_path = get_dir() + "/data/";
    if (!strcmp(url, "https://quay.io/v2/")) {
	file = data_path + "v1_ping_head";
    } else if (!strcmp(url, "https://quay.io/v2/coreos/etcd/manifests/v3.3.17-arm64")) {
	file = data_path + "v1_manifest_head";
    } else {
	ERROR("%s not match failed", url);
	return -1;
    }

    *output = util_read_text_file(file.c_str());
    if (*output == NULL) {
	ERROR("read file %s failed", file.c_str());
	return -1;
    }

    return 0;
}

int invokeHttpRequestV1(const char *url, struct http_get_options *options, long *response_code, int recursive_len)
{
    std::string file;
    char *data = NULL;
    Buffer *output_buffer = (Buffer *)options->output;

    std::string data_path = get_dir() + "/data/v1/";
    if (!strcmp(url, "https://quay.io/v2/")) {
	file = data_path + "ping_head";
    } else if (!strcmp(url, "https://quay.io/v2/coreos/etcd/manifests/v3.3.17-arm64")) {
	file = data_path + "manifest_head";
    } else if (util_has_prefix(url, "https://quay.io/v2/auth")) {
	file = data_path + "token_body";
    } else if (util_has_prefix(url, "https://quay.io/v2/coreos/etcd/manifests/sha256")) {
	file = data_path + "manifest_body";
    } else if (util_has_prefix(url, "https://quay.io/v2/coreos/etcd/blobs/sha256")) {
	file = std::string("");
    } else {
	ERROR("%s not match failed", url);
	return -1;
    }

    if (file == std::string("")) {
	data = util_strdup_s("test");
    } else {
    	data = util_read_text_file(file.c_str());
	if (data == NULL) {
	    ERROR("read file %s failed", file.c_str());
	    return -1;
	}
    }
    if (options->outputtype == HTTP_REQUEST_STRBUF) {
	output_buffer->contents = util_strdup_s(data);
    } else {
    	if (util_write_file((const char *)options->output, data, strlen(data), 0600) != 0) {
        	free(data);
		ERROR("write file %s failed", (char *)options->output);
		return -1;
    	}
    }
    free(data);

    return 0;
}

int invokeHttpRequestV2(const char *url, struct http_get_options *options, long *response_code, int recursive_len)
{
    std::string file;
    char *data = NULL;
    int64_t size = 0;
    Buffer *output_buffer = (Buffer *)options->output;

    // Test insecure registry, assume registry cann't support https.
    if (util_has_prefix(url, "https://")) {
	return -1;
    }

    std::string data_path = get_dir() + "/data/v2/";
    if (!strcmp(url, "http://hub-mirror.c.163.com/v2/")) {
	file = data_path + "ping_head";
    } else if (!strcmp(url, "http://hub-mirror.c.163.com/v2/library/busybox/manifests/latest")) {
	file = data_path + "manifest_head";
    } else if (util_has_prefix(url, "http://hub-mirror.c.163.com/v2/library/busybox/manifests/sha256:9ddee63a")) {
	file = data_path + "manifest_list";
    } else if (util_has_prefix(url, "http://hub-mirror.c.163.com/v2/library/busybox/manifests/sha256:2131f09e")) {
	file = data_path + "manifest_body";
    } else if (util_has_prefix(url, "http://hub-mirror.c.163.com/v2/library/busybox/blobs/sha256:c7c37e47")) {
	file = data_path + "config";
    } else if (util_has_prefix(url, "http://hub-mirror.c.163.com/v2/library/busybox/blobs/sha256:91f30d77")) {
	file = data_path + "0";
    } else {
	ERROR("%s not match failed", url);
	return -1;
    }

    size = util_file_size(file.c_str());
    if (size < 0) {
	ERROR("get file %s size failed", file.c_str());
	return -1;
    }

    data = util_read_text_file(file.c_str());
    if (data == NULL) {
	ERROR("read file %s failed", file.c_str());
	return -1;
    }

    if (options->outputtype == HTTP_REQUEST_STRBUF) {
	output_buffer->contents = util_strdup_s(data);
    } else {
    	if (util_write_file((const char *)options->output, data, size, 0600) != 0) {
        	free(data);
		ERROR("write file %s failed", (char *)options->output);
		return -1;
    	}
    }
    free(data);

    return 0;
}

int invokeHttpRequestOCI(const char *url, struct http_get_options *options, long *response_code, int recursive_len)
{
    std::string file;
    char *data = NULL;
    int64_t size = 0;
    Buffer *output_buffer = (Buffer *)options->output;

    std::string data_path = get_dir() + "/data/oci/";
    if (!strcmp(url, "https://hub-mirror.c.163.com/v2/")) {
	file = data_path + "ping_head";
    } else if (!strcmp(url, "https://hub-mirror.c.163.com/v2/library/busybox/manifests/latest")) {
	file = data_path + "manifest_head";
    } else if (util_has_prefix(url, "https://hub-mirror.c.163.com/v2/library/busybox/manifests/sha256:bd28e852")) {
	file = data_path + "index";
    } else if (util_has_prefix(url, "https://hub-mirror.c.163.com/v2/library/busybox/manifests/sha256:106429d7")) {
	file = data_path + "manifest_body";
    } else if (util_has_prefix(url, "https://hub-mirror.c.163.com/v2/library/busybox/blobs/sha256:c7c37e47")) {
	file = data_path + "config";
    } else if (util_has_prefix(url, "https://hub-mirror.c.163.com/v2/library/busybox/blobs/sha256:91f30d77")) {
	file = data_path + "0";
    } else {
	ERROR("%s not match failed", url);
	return -1;
    }

    size = util_file_size(file.c_str());
    if (size < 0) {
	ERROR("get file %s size failed", file.c_str());
	return -1;
    }

    data = util_read_text_file(file.c_str());
    if (data == NULL) {
	ERROR("read file %s failed", file.c_str());
	return -1;
    }

    if (options->outputtype == HTTP_REQUEST_STRBUF) {
	output_buffer->contents = util_strdup_s(data);
    } else {
    	if (util_write_file((const char *)options->output, data, size, 0600) != 0) {
        	free(data);
		ERROR("write file %s failed", (char *)options->output);
		return -1;
    	}
    }
    free(data);

    return 0;
}

int invokeStorageImgCreate(const char *id, const char *parent_id, const char *metadata, struct storage_img_create_options *opts)
{
    return 0;
}

imagetool_image * invokeStorageImgGet(const char *img_id)
{
    return NULL;
}

int invokeStorageImgSetBigData(const char *img_id, const char *key, const char *val)
{
    return 0;
}

int invokeStorageImgAddName(const char *img_id, const char *img_name)
{
    return 0;
}

int invokeStorageImgDelete(const char *img_id, bool commit)
{
    return 0;
}

int invokeStorageImgSetLoadedTime(const char *img_id, types_timestamp_t *loaded_time)
{
    return 0;
}

int invokeStorageImgSetImageSize(const char *image_id)
{
    return 0;
}

char * invokeStorageGetImgTopLayer(const char *id)
{
    return NULL;
}

int invokeStorageLayerCreate(const char *layer_id, storage_layer_create_opts_t *opts)
{
    return 0;
}

struct layer * invokeStorageLayerGet(const char *layer_id)
{
    return NULL;
}

int invokeStorageLayerTryRepairLowers(const char *layer_id, const char *last_layer_id)
{
    return 0;
}

static int init_log()
{
    struct isula_libutils_log_config lconf = { 0 };

    lconf.name = "registry_unit_test";
    lconf.file = NULL;
    lconf.priority = "ERROR";
    lconf.driver = "stdout";
    if (isula_libutils_log_enable(&lconf)) {
        fprintf(stderr, "log init failed\n");
        return -1;
    }

    return 0;
}

void mockStorageAll(MockStorage *mock)
{
    EXPECT_CALL(*mock, StorageImgCreate(::testing::_,::testing::_,::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgCreate));
    EXPECT_CALL(*mock, StorageImgGet(::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgGet));
    EXPECT_CALL(*mock, StorageImgSetBigData(::testing::_,::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgSetBigData));
    EXPECT_CALL(*mock, StorageImgAddName(::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgAddName));
    EXPECT_CALL(*mock, StorageImgDelete(::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgDelete));
    EXPECT_CALL(*mock, StorageImgSetLoadedTime(::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgSetLoadedTime));
    EXPECT_CALL(*mock, StorageImgSetImageSize(::testing::_))
    .WillRepeatedly(Invoke(invokeStorageImgSetImageSize));
    EXPECT_CALL(*mock, StorageGetImgTopLayer(::testing::_))
    .WillRepeatedly(Invoke(invokeStorageGetImgTopLayer));
    EXPECT_CALL(*mock, StorageLayerCreate(::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageLayerCreate));
    EXPECT_CALL(*mock, StorageLayerGet(::testing::_))
    .WillRepeatedly(Invoke(invokeStorageLayerGet));
    EXPECT_CALL(*mock, StorageLayerTryRepairLowers(::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeStorageLayerTryRepairLowers));
    return;
}

TEST_F(PullUnitTest, test_pull_v1_image)
{
    registry_pull_options options;
    options.image_name = (char*)"quay.io/coreos/etcd:v3.3.17-arm64";
    options.dest_image_name = (char*)"quay.io/coreos/etcd:v3.3.17-arm64";
    options.auth.username = (char*)"test";
    options.auth.password = (char*)"test";
    options.skip_tls_verify = (char*)false;
    options.insecure_registry = (char*)false;

    ASSERT_EQ(init_log(), 0);
    ASSERT_EQ(registry_init(), 0);

    EXPECT_CALL(m_http_mock, HttpRequest(::testing::_,::testing::_,::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeHttpRequestV1));

    mockStorageAll(&m_storage_mock);

    ASSERT_EQ(registry_pull(&options), 0);
}

TEST_F(PullUnitTest, test_pull_v2_image)
{
    registry_pull_options options;
    options.image_name = (char*)"hub-mirror.c.163.com/library/busybox:latest";
    options.dest_image_name = (char*)"docker.io/library/busybox:latest";
    options.auth.username = (char*)"test";
    options.auth.password = (char*)"test";
    options.skip_tls_verify = (char*)true;
    options.insecure_registry = (char*)true;

    EXPECT_CALL(m_http_mock, HttpRequest(::testing::_,::testing::_,::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeHttpRequestV2));

    mockStorageAll(&m_storage_mock);

    ASSERT_EQ(registry_pull(&options), 0);
}

TEST_F(PullUnitTest, test_pull_oci_image)
{
    registry_pull_options options;
    options.image_name = (char*)"hub-mirror.c.163.com/library/busybox:latest";
    options.dest_image_name = (char*)"docker.io/library/busybox:latest";
    options.auth.username = NULL;
    options.auth.password = NULL;
    options.skip_tls_verify = (char*)false;
    options.insecure_registry = (char*)false;

    EXPECT_CALL(m_http_mock, HttpRequest(::testing::_,::testing::_,::testing::_,::testing::_))
    .WillRepeatedly(Invoke(invokeHttpRequestOCI));

    mockStorageAll(&m_storage_mock);

    ASSERT_EQ(registry_pull(&options), 0);
}
