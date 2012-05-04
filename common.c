#include <stdarg.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"
#include "err.h"

int sockaddr_dotted(struct sockaddr_in* sockaddr, char* dotted_addr, in_port_t port) {
	sockaddr->sin_family = AF_INET;
	sockaddr->sin_port = htons(port);
	return inet_aton(dotted_addr, &sockaddr->sin_addr);
}

void setup_multicast_sockopt(int sock, int ttl, int loopback) {
	int optval = 1;
	/* enable multicast sending */
	TRY_SYS(setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
				(void*) &optval, sizeof(optval)));
	/* set TTL for multicast packets */
	optval = ttl;
	TRY_SYS(setsockopt(sock, SOL_IP, IP_MULTICAST_TTL, (void *) &optval,
				sizeof(optval)));
	/* disable loopback for multicast packets */
	if (!loopback) {
		optval = 0;
		TRY_SYS(setsockopt(sock, SOL_IP, IP_MULTICAST_LOOP, (void *) &optval,
					sizeof(optval)));
	}
}

void dlog(const char *fmt, ...) {
	UNUSED(fmt);
#ifdef DEBUG_FLAG
	va_list fmt_args;

	va_start(fmt_args, fmt);
	vfprintf(stderr, fmt, fmt_args);
	va_end (fmt_args);
#endif
}
