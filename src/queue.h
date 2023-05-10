#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct Queue {
    char* data;
    size_t front;
    size_t max_items;
    size_t num_items;
    size_t item_size;
} Queue;

void queue_init(Queue * const q, const size_t item_size);
void queue_destroy(Queue * const q);
void queue_push_back(Queue * const q, const void * const data);
void* queue_front(const Queue * const q);
void queue_pop_front(Queue * const q);
bool queue_empty(const Queue * const q);
