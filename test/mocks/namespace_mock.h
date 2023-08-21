/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: wujing
 * Create: 2020-02-14
 * Description: provide namespace mock
 ******************************************************************************/

#ifndef _ISULAD_TEST_MOCKS_NAMESPACE_MOCK_H
#define _ISULAD_TEST_MOCKS_NAMESPACE_MOCK_H

#include <gmock/gmock.h>
#include "namespace.h"
#include "specs_namespace.h"

class MockNamespace {
public:
    virtual ~MockNamespace() = default;
    MOCK_METHOD1(ConnectedContainer, char *(const char *mode));
    MOCK_METHOD3(GetShareNamespacePath, int(const char *type, const char *src_path, char **dest_path));
    MOCK_METHOD4(GetNetworkNamespacePath, int(const host_config *host_spec,
                                              const container_network_settings *network_settings, const char *type, char **dest_path));
    MOCK_METHOD1(GetContainerProcessLabel, char *(const char *path));
#ifdef ENABLE_CRI_API_V1
    MOCK_METHOD2(NamespaceIsSandbox, bool(const char *mode, const container_sandbox_info *sandbox_info));
#endif
    MOCK_METHOD2(FormatShareNamespacePath, char *(int pid, const char *type));
};

void MockNamespace_SetMock(MockNamespace *mock);

#endif // _ISULAD_TEST_MOCKS_NAMESPACE_MOCK_H
