#include "socket_utils.h"

int createTCPIpv4Socket()
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

struct sockaddr* createTCPIpv4SocketAddress(char* ip, int port_num)
{
	struct sockaddr_in* address = (struct sockaddr_in*) malloc(sizeof(struct sockaddr_in));
	address->sin_port = htons(port_num);
	address->sin_family = AF_INET;
	
	if (strlen(ip) == 0)
		address->sin_addr.s_addr = INADDR_ANY;
	else
		inet_pton(AF_INET, ip, &address->sin_addr.s_addr);
	
	return (struct sockaddr*) address;
}
