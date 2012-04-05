#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <event2/event.h>

#include "err.h"
#include "common.h"
#include "proto.h"

#define MAX_PENDING_RET 1<<16

/* transmitter configuration */
char mcast_dotted[ADDR_LEN] = "";
in_port_t data_port = DATA_PORT;
in_port_t ctrl_port = CTRL_PORT;
int psize = PSIZE;
int fsize = FSIZE;
int rtime = RTIME;
char name[NAME_LEN] = "Nienazwany Nadajnik";

struct proto_ident my_ident;

/* sockets */
struct sockaddr_in local_addr;
struct sockaddr_in mcast_addr;

int mcast_sock;
int ctrl_sock;

/* events */
struct event_base *base;

struct event *stdin_evt;
struct event *ctrl_evt;
struct event *rtime_evt;

/* transmitter state */
struct proto_header pending_ret[MAX_PENDING_RET]; // TODO save also receipent
int pending_retsz;

/* callbacks (should be treated like ISRs) */
void stdin_cb(evutil_socket_t sock, short ev, void *arg) {
	static char buff[PSIZE];
	static ssize_t len, r;

	TRY_ZERO(r = read(sock, buff + len, sizeof(buff) - len));
	if (r) {
		len += r;
		if (len == sizeof(buff)) {
			// TODO push
		}
	} else {
		// TODO push
		// TODO end of input file
		event_base_loopexit(base, NULL);
	}
}

void ctrl_cb(evutil_socket_t sock, short ev, void *arg) {
	struct proto_header *packet = pending_ret + pending_retsz; /* no-copy */

	ssize_t r;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);
	TRY_ZERO(r = recvfrom(sock, packet, sizeof(*packet), 0,
				(struct sockaddr *) &addr, &addr_len));
	if (r == sizeof(*packet)) {
		if (PROTO_RET & packet->flags) {
			pending_retsz++; /* everything in the right place */
			pending_retsz %= sizeof(pending_ret);
			/* NOTE: if we're losing many packets the oldest requests get overriden,
			 * many means more than MAX_PENDING_RET during RTIME ms */
		}
		if (PROTO_IDQ & packet->flags) {
			/* NOTE: we don't want to fail here */
			if (sendto(sock, &my_ident, sizeof(my_ident), 0,
					(struct sockaddr *) &addr, addr_len) != sizeof(my_ident))
				fprintf(stderr, "Sending ID response failed.\n");
		}
	}
	/* else: ignore packet */
}

void do_retr_cb(evutil_socket_t sock, short ev, void *arg) {
	for (; pending_retsz > 0; --pending_retsz) {
		// TODO
	}
}

int main(int argc, char **argv) {
	/* parse options */
		int errflg = 0;
		extern char *optarg;
		extern int optind, optopt;
		int c;
		while ((c = getopt(argc, argv, "a:P:C:p:f:R:n:")) != -1) {
			switch(c) {
				case 'a':
					strncpy(mcast_dotted, optarg, sizeof(mcast_dotted));
					break;
				case 'P':
					data_port = (in_port_t) atoi(optarg);
					break;
				case 'C':
					ctrl_port = (in_port_t) atoi(optarg);
					break;
				case 'p':
					psize = atoi(optarg);
					break;
				case 'f':
					fsize = atoi(optarg);
					break;
				case 'R':
					rtime = atoi(optarg);
					break;
				case 'n':
					strncpy(name, optarg, sizeof(name));
					break;
				default:
					errflg++;
					break;
			}
		}
		if (strlen(mcast_dotted) == 0)
			errflg++;

		if (errflg) {
			fprintf(stderr, "Usage: %s ... \n", argv[0]); // TODO
			exit(EXIT_FAILURE);
		}

	/* setup ctrl socket */
	local_addr.sin_family = AF_INET;
	local_addr.sin_family = htonl(INADDR_ANY);
	local_addr.sin_port = htons(ctrl_port);

	TRY_ZERO(ctrl_sock = socket(PF_INET, SOCK_DGRAM, 0));
	TRY_ZERO(bind(ctrl_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)));

	/* setup mcast socket */
	TRY_TRUE(sockaddr_dotted(&mcast_addr, mcast_dotted, data_port) == 1);
	TRY_ZERO(mcast_sock = socket(PF_INET, SOCK_DGRAM, 0));
	{
		int optval = 1;
		TRY_ZERO(setsockopt(mcast_sock, SOL_SOCKET, SO_BROADCAST,
					(void*) &optval, sizeof(optval)));
	}
	TRY_ZERO(connect(mcast_sock, (struct sockaddr *) &mcast_addr, sizeof(mcast_addr)));

	/* better to keep such things precomputed -- no-copy */
	my_ident.header.flags = PROTO_IDR;
	memcpy(&my_ident.mcast_addr, &mcast_addr, sizeof(my_ident.mcast_addr));
	strncpy(my_ident.app_name, argv[0], sizeof(my_ident.app_name));
	strncpy(my_ident.tune_name, name, sizeof(my_ident.tune_name));

	/* setup eventbase */
	TRY_TRUE(base = event_base_new());

	/* setup events */
	TRY_TRUE(stdin_evt = event_new(base, 0, EV_READ|EV_PERSIST, stdin_cb, NULL));
	TRY_ZERO(event_add(stdin_evt, NULL));

	TRY_TRUE(ctrl_evt = event_new(base, ctrl_sock, EV_READ|EV_PERSIST, ctrl_cb, NULL));
	TRY_ZERO(event_add(ctrl_evt, NULL));

	TRY_TRUE(rtime_evt = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, do_retr_cb, NULL));
	struct timeval rtime_tv = {0, 1000*rtime};
	TRY_ZERO(event_add(rtime_evt, &rtime_tv));

	/* dispatcher loop */
	TRY_ZERO(event_base_dispatch(base));

	/* clean exit */
	event_free(stdin_evt);
	event_base_free(base);
	close(mcast_sock);

	exit(EXIT_SUCCESS);
}
