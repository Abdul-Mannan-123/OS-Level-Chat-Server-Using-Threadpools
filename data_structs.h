#ifndef DATA_STRUCTS_H
#define DATA_STRUCTS_H

#include <stdio.h>
#include <string.h>

#define QUEUE_SIZE 50
#define BUFFER_SIZE 200

struct messageQueue
{
	char messages[QUEUE_SIZE][BUFFER_SIZE];
	int client_socket_fds[QUEUE_SIZE];
	int front;
	int rear;
	int numMessages;
};

void messageQueue_init(struct messageQueue* Q);
// enqueue returns 0 on success, -1 on overflow (message dropped)
int enqueue(struct messageQueue* Q, const char* message, int socket_fd);
// dequeue returns pointer to message buffer on success, NULL if empty.
// If NULL is returned, *socket_fd will be set to -1.
char* dequeue(struct messageQueue* Q, int* socket_fd);

#endif
