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

#include "shim_v2_mock.h"

namespace {
MockShimV2 *g_shimv2_mock = nullptr;
}

void MockShimV2_SetMock(MockShimV2* mock)
{
    g_shimv2_mock = mock;
}

int shim_v2_state(const char *container_id, const struct State *state)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2State(container_id, state);
    }
    return 0;
}

int shim_v2_create(const char *container_id, const char *bundle, const bool terminal, const char *stdin, const char *stdout,
                const char *stderr, const int *pid)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Create(container_id, bundle, terminal, stdin, stdout, stderr, pid);
    }
    return 0;
}

int shim_v2_exec(const char *container_id, const char *exec_id, const bool terminal, const char *stdin, const char *stdout,
                const char *stderr, const char *spec)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Exec(container_id, exec_id, terminal, stdin, stdout, stderr, spec);
    }
    return 0;

}

int shim_v2_resize_pty(const char *container_id, const char *exec_id, unsigned int height, unsigned int width)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2ResizePty(container_id, exec_id, height, width);
    }
    return 0;
}

int shim_v2_kill(const char *container_id, const char *exec_id, const unsigned int signal, const bool all)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Kill(container_id, exec_id, signal, all);
    }
    return 0;
}

int shim_v2_new(const char *container_id, const char *addr)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2New(container_id, addr);
    }
    return 0;

}

int shim_v2_start(const char *container_id, const char *exec_id, const int *pid)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Start(container_id, exec_id, pid);
    }
    return 0;
}

int shim_v2_delete(const char *container_id, const char *exec_id, const struct DeleteResponse *resp)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Delete(container_id, exec_id, resp);
    }
    return 0;
}

int shim_v2_shutdown(const char *container_id)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Shutdown(container_id);
    }
    return 0;
}

int shim_v2_pause(const char *container_id)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Pause(container_id);
    }
    return 0;
}

int shim_v2_resume(const char *container_id)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Resume(container_id);
    }
    return 0;
}

int shim_v2_close(const char *container_id)
{
    if (g_shimv2_mock != nullptr) {
        return g_shimv2_mock->ShimV2Close(container_id);
    }
    return 0;
}
