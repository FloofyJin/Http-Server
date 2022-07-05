#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct Node Node;

typedef struct Queue Queue;

Queue *queue_create();

void queue_delete(Queue **q);

bool queue_empty(Queue *q);

bool queue_full(Queue *q);

uint32_t queue_size(Queue *q);

bool enqueue(Queue *q, int x);

int dequeue(Queue *q);

void queue_print(Queue *q);

#endif
