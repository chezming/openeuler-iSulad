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
 * Description: provide circular queue utils functions
 *******************************************************************************/
#include "utils_queue.h"
#include "utils.h"
#include "isula_libutils/log.h"

static void free_queue_node(queue_node_t *node)
{
    node->data = NULL;
    node->next = NULL;
    free(node);
}

circular_queue_buffer *circular_queue_init()
{
    circular_queue_buffer *q = (circular_queue_buffer *)util_common_calloc_s(sizeof(circular_queue_buffer));
    if (q == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    if (pthread_mutex_init(&(q->mutex), NULL) != 0) {
        ERROR("Mutex initialization failed");
        return NULL;
    }

    q->head = q->tail = NULL;
    q->length = 0;

    return q;
}

int circular_queue_push(circular_queue_buffer *q, void *data, size_t len)
{
    queue_node_t *temp = NULL;

    if (q == NULL || data == NULL) {
        ERROR("Invalid parameter: null");
        return -1;
    }

    pthread_mutex_lock(&(q->mutex));

    temp = (queue_node_t *)util_common_calloc_s(sizeof(queue_node_t));
    if (temp == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    temp->data = data;
    temp->len = len;

    if (q->head == NULL) {
        q->head = temp;
    } else {
        q->tail->next = temp;
    }

    q->tail = temp;
    q->tail->next = q->head;
    q->length++;

    pthread_mutex_unlock(&(q->mutex));

    return 0;
}

void *circular_queue_back(circular_queue_buffer *q, size_t *len)
{
    void *value = NULL;

    if (q == NULL || q->head == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&(q->mutex));
    value = q->head->data;
    *len = q->head->len;
    pthread_mutex_unlock(&(q->mutex));

    return value;
}

int circular_queue_pop(circular_queue_buffer *q)
{
    if (q == NULL || q->head == NULL) {
        ERROR("Empty Circular Queue");
        return -1;
    }

    pthread_mutex_lock(&(q->mutex));
    if (q->head == q->tail) {
        free_queue_node(q->head);
        q->head = NULL;
        q->tail = NULL;
        q->length = 0;
    } else {
        queue_node_t *temp = q->head;
        q->head = q->head->next;
        q->tail->next = q->head;
        free_queue_node(temp);
        temp = NULL;
        q->length--;
    }

    pthread_mutex_unlock(&(q->mutex));

    return 0;
}

size_t circular_queue_length(circular_queue_buffer *q)
{
    size_t length = 0;

    pthread_mutex_lock(&(q->mutex));
    length = q->length;
    pthread_mutex_unlock(&(q->mutex));

    return length;
}

void circular_queue_destroy(circular_queue_buffer *q)
{
    char *value = NULL;
    size_t len = 0;

    if (q == NULL) {
        return;
    }

    while ((value = circular_queue_back(q, &len)) != NULL) {
        free(value);
        value = NULL;
        circular_queue_pop(q);
    }
    (void)pthread_mutex_destroy(&(q->mutex));

    free(q);
}