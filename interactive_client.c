#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "socket_utils.h"

#define BUFFER_SIZE 256
#define SERVER_RESPONSE_SIZE 4096

int sockfd = -1;
int running = 1;

static int write_all(int fd, const char* buffer, size_t length) {
    size_t total = 0;
    while (total < length) {
        ssize_t written = write(fd, buffer + total, length - total);
        if (written <= 0) {
            return -1;
        }
        total += (size_t)written;
    }
    return 0;
}

void* receive_thread(void* arg) {
    char buffer[SERVER_RESPONSE_SIZE];
    int n;
    
    while (running) {
        n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            printf("\n[SERVER] %s\n> ", buffer);
            fflush(stdout);
        } else if (n == 0) {
            printf("\n[SERVER DISCONNECTED]\n");
            running = 0;
            break;
        } else {
            perror("recv");
            running = 0;
            break;
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <port> <your_nickname>\n", argv[0]);
        exit(1);
    }
    
    const char* host = argv[1];
    int port = atoi(argv[2]);
    const char* nickname = argv[3];
    
    struct addrinfo hints;
    struct addrinfo* server_info = NULL;
    struct addrinfo* current = NULL;
    char port_text[16];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_text, sizeof(port_text), "%d", port);
    if (getaddrinfo(host, port_text, &hints, &server_info) != 0) {
        fprintf(stderr, "Could not resolve host: %s\n", host);
        exit(1);
    }

    for (current = server_info; current != NULL; current = current->ai_next) {
        sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        if (connect(sockfd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(server_info);

    if (sockfd < 0) {
        perror("connect");
        exit(1);
    }
    
    printf("Connected to server at %s:%d\n", host, port);
    printf("Type commands: /nick <name>, /join <room>, /msg <user> <msg>, /users, /rooms, /quit\n\n");
    
    // Start receiver thread
    pthread_t rx_thread;
    pthread_create(&rx_thread, NULL, receive_thread, NULL);
    
    // Send nickname
    char cmd[BUFFER_SIZE];
    snprintf(cmd, sizeof(cmd), "/nick %s\n", nickname);
    if (write_all(sockfd, cmd, strlen(cmd)) < 0) {
        perror("write");
        close(sockfd);
        exit(1);
    }
    
    sleep(0.5);
    printf("> ");
    fflush(stdout);
    
    // Read user input
    char input[BUFFER_SIZE];
    while (running && fgets(input, sizeof(input), stdin) != NULL) {
        if (strlen(input) > 0) {
            if (write_all(sockfd, input, strlen(input)) < 0) {
                perror("write");
                break;
            }
            
            if (strncmp(input, "/quit", 5) == 0) {
                running = 0;
                break;
            }
        }
        if (running) {
            printf("> ");
            fflush(stdout);
        }
    }
    
    close(sockfd);
    return 0;
}
