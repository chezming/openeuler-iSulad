/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Description: utils_queue unit test
 * Author: WuJing
 * Create: 2020-11-11
 */
#include <iostream>
#include <thread>
#include <stdlib.h>
#include <stdio.h>
#include <gtest/gtest.h>
#include "utils_queue.h"

TEST(utils_queue, test_circular_queue_order)
{
    circular_queue_buffer *queue = circular_queue_init();
    ASSERT_NE(queue, nullptr);

    for (int i {}; i < 10000; i++) {
        char str[21] { 0x00 };
        sprintf(str, "%d", i);
        circular_queue_push(queue, strdup(str), strlen(str));
    }

    for (int i {}; i < 10000; i++) {
        ASSERT_EQ(circular_queue_length(queue), 10000 - i);
        size_t len {};
        char *data = (char *)circular_queue_back(queue, &len);
        circular_queue_pop(queue);
        char str[21] { 0x00 };
        sprintf(str, "%d", i);
        ASSERT_STREQ(data, str);
        free(data);
        data = nullptr;
    }

    ASSERT_EQ(circular_queue_length(queue), 0);
    circular_queue_destroy(queue);
    queue = nullptr;
}


TEST(utils_queue, test_circular_queue_cross)
{
    circular_queue_buffer *queue = circular_queue_init();
    ASSERT_NE(queue, nullptr);

    for (int i {}; i < 10000; i++) {
        char str[21] { 0x00 };
        sprintf(str, "%d", i);
        circular_queue_push(queue, strdup(str), strlen(str));
        ASSERT_EQ(circular_queue_length(queue), 1);

        size_t len {};
        char *data = (char *)circular_queue_back(queue, &len);
        circular_queue_pop(queue);
        ASSERT_STREQ(data, str);
        ASSERT_EQ(circular_queue_length(queue), 0);
        free(data);
        data = nullptr;
    }

    ASSERT_EQ(circular_queue_pop(queue), -1);
    circular_queue_destroy(queue);
    queue = nullptr;
}

TEST(utils_queue, test_circular_queue_multithreading)
{
    circular_queue_buffer *queue = circular_queue_init();
    ASSERT_NE(queue, nullptr);

	std::function<void(circular_queue_buffer *queue, size_t start, size_t end)> push_range_to_queue =
		 [](circular_queue_buffer *queue, int start, int end) { 
			 for (int i {start}; i < end; i++) {
				char str[100] { 0x00 };
				sprintf(str, "%d", i);
				circular_queue_push(queue, strdup(str), strlen(str) + 1);
			} };

    std::thread pushThreadObj1([&] {
		push_range_to_queue(queue, 0, 10000);
    });

    std::thread pushThreadObj2([&] {
		push_range_to_queue(queue, 10000, 20000);
    });

    std::thread pushThreadObj3([&] {
		push_range_to_queue(queue, 20000, 30000);
    });

    pushThreadObj1.join();
    pushThreadObj2.join();
    pushThreadObj3.join();

    ASSERT_EQ(circular_queue_length(queue), 30000);

    /*std::vector<std::thread> vecOfThreads;
    std::function<void()> func = [&]() {
                     for (int i {}; i < 10000; i++) {
                            size_t len {};
                            void *data = circular_queue_back(queue, &len);
                            free(data);
                            data = nullptr;
                            circular_queue_pop(queue);
                        }
                    };
    vecOfThreads.push_back(std::thread(func));
    vecOfThreads.push_back(std::thread(func));
    vecOfThreads.push_back(std::thread(func));
    for (std::thread & th : vecOfThreads)
    {
        if (th.joinable())
            th.join();
    }*/

    for (size_t i {}; i < 30000; i++) {
        size_t len {};
        char *data = (char *)circular_queue_back(queue, &len);
        circular_queue_pop(queue);
        free(data);
        data = nullptr;
    }

    ASSERT_EQ(circular_queue_length(queue), 0);
    circular_queue_destroy(queue);
    queue = nullptr;
}
