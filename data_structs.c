#include "data_structs.h"

void messageQueue_init(struct messageQueue* Q)
{
	Q->front = 0;
	Q->rear = 0;
	Q->numMessages = 0;
}

int enqueue(struct messageQueue* Q, const char* message, int socket_fd)
{
	if (Q->numMessages == QUEUE_SIZE)
	{
		// queue full
		return -1;
	}

	// copy safely with truncation
	if (message != NULL)
		snprintf(Q->messages[Q->rear], BUFFER_SIZE, "%s", message);
	else
		Q->messages[Q->rear][0] = '\0';

	Q->client_socket_fds[Q->rear] = socket_fd;
	Q->rear = (Q->rear + 1) % QUEUE_SIZE;
	Q->numMessages++;
	return 0;
}

char* dequeue(struct messageQueue* Q, int* socket_fd)
{
	if (Q->numMessages == 0)
	{
		Q->front = 0;
		Q->rear = 0;
		if (socket_fd) *socket_fd = -1;
		return NULL;
	}
	Q->numMessages--;
	int index = Q->front;
	Q->front = (Q->front + 1) % QUEUE_SIZE;
	if (socket_fd) *socket_fd = Q->client_socket_fds[index];
	return Q->messages[index];
}
