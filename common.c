#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"


int sockaddr_dotted(struct sockaddr_in* sockaddr, char* dotted_addr,
		in_port_t port) {
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	return inet_aton(dotted_addr, &sockaddr->sin_addr);
}

