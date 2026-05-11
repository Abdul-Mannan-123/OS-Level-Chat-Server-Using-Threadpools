#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_HOST_ADDRESS "127.0.0.1"
#define PORT_NUM 2000

int createTCPIpv4Socket ();
struct sockaddr* createTCPIpv4SocketAddress (char* ip, int port_num);

#endif
