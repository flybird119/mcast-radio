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
#include "stations.h"
#include "recvbuff.h"

#define UI_CLIENTS_MAX 32
#define NO_SOCK -1

#define DISCOVER_BURST_NUM 6
#define DISCOVER_BURST_TIMEOUT 500
#define DISCOVER_TIMEOUT 5000

#define FLUSH_THRESH 75 /* % */

#define RCOUNT_MAX 8
#define RDELAY_FIRST 2
#define RDELAY_NEXT 2

#define TELNET_KEY_DOWN 4348699
#define TELNET_KEY_UP 4283163

/* receiver configuration */
char discover_dotted[ADDR_LEN] = DISCOVER_ADDR;
in_port_t data_port = DATA_PORT;
in_port_t ctrl_port = CTRL_PORT;
int ui_port = UI_PORT;
int bsize = BSIZE;
int rtime = RTIME;
char dest_tune_name[NAME_LEN] = "";

/* sockets */
struct sockaddr_in discover_addr;
struct sockaddr_in ui_addr;

int ctrl_sock;
int mcast_sock;

struct ip_mreq current_membership;

/* events */
struct event_base *base;

struct event *rtime_evt;
struct event *discover_timeout_evt;
struct event *ctrl_recv_evt;
struct event *refresh_ui_evt;
struct event *mcast_recv_evt;

struct evconnlistener *ui_listener;

/* received data buffer */
struct recvbuff packets;

/* stations */
struct stations_list stations;

void current_station_connect(struct stations_list *list) {
	struct station_desc* st = stations_list_current(list);

	if (st->expiry_ticks) {
		/* reinit buffers */
		// packets_buf_init(st->psize); // TODO fake station psize == 0

		/* change mcast_sock multicast group membership */
		/* remove from old group */
		if (current_membership.imr_multiaddr.s_addr != htonl(INADDR_ANY)) {
			/* we're added to a group */
			TRY_SYS(setsockopt(mcast_sock, SOL_IP, IP_DROP_MEMBERSHIP,
						(void *) &current_membership, sizeof(current_membership)));
		}
		/* add to new group */
		memcpy(&current_membership.imr_multiaddr, &st->mcast_addr.sin_addr,
				sizeof(current_membership.imr_multiaddr));
		if (st->mcast_addr.sin_addr.s_addr) {
			TRY_SYS(setsockopt(mcast_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
						(void *) &current_membership, sizeof(current_membership)));
			/* the following doesn't work one would expect, we have to manually check
			 * address got from recvfrom() and compare with current station */
			/* TRY_SYS(connect(mcast_sock, (struct sockaddr *) &st->local_addr, sizeof(st->local_addr))); */
			fprintf(stderr, "Connected %s:%d.\n", inet_ntoa(st->local_addr.sin_addr), ntohs(st->local_addr.sin_port));
		}
		/* ready to listen */
	}
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
void mcast_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	static char buffer[PROTO_MAX_PACKET];

	ssize_t r = 0;
	struct proto_packet *packet = (struct proto_packet *) buffer;
	struct proto_header *header = &packet->header;
	struct station_desc *st = stations_list_current(&stations);

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);

	TRY_SYS(r = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &addr, &len));
	fprintf(stderr, "Read smth from %s.\n", inet_ntoa(addr.sin_addr));
	if (r && r == (int) packet_length(packet) && check_version(header)) {
		/* this should be done via connect() but doesn't work that way */
		if (addr.sin_addr.s_addr != st->local_addr.sin_addr.s_addr
				|| addr.sin_port != st->local_addr.sin_port) {
			/* ignore packet */
			return;
		}
		len_t length = data_length(packet);
		if (header_flag_isset(header, PROTO_DATA) && length <= packets.psize) {
			seqno_t seqno = header_seqno(header);
			fprintf(stderr, "Packet with seqno %d of total length %d.\n",seqno, length);
			/* check if it's a first packet */
			if (packets.end == 0 && packets.fseqno == 0) {
				/* we assume that this is the first packet from this station ever */
				packets.fseqno = seqno;
			}
			/* copy data */
			int index = recvbuff_index(&packets, seqno);
			char *data = recvbuff_buf_get(&packets, index);
			struct packet_desc *pdesc = recvbuff_map_get(&packets, index);
			if (data) {
				memcpy(data, packet->data, length);
				/* mark as valid */
				pdesc->length = length;
				/* mark packets up to this one as pending retransmission */
				struct packet_desc *d = recvbuff_map_get(&packets, packets.end);
				/* we start from end, none of these packets had been known to receiver before */
				while(d < pdesc) { /* implied range checking */
					/* proper delay */
					d->rdelay = RDELAY_FIRST;
					d->rcount = RCOUNT_MAX;
					d->length = 0;
					/* next */
					++d;
					++packets.end;
				}
			}
			/* else: seqno out of range */
		}
		/* else: ignore packet */
	} 
	/* else: ignore packet */
}

void rtime_timeout_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	/* find last dead packet */
	int last_dead = -1;
	int dead_count = 0;
	for (int i = packets.consistient; i < packets.end; ++i) {
		struct packet_desc *pdesc = recvbuff_map_get(&packets, i);
		if (pdesc->length == 0 && pdesc->rdelay == 0 && pdesc->rcount == 0) {
			/* dead packet */
			last_dead = i;
			dead_count++;
		}
	}
	/* depending on policy try to flush buffer up to this place or discard */
	if (dead_count > 0) {
		recvbuff_flush(&packets, -1, last_dead + 1); /* flush including last dead packet */
	}

	/* make retransmission requests */
	/* if there's no station (fake one), then we haven't got any packets
	 * and packets.end == packets.consistient */
	for (int i = packets.consistient; i < packets.end; ++i) {
		struct packet_desc *pdesc = recvbuff_map_get(&packets, i);
		if (pdesc->length == 0) {
			if (pdesc->rdelay == 0) {
				if (pdesc->rcount > 0) {
					pdesc->rcount--;
					/* send request */
					struct proto_packet packet;
					struct station_desc *st = stations_list_current(&stations);

					header_init(&packet.header, packets.fseqno + i, 0, PROTO_RETQUERY);
					EXPECT(sendto(ctrl_sock, &packet, packet_length(&packet), 0, (struct sockaddr *)
								&st->ctrl_addr, sizeof(st->ctrl_addr)) == (int) packet_length(&packet),
							"Sending retransmission request failed.\n");
				} else {
					/* this shouldn't happened */
					ASSERT(0);
				}
			} else {
				/* update delay */
				pdesc->rdelay--;
			}
		}
	}
}

void discover_timeout_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	static int discover_burst = DISCOVER_BURST_NUM;

	/* send discover request */
	struct proto_packet packet;
	header_init(&packet.header, 0, 0, PROTO_IDQUERY);

	EXPECT(sendto(ctrl_sock, &packet, packet_length(&packet), 0, (struct sockaddr *)
				&discover_addr,	sizeof(discover_addr)) == (int) packet_length(&packet),
			"Sending identification request failed.\n");

	if (discover_burst) {
		--discover_burst;
		if (0 == discover_burst) {
			/* switch to normal discover timeout */
			struct timeval dtime_tv = {(DISCOVER_TIMEOUT / 1000),
				1000*(DISCOVER_TIMEOUT % 100)};
			TRY_SYS(event_add(discover_timeout_evt, &dtime_tv));
		}
	}

	int r = 0;
	/* remove expired stations */
	int i;
	for (i = 0; i < stations.capacity; ++i) {
		/* NOTE: EXPIRY_INFTY < 0 */
		struct station_desc *st = stations_list_get(&stations, i);
		if (st->expiry_ticks > 0) {
			st->expiry_ticks--;
			if (st->expiry_ticks == 0)
				r++;
		}
	}
	/* if current station disappears */
	if (stations_list_current(&stations)->expiry_ticks == 0) {
		if (next_station(&stations)) {
			current_station_connect(&stations);
		}
		r++;
	}

	/* refresh */
	if (r)
		event_active(refresh_ui_evt, 0, 0);
}

void ctrl_recv_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	struct proto_ident packet;
	ssize_t r;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);

	TRY_SYS(r = recvfrom(sock, &packet, sizeof(packet), 0,
				(struct sockaddr *) &addr, &addrlen));
	if (r && check_version(&packet.header)) {
		if (r == sizeof(struct proto_ident) && header_flag_isset(&packet.header, PROTO_IDRESP)) {
			/* we've got identification response */
			/* check if already exists */
			struct station_desc *oldst = stations_list_find(&stations, &packet);
			if (oldst) {
				/* reset station */
				oldst->expiry_ticks = DISCOVER_KICK_THRESH;
			} else {
				/* add new station */
				struct station_desc *st = stations_list_new(&stations);
				/* if found free slot */
				if (st) {
					/* init station */
					station_desc_init(st, &packet, &addr);

					/* check if we're waiting for this station (and switch if so) */
					ASSERT(sizeof(dest_tune_name) == sizeof(st->tune_name));
					ASSERT(dest_tune_name[sizeof(dest_tune_name) - 1] == 0);
					if (strcmp(st->tune_name, dest_tune_name) == 0)
						stations_list_set_current(&stations, st);
						current_station_connect(&stations);
					/* refresh if added */
					event_active(refresh_ui_evt, 0, 0);
				}
			}
		} else if (r == sizeof(struct proto_header) && header_flag_isset(&packet.header, PROTO_FAIL)) {
			/* we've been notified that retransmission failed */
			fprintf(stderr, "Retransmission of packet %d failed.\n", header_seqno(&packet.header));
			/* find packet in packets.map and set rcount = rdelay = 0 */
			struct packet_desc *pdesc = recvbuff_map_get(&packets, recvbuff_index(&packets, header_seqno(&packet.header)));
			if (pdesc) {
				pdesc->rcount = pdesc->rdelay = 0;
				/* NOTE: we're leavind decision what to do with buffer until rtime timeout */
			}
		}
		/* else: ignore other responses */
	}
	/* else: ignore packet */
}

void refresh_ui_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	static unsigned char cls_comm[] = {0x1b, '[', '2', 'J'};
	static char rendbuf[16192];
	static const int rendbufsz = sizeof(rendbuf);

	int n = stations_list_render(&stations, rendbuf, rendbufsz);
	/* send to each client */
	for (int i = 0; i < ui_clients_socks_cap; ++i) {
		if (ui_clients_socks[i] != NO_SOCK) {
			/* clean screen */
			EXPECT(write(ui_clients_socks[i], cls_comm, SIZEOF(cls_comm)) == SIZEOF(cls_comm),
					"Clearing remote screen failed.");
			TRY_TRUE(write(ui_clients_socks[i], rendbuf, n) == n);
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
			if (next_station(&stations)) {
				current_station_connect(&stations);
				event_active(refresh_ui_evt, 0, 0);
			}
		} else if (comm == TELNET_KEY_UP) {
			if (prev_station(&stations)) {
				current_station_connect(&stations);
				event_active(refresh_ui_evt, 0, 0);
			}
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
	 * but we'll skip it in event callback eitherway
	 * if client really can't change to character mode we really
	 * can't do much more than just ask it */

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
	while ((c = getopt(argc, argv, "d:P:C:b:R:n:U:")) != -1) {
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
				strncpy(dest_tune_name, optarg, sizeof(dest_tune_name) - 1);
				break;
			case 'U':
				ui_port = atoi(optarg);
				break;
			default:
				errflg++;
				break;
		}
	}

	if (errflg) {
		fprintf(stderr, "Error parsing parameters.\n");
		exit(EXIT_FAILURE);
	}

	/* setup stations */
	stations_list_init(&stations);

	/* setup ui clients list */
	for (int i = 0; i < ui_clients_socks_cap; ++i)
		ui_clients_socks[i] = NO_SOCK;

	/* setup addresses */
	TRY_TRUE(sockaddr_dotted(&discover_addr, discover_dotted, ctrl_port) == 1);

	ui_addr.sin_family = AF_INET;
	ui_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ui_addr.sin_port = htons(ui_port);

	/* we need this to determine whether we're a member of group */
	current_membership.imr_multiaddr.s_addr = htonl(INADDR_ANY);
	current_membership.imr_interface.s_addr = htonl(INADDR_ANY);

	{
		struct sockaddr_in temp_addr;
		temp_addr.sin_family = AF_INET;
		temp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		/* setup ctrl socket */
		temp_addr.sin_port = htons(0);
		TRY_SYS(ctrl_sock = socket(PF_INET, SOCK_DGRAM, 0));
		TRY_SYS(bind(ctrl_sock, (const struct sockaddr *) &temp_addr,
					sizeof(temp_addr)));
		setup_multicast_sockopt(ctrl_sock, MCAST_TTL, MCAST_LOOPBACK);

		/* setup mcast socket */
		temp_addr.sin_port = htons(data_port);
		TRY_SYS(mcast_sock = socket(PF_INET, SOCK_DGRAM, 0));
		TRY_SYS(bind(mcast_sock, (struct sockaddr *) &temp_addr,
					sizeof(temp_addr)));
		setup_multicast_sockopt(mcast_sock, MCAST_TTL, MCAST_LOOPBACK);
	}

	/* setup eventbase */
	TRY_TRUE(base = event_base_new());

	/* setup events */
	TRY_TRUE(mcast_recv_evt = event_new(base, mcast_sock,
				EV_READ|EV_PERSIST, mcast_recv_cb, NULL));
	TRY_SYS(event_add(mcast_recv_evt, NULL));

	TRY_TRUE(ui_listener = evconnlistener_new_bind(base, ui_new_client_cb, NULL,
				LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
				(struct sockaddr*) &ui_addr, sizeof(ui_addr)));

	TRY_TRUE(refresh_ui_evt = event_new(base, -1, 0, refresh_ui_cb, NULL));

	TRY_TRUE(ctrl_recv_evt = event_new(base, ctrl_sock,
				EV_READ|EV_PERSIST, ctrl_recv_cb, NULL));
	TRY_SYS(event_add(ctrl_recv_evt, NULL));

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
	event_free(mcast_recv_evt);
	event_free(refresh_ui_evt);
	event_free(ctrl_recv_evt);
	event_free(discover_timeout_evt);
	event_free(rtime_evt);

	event_base_free(base);
	close(ctrl_sock);
	close(mcast_sock);

	recvbuff_free(&packets);

	exit(EXIT_SUCCESS);
}
