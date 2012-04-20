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
#include <event2/listener.h>

#include "err.h"
#include "common.h"
#include "proto.h"

#define STATIONS_MAX 32
#define UI_CLIENTS_MAX 32
#define NO_SOCK -1

#define DISCOVER_BURST_NUM 6
#define DISCOVER_BURST_TIMEOUT 500
#define DISCOVER_TIMEOUT 5000
#define DISCOVER_KICK_THRESH 4

#define TELNET_KEY_DOWN 4348699
#define TELNET_KEY_UP 4283163

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
struct sockaddr_in ui_addr;

int discover_sock;

/* events */
struct event_base *base;

struct event *rtime_evt;
struct event *discover_timeout_evt;
struct event *discover_recv_evt;
struct event *refresh_ui_evt;

struct evconnlistener *ui_listener;

/* receiver state */
int discovery_burst = DISCOVER_BURST_NUM;

/* received data buffer */

/* stations */
struct station_desc {
	char expiry_ticks;
	struct sockaddr_in mcast_addr;
	char tune_name[NAME_LEN];
};
struct station_desc stations[STATIONS_MAX];
int stations_cap = SIZEOF(stations);
int stations_curr = 0;

struct station_desc* stations_free_slot() {
	int i;
	for (i = 0; i < stations_cap; ++i)
		if (stations[i].expiry_ticks == 0)
			break;
	if (i < stations_cap)
		return (stations + i);
	else
		return NULL;
}

void switch_station(int new_st) {
	stations_curr = new_st;
	// TODO
	// reinit buffers
	// change mcast

	// TODO what to do with retransmission packets from previous station?
}

void next_station() {
	int i = stations_curr;
	do {
		i = (i+1) % stations_cap;
	} while (i != stations_curr);
	switch_station((i == stations_curr) ? 0 : i);
}

void prev_station() {
	int i = stations_curr;
	do {
		--i;
		if (i < 0)
			i = stations_cap - 1;
	} while (i != stations_curr);
	switch_station((i == stations_curr) ? 0 : i);
}

/* ui clients */
int ui_clients_socks[UI_CLIENTS_MAX]; /* must be initialized */
int ui_clients_socks_cap = SIZEOF(ui_clients_socks);

int *ui_clients_free_slot() {
	int i;
	for (i = 0; i < ui_clients_socks_cap; ++i)
		if (ui_clients_socks[i] == NO_SOCK)
			break;
	if (i < ui_clients_socks_cap)
		return (ui_clients_socks + i);
	else
		return NULL;
}

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
	header_init(&packet.header, 0, 0, PROTO_IDQUERY);

	EXPECT(sendto(discover_sock, &packet, packet_length(&packet), 0, (struct sockaddr *)
				&discover_addr,	sizeof(discover_addr)) == (int) packet_length(&packet),
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

	int r = 0;
	/* remove expired stations */
	int i;
	for (i = 0; i < stations_cap; ++i) {
		if (stations[i].expiry_ticks > 0) {
			stations[i].expiry_ticks--;
			if (stations[i].expiry_ticks == 0)
				r++;
		}
	}
	/* if current station disappears */
	if (stations[stations_curr].expiry_ticks == 0) {
		next_station();
		r++;
	}

	/* refresh */
	if (r)
		event_active(refresh_ui_evt, 0, 0);
}

void discover_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	struct proto_ident packet;
	ssize_t r;
	TRY_SYS(r = read(sock, &packet, sizeof(packet)));
	if (check_version(&packet.header) && r == sizeof(packet)) {
		if (header_flag_isset(&packet.header, PROTO_IDRESP)) {
			/* check if already exists */
			int i;
			for (i = 0; i < stations_cap; ++i)
				if (stations[i].expiry_ticks &&
						(strcmp(stations[i].tune_name, packet.tune_name) == 0))
					break;
			if (i < stations_cap) {
				/* reset station */
				stations[i].expiry_ticks = DISCOVER_KICK_THRESH;
			} else {
				/* add new station */
				struct station_desc *st = stations_free_slot();
				/* if found free slot */
				if (st) {
					st->expiry_ticks = DISCOVER_KICK_THRESH;
					memcpy(&st->mcast_addr, &packet.mcast_addr, sizeof(st->mcast_addr));
					strncpy(st->tune_name, packet.tune_name, sizeof(st->tune_name) - 1);

					/* check if we're waiting for it (and switch if so) */
					ASSERT(sizeof(tune_name) == sizeof(st->tune_name));
					ASSERT(tune_name[sizeof(tune_name) - 1] == 0);
					if (strcmp(st->tune_name, tune_name) == 0)
						stations_curr = i;
					/* refresh if added */
					event_active(refresh_ui_evt, 0, 0);
				}
			}
		}
	}
	/* else: ignore packet */
}

void refresh_ui_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	static unsigned char cls_comm[] = {0x1b, '[', '2', 'J'};
	static char rendered_screen[8096];

	int n = 0;
	n += sprintf(rendered_screen + n, "+------------------------------Znalezione stacje:------------------------------+\r\n|\r\n");
	/* print stations list */
	for (int i = 0; i < stations_cap; ++i) {
		if (stations[i].expiry_ticks) {
			n += sprintf(rendered_screen + n,
					"| %c %d %s\r\n",
					(stations_curr == i) ? '>' : ' ',
					i+1,
					stations[i].tune_name);
		}
	}
	n += sprintf(rendered_screen + n, "+------------------------------------------------------------------------------+\r\n");

	/* send to each client */
	for (int i = 0; i < ui_clients_socks_cap; ++i) {
		if (ui_clients_socks[i] != NO_SOCK) {
			/* clean screen */
			EXPECT(write(ui_clients_socks[i], cls_comm, SIZEOF(cls_comm)) == SIZEOF(cls_comm),
					"Clearing remote screen failed.");
			TRY_TRUE(write(ui_clients_socks[i], rendered_screen, n) == n);
		}
	}
}

void ui_client_action_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	struct event *evt = arg;

	int comm = 0;
	ssize_t r;
	TRY_SYS(r = read(sock, &comm, sizeof(comm)));
	if (r) {
		if (comm == TELNET_KEY_DOWN) {
			next_station();
			event_active(refresh_ui_evt, 0, 0);
		} else if (comm == TELNET_KEY_UP) {
			prev_station();
			event_active(refresh_ui_evt, 0, 0);
		}
		/* else: unrecognized option */
	} else {
		/* end of connection */
		event_free(evt);
		close(sock);

		/* remove from clients list */
		for (int i = 0; i < ui_clients_socks_cap; ++i) {
			if (ui_clients_socks[i] == sock) {
				ui_clients_socks[i] = NO_SOCK;
				break;
			}
		}
	}
}

void ui_new_client_cb(struct evconnlistener *listener, evutil_socket_t sock,
		struct sockaddr *addr, int addrlen, void *arg) {
	UNUSED(listener);
	UNUSED(addr);
	UNUSED(addrlen);
	UNUSED(arg);

	/* put socket into clients list */
	int *slot = ui_clients_free_slot();
	if (slot) {
		*slot = sock;
	} else {
		/* no place - close connection */
		close(sock);
		/* ABORT */
		return;
	}

	/* force character mode */
	static unsigned char mode[] = {255, 251, 1, 255, 251, 3, 255, 252, 34};
	EXPECT(write(sock, mode, SIZEOF(mode)) == SIZEOF(mode),
			"Setting character mode of remote terminal failed.");
	/* we might read all the crap that telnet sent us in response,
	 * but we'll skip it in event callback eitherway */

	/* create event for new connection */
	struct event *new_evt = malloc(event_get_struct_event_size());
	TRY_SYS(event_assign(new_evt, base, sock, EV_READ|EV_PERSIST, ui_client_action_cb, new_evt));
	TRY_SYS(event_add(new_evt, NULL));

	/* redraw interface */
	event_active(refresh_ui_evt, 0, 0);
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

	/* setup buffers */
	for (int i = 0; i < ui_clients_socks_cap; ++i)
		ui_clients_socks[i] = NO_SOCK;

	/* setup addresses */
	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	local_addr.sin_port = htons(0);

	TRY_TRUE(sockaddr_dotted(&discover_addr, discover_dotted, ctrl_port) == 1);

	ui_addr.sin_family = AF_INET;
	ui_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ui_addr.sin_port = htons(ui_port);

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
	// TODO data recv
	// TODO ctrl recv

	TRY_TRUE(ui_listener = evconnlistener_new_bind(base, ui_new_client_cb, NULL,
				LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, (struct sockaddr*) &ui_addr, sizeof(ui_addr)));

	TRY_TRUE(refresh_ui_evt = event_new(base, -1, 0, refresh_ui_cb, NULL));

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
	evconnlistener_free(ui_listener);
	event_free(refresh_ui_evt);
	event_free(discover_recv_evt);
	event_free(discover_timeout_evt);
	event_free(rtime_evt);

	event_base_free(base);
	close(discover_sock);

	exit(EXIT_SUCCESS);
}
