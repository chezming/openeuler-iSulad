/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v2 for more details.
 * Author: WuJing
 * Create: 2020-10-07
 * Description: provide circular queue functions
 ********************************************************************************/

#ifndef UTILS_CUTILS_UTILS_QUEUE_H
#define UTILS_CUTILS_UTILS_QUEUE_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct queue_node {
    void * volatile data;
    size_t len;
    struct queue_node * volatile next;
} queue_node_t;

typedef struct circular_queue {
    queue_node_t * volatile head;
    queue_node_t * volatile tail;
    volatile size_t length;
    pthread_mutex_t mutex;
} circular_queue_buffer;

circular_queue_buffer *circular_queue_init();
int circular_queue_push(circular_queue_buffer *q, void *data, size_t le);
void *circular_queue_back(circular_queue_buffer *q, size_t *len);
int circular_queue_pop(circular_queue_buffer *q);
size_t circular_queue_length(circular_queue_buffer *q);
void circular_queue_destroy(circular_queue_buffer *q);

#ifdef __cplusplus
}
#endif

#endif // UTILS_CUTILS_UTILS_STRING_H

