#include "queue.h"

#include "mem.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SCALING_FACTOR 2
#define IN_BYTES(q, x) ((x) * q->item_size)

void queue_init(Queue * const q, const size_t item_size) {
    q->data = (char*)checked_malloc(1 * item_size);
    q->front = q->num_items = 0;
    q->max_items = 1;
    q->item_size = item_size;
}

void queue_destroy(Queue * const q) {
    free(q->data);
    memset(q, 0, sizeof(Queue));
}

void queue_push_back(Queue * const q, const void * const data) {
    if (q->num_items == q->max_items) {
        size_t old_max_items = q->max_items;
        q->max_items *= SCALING_FACTOR;
        q->data = (char*)checked_realloc(q->data, IN_BYTES(q, q->max_items));
        char* data_start = q->data + IN_BYTES(q, q->front);
        size_t added_space = q->max_items - old_max_items;
        if (q->front + q->num_items > old_max_items) {
            memmove(data_start + IN_BYTES(q, added_space), data_start, IN_BYTES(q, old_max_items - q->front));
            q->front += added_space;
        }
    }

    char* data_end = q->data + IN_BYTES(q, (q->front + q->num_items) % q->max_items);
    memcpy(data_end, data, q->item_size);
    q->num_items++;
}

void* queue_front(const Queue * const q) {
    return q->data + IN_BYTES(q, q->front);
}

void queue_pop_front(Queue * const q) {
    assert(q->num_items != 0);
    q->num_items--;
    q->front = (q->front + 1) % q->max_items;
}

bool queue_empty(const Queue * const q) {
    return q->num_items == 0;
}
