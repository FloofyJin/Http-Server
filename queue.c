#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>

struct Node {
    int socket;
    Node *next;
};

struct Queue {
    Node *head; //end of queue
    Node *tail; //beginning of queue
    uint32_t size;
    uint32_t capacity;
    pthread_mutex_t mutex;
};

Queue *queue_create() { //create queue
    Queue *q = (Queue *) malloc(sizeof(Queue));
    if (q) {
        q->head = NULL;
        q->tail = NULL;
        q->size = 0;
        q->capacity = 4096; //bounded queue of size 4096
        pthread_mutex_t pmutex;
        pthread_mutex_init(&pmutex, NULL);
        q->mutex = pmutex;
    }
    return q;
}

Node *new_Node(int socket) {
    Node *N = malloc(sizeof(Node));
    N->socket = socket;
    N->next = NULL;
    return N;
}

void freeNode(Node **pN) {
    if (pN != NULL && *pN != NULL) {
        free(*pN);
        *pN = NULL;
    }
}

void queue_delete(Queue **q) { //free memory space
    if (q != NULL && *q != NULL) {
        Node *tmp;
        while ((*q)->head != NULL) {
            tmp = (*q)->head;
            (*q)->head = (*q)->head->next;
            freeNode(&tmp);
        }
        free(*q);
        *q = NULL;
    }
    return;
}

bool queue_empty(Queue *q) { //is queue empty?
    if (q && queue_size(q) == 0) {
        return true;
    }
    return false;
}

bool queue_full(Queue *q) { //is qeuue full?
    if (queue_size(q) >= q->capacity) {
        return true;
    }
    return false;
}

uint32_t queue_size(Queue *q) { //current length of queue
    pthread_mutex_lock(&(q->mutex));
    int x = q->size;
    pthread_mutex_unlock(&(q->mutex));
    return x;
}

bool enqueue(Queue *q, int x) { //add item to queue
    if (q && !queue_full(q)) { //is not full
        pthread_mutex_lock(&q->mutex);
        Node *newNode = new_Node(x);
        newNode->socket = x; //add to queue
        newNode->next = NULL;
        if (q->tail == NULL) {
            q->head = newNode;
        } else {
            q->tail->next = newNode;
        }
        q->tail = newNode;
        q->size++;
        pthread_mutex_unlock(&q->mutex);
        return true;
    }
    return false;
}

int dequeue(Queue *q) { //remove item from queue
    int x = -1;
    if (q && !queue_empty(q)) { //is not empty
        pthread_mutex_lock(&q->mutex);
        x = q->head->socket; //get first item in queue
        Node *tmp = q->head;
        q->head = q->head->next; //increment queue
        if (q->head == NULL) {
            q->tail = NULL;
        }
        freeNode(&tmp);
        q->size--;
        pthread_mutex_unlock(&q->mutex);
    }
    return x;
}

void queue_print(Queue *q) { //debug
    Node *tmp = q->head;
    fprintf(stderr, "{");
    while (tmp != NULL) {
        fprintf(stderr, "%d ", tmp->socket);
        tmp = tmp->next;
    }
    fprintf(stderr, "}\n");
}
