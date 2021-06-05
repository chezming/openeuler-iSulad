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
 * Author: gaohuatao
 * Create: 2021-06-05
 * Description: provide shim v2 mock
 ******************************************************************************/

#ifndef _ISULAD_TEST_MOCKS_SHIMV2_MOCK_H
#define _ISULAD_TEST_MOCKS_SHIMV2_MOCK_H

#include <gmock/gmock.h>
#include "shim_v2.h"

class MockShimV2 {
public:
    virtual ~MockShimV2() = default;
    MOCK_METHOD7(ShimV2Create, int (const char *container_id, const char *bundle, const bool terminal, const char *stdin, const char *stdou, 
                const char *stderr, const int *pid));
    MOCK_METHOD7(ShimV2Exec, int (const char *container_id, const char *exec_id, const bool terminal, const char *stdin, const char *stdout, 
                const char *stderr, const char *spec));
    MOCK_METHOD3(ShimV2Start, int (const char *container_id, const char *exec_id, const int *pid));
    MOCK_METHOD2(ShimV2State, int (const char *container_id, const struct State *state));
    MOCK_METHOD2(ShimV2New, int (const char *container_id, const char *addr));
    MOCK_METHOD3(ShimV2Delete, int (const char *container_id, const char *exec_id, const struct DeleteResponse *resp));
    MOCK_METHOD1(ShimV2Pause, int (const char *container_id));
    MOCK_METHOD1(ShimV2Resume, int (const char *container_id));
    MOCK_METHOD1(ShimV2Close, int (const char *container_id));
    MOCK_METHOD1(ShimV2Shutdown, int (const char *container_id));
    MOCK_METHOD4(ShimV2Kill, int (const char *container_id, const char *exec_id, const unsigned int signal, const bool all));
    MOCK_METHOD4(ShimV2ResizePty, int (const char *container_id, const char *exec_id, unsigned int height, unsigned int width));
};

void MockShimV2_SetMock(MockShimV2* mock);

#endif // _ISULAD_TEST_MOCKS_SHIMV2_MOCK_H
