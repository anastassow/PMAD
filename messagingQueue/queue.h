/*
 * queue.h
 * Single-threaded singly-linked FIFO queue.
 * Both enqueue() and dequeue() are O(1).
 *
 * Node memory is managed via the caller-supplied alloc/free callbacks,
 * so the same queue implementation works for both malloc and PMAD.
 */

#ifndef QUEUE_H
#define QUEUE_H

#include "message.h"
#include <stddef.h>

/* -------------------------------------------------------------------
 * Queue node
 * ------------------------------------------------------------------- */
typedef struct Node {
  Message *msg;
  struct Node *next;
} Node;

/* -------------------------------------------------------------------
 * Queue
 * ------------------------------------------------------------------- */
typedef struct {
  Node *head; /* dequeue end  */
  Node *tail; /* enqueue end  */
} Queue;

/* Allocator callbacks — plug in malloc or pmad_alloc */
typedef void *(*AllocFn)(size_t);
typedef void (*FreeFn)(void *);

/* -------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------- */

/* Initialise an empty queue. */
static inline void queue_init(Queue *q) {
  q->head = NULL;
  q->tail = NULL;
}

/* Enqueue msg.  Returns 0 on success, -1 on allocation failure. */
static inline int enqueue(Queue *q, Message *msg, AllocFn node_alloc) {
  Node *node = (Node *)node_alloc(sizeof(Node));
  if (!node)
    return -1;

  node->msg = msg;
  node->next = NULL;

  if (q->tail) {
    q->tail->next = node;
  } else {
    q->head = node;
  }
  q->tail = node;
  return 0;
}

/* Dequeue a message.  Caller must free the returned message.
 * Returns NULL when the queue is empty. */
static inline Message *dequeue(Queue *q, FreeFn node_free) {
  if (!q->head)
    return NULL;

  Node *node = q->head;
  Message *msg = node->msg;

  q->head = node->next;
  if (!q->head)
    q->tail = NULL;

  node_free(node);
  return msg;
}

#endif /* QUEUE_H */
