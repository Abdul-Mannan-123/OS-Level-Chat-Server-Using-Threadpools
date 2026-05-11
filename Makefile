CC=gcc
CFLAGS=-std=c11 -pthread -Wall -O2
SRCS=server.c data_structs.c socket_utils.c

INTERACTIVE_SRCS=interactive_client.c socket_utils.c

.PHONY: all server interactive_client clean run

all: server interactive_client

server: $(SRCS)
	$(CC) $(CFLAGS) server.c data_structs.c socket_utils.c -o server

interactive_client: $(INTERACTIVE_SRCS)
	$(CC) $(CFLAGS) interactive_client.c socket_utils.c -o interactive_client

run: server
	./server > final_test_server.log 2>&1 & echo $$! > server.pid

clean:
	rm -f server interactive_client *.o final_test_server.log server.pid
