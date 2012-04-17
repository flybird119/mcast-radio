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

#define STATIONS_MAX 32

#define DISCOVER_BURST_NUM 6
#define DISCOVER_BURST_TIMEOUT 500
#define DISCOVER_TIMEOUT 5000
#define DISCOVER_KICK_THRESH 4

/* receiver configuration */
char discover_dotted[ADDR_LEN] = DISCOVER_ADDR;
in_port_t data_port = DATA_PORT;
in_port_t ctrl_port = CTRL_PORT;
int ui_port = UI_PORT;
int bsize = BSIZE;
int rtime = RTIME;
char tune_name[NAME_LEN] = "";

/* sockets */
struct sockaddr_in local_addr;
struct sockaddr_in discover_addr;

int discover_sock;

/* events */
struct event_base *base;

struct event *rtime_evt;
struct event *discover_timeout_evt;
struct event *discover_recv_evt;

/* receiver state */
int discovery_burst = DISCOVER_BURST_NUM;

/* received data buffer */

/* stations */
struct station {
	struct sockaddr_in mcast_addr;
	tune_name[NAME_LEN];
};
struct station stations[STATIONS_MAX];
int stations_cap = sizeof(stations);
int stations_end = 0;
int stations_cur = 0;

/* callbacks */
void data_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	// TODO
}

void ctrl_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	// TODO
}

void rtime_timeout_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	// TODO
}

void discover_timeout_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	/* send discover request */
	struct proto_packet packet;
	init_header(packet.header, 0, 0, PROTO_IDQUERY);

	EXPECT(sendto(discover_sock, &packet, packet_len(packet), 0, (struct sockaddr *)
				&discover_addr,	sizeof(discover_addr)) == (int) packet_len(packet),
			"Sending identification request failed.\n");

	if (discovery_burst) {
		--discovery_burst;
		if (0 == discovery_burst) {
			/* switch to normal discovery timeout */
			struct timeval dtime_tv = {(DISCOVER_TIMEOUT / 1000),
				1000*(DISCOVER_TIMEOUT % 100)};
			TRY_SYS(event_add(discover_timeout_evt, &dtime_tv));
		}
	}
	// TODO decrement stations markers and remove if void
}

void discover_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	struct proto_ident packet;
	ssize_t r;
	TRY_SYS(r = read(sock, &packet, sizeof(packet)));
	if (r == sizeof(packet)) {
		if (PROTO_IDRESP & packet.header.flags) {
			// TODO check if already exists
			// TODO mark if so
			// TODO add if not
			if (stations_end < stations_cap) {
				struct station *st = stations[stations_end];
				memcpy(&st->mcast_addr, &packet.mcast_addr, sizeof(st->mcast_addr));
				strncpy(st->tune_name, packet->tune_name, sizeof(st->tune_name) - 1);
				// check if we're waiting for it (and switch if so)
				ASSERT(sizeof(tune_name) == sizeof(st->tune_name));
				ASSERT(tune_name[sizeof(tune_name) - 1] == 0);
				if (strcmp(st->tune_name, tune_name) == 0) {
					stations_curr = stations_end;
				}
				++stations_end;
			}
		}
	}
	/* else: ignore packet */
}

void ui_new_client_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	// TODO
}


int main(int argc, char **argv) {
	/* parse options */
	int errflg = 0;
	extern char *optarg;
	extern int optind, optopt;
	int c;
	while ((c = getopt(argc, argv, "d:P:C:b:R:n:")) != -1) {
		switch(c) {
			case 'd':
				strncpy(discover_dotted, optarg, sizeof(discover_dotted) - 1);
				if (strlen(discover_dotted) == 0)
					errflg++;
				break;
			case 'P':
				data_port = (in_port_t) atoi(optarg);
				break;
			case 'C':
				ctrl_port = (in_port_t) atoi(optarg);
				break;
			case 'b':
				bsize = atoi(optarg);
				if (bsize <= 0)
					errflg++;
				break;
			case 'R':
				rtime = atoi(optarg);
				if (rtime <= 0)
					errflg++;
				break;
			case 'n':
				strncpy(tune_name, optarg, sizeof(tune_name) - 1);
				break;
			default:
				errflg++;
				break;
		}
	}

	if (errflg) {
		fprintf(stderr, "Usage: %s ... \n", argv[0]); // TODO
		exit(EXIT_FAILURE);
	}

	/* setup addresses  */
	local_addr.sin_family = AF_INET;
	local_addr.sin_family = htonl(INADDR_ANY);
	local_addr.sin_port = htons(0);

	TRY_TRUE(sockaddr_dotted(&discover_addr, discover_dotted, ctrl_port) == 1);

	/* setup sockets */
	TRY_SYS(discover_sock = socket(PF_INET, SOCK_DGRAM, 0));
	{
		int optval = 1;
		TRY_SYS(setsockopt(discover_sock, SOL_SOCKET, SO_BROADCAST,
					(void*) &optval, sizeof(optval)));
	}
	TRY_SYS(bind(discover_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)));

	/* setup eventbase */
	TRY_TRUE(base = event_base_new());

	/* setup events */
	// TODO
	
	TRY_TRUE(discover_recv_evt = event_new(base, discover_sock,
				EV_READ|EV_PERSIST, discover_recv_cb, NULL));
	TRY_SYS(event_add(discover_recv_evt, NULL));

	TRY_TRUE(discover_timeout_evt = 
			event_new(base, -1, EV_TIMEOUT|EV_PERSIST, discover_timeout_cb, NULL));
	struct timeval dtime_tv = {0, 1000*DISCOVER_BURST_TIMEOUT};
	TRY_SYS(event_add(discover_timeout_evt, &dtime_tv));

	TRY_TRUE(rtime_evt = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, rtime_timeout_cb, NULL));
	struct timeval rtime_tv = {(rtime / 1000), 1000*(rtime % 1000)};
	TRY_SYS(event_add(rtime_evt, &rtime_tv));

	/* dispatcher loop */
	TRY_SYS(event_base_dispatch(base));

	/* clean exit */
	event_free(discover_timeout_evt);
	event_free(rtime_evt);

	event_base_free(base);
	close(discover_sock);

	exit(EXIT_SUCCESS);
}
