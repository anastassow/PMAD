/*
 * message.h
 * Defines the fixed-size Message structure used in the messaging benchmark.
 * sizeof(Message) == 264 bytes (4 + 4 + 256).
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#define PAYLOAD_SIZE 256

typedef struct {
  int topic;
  int size;
  char payload[PAYLOAD_SIZE];
} Message;

/* Prevent the compiler from optimizing away message processing. */
static inline void process_message(const Message *msg) {
  volatile int x = msg->topic * msg->size;
  (void)x;
}

#endif /* MESSAGE_H */
