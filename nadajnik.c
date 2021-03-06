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
#include "sendbuff.h"

#define MAX_PENDING_RET 1<<16

/* transmitter configuration */
static char mcast_dotted[ADDR_LEN] = "";
static in_port_t data_port = DATA_PORT;
static in_port_t ctrl_port = CTRL_PORT;
static int psize = PSIZE;
static int fsize = FSIZE;
static int rtime = RTIME;
static char name[NAME_LEN] = "Nienazwany Nadajnik";

static struct proto_ident pack_my_ident;
static struct proto_packet pack_ret_failed;

/* sockets */
static struct sockaddr_in mcast_addr;

static int mcast_sock;
static int ctrl_sock;

/* events */
static struct event_base *base;

static struct event *stdin_evt;
static struct event *ctrl_evt;
static struct event *rtime_evt;

/* transmitter state */
static seqno_t last_seqno = 0;

/* retransmit requests */
/* if packet in buffer needs to be retransmitted its PROTO_DORETR flag is set */

/* buffered packets */
static struct sendbuff packets;

/* callbacks */
void stdin_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	static ssize_t len = 0, r = 0;

	struct proto_packet *packet = sendbuff_back(&packets);
	struct proto_header *header = &packet->header;

	TRY_SYS(r = read(sock, packet->data + len, psize - len));
	if (r) {
		len += r;
		if (len == psize) {
			/* packet is full, init */
			header_init(header, last_seqno++, len, PROTO_DATA);
			ASSERT(psize + (int) sizeof(struct proto_header) == packets.hpsize);
			ASSERT(packets.hpsize == (int) packet_length(packet));
			/* send */
			EXPECT(write(mcast_sock, packet, packets.hpsize) == packets.hpsize,
					"Sending streming data failed.");
			/* start new packet */
			len = 0;
			sendbuff_next(&packets);
		}
	} else {
		if (len) {
			/* end of input, init packet as is */
			header_init(header, last_seqno++, len, PROTO_DATA);
			/* send */
			ASSERT(len + sizeof(struct proto_header));
			len = packet_length(packet);
			EXPECT(write(mcast_sock, packet, len) == len,
					"Sending streaming data failed.");
			/* everything sent */
			len = 0;
		}
		/* end of input file, exit */
		event_base_loopexit(base, NULL);
	}
}

void ctrl_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	struct proto_header header;
	ssize_t r;
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(addr);

	TRY_SYS(r = recvfrom(sock, &header, sizeof(header), 0,
				(struct sockaddr *) &addr, &addr_len));
	if (validate_packet((struct proto_packet *) &header, r) && header_isempty(&header)) {
		if (header_flag_isset(&header, PROTO_RETQUERY)) {
			/* mark packet in buffer with PROTO_DORETR flag */
			seqno_t ask_seqno = header_seqno(&header);
			struct proto_packet *pack = sendbuff_getseqno(&packets, ask_seqno);

			if (pack) {
				header_flag_set(&pack->header, PROTO_DORETR);
			} else {
				/* invalid retransmission request - response directly to requester */
				header_init(&pack_ret_failed.header, ask_seqno, 0, PROTO_FAIL);

				EXPECT(sendto(ctrl_sock, &pack_ret_failed, sizeof(pack_ret_failed), 0,
							(struct sockaddr *) &addr, addr_len) == sizeof(pack_ret_failed),
						"Sending invalid retransmission response failed.");
			}
		}
		/* whatever we've done addr is still valid */
		if (header_flag_isset(&header, PROTO_IDQUERY)) {
			EXPECT(sendto(sock, &pack_my_ident, sizeof(pack_my_ident), 0, (struct sockaddr *)
						&addr, addr_len) == sizeof(pack_my_ident),
					"Sending id response failed.");
		}
	}
	/* else: ignore packet */
}

void do_retr_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	int i = 0;
	struct proto_packet *packet;
	while ((packet = sendbuff_getnth(&packets, i)) != NULL) {
		if (header_flag_isset(&packet->header, PROTO_DORETR)) {
			header_flag_clear(&packet->header, PROTO_DORETR);
			EXPECT(write(mcast_sock, packet, packets.hpsize) == packets.hpsize,
					"Sending retransmission response failed.");
		}
		/* next */
		++i;
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
				strncpy(mcast_dotted, optarg, sizeof(mcast_dotted) - 1);
				if (strlen(mcast_dotted) == 0)
					errflg++;
				break;
			case 'P':
				data_port = (in_port_t) atoi(optarg);
				break;
			case 'C':
				ctrl_port = (in_port_t) atoi(optarg);
				break;
			case 'p':
				psize = atoi(optarg);
				if (psize <= 0 || psize > (int) PROTO_MAX_DATA)
					errflg++;
				break;
			case 'f':
				fsize = atoi(optarg);
				if (fsize <= 0)
					errflg++;
				break;
			case 'R':
				rtime = atoi(optarg);
				if (rtime <= 0)
					errflg++;
				break;
			case 'n':
				strncpy(name, optarg, sizeof(name) - 1);
				break;
			default:
				errflg++;
				break;
		}
	}
	if (strlen(mcast_dotted) == 0)
		errflg++;

	if (errflg) {
		fprintf(stderr, "Error parsing parameters.\n");
		exit(EXIT_FAILURE);
	}

	/* initialize packets buffer */
	sendbuff_init(&packets, fsize, psize);

	{
		struct sockaddr_in temp_addr;
		temp_addr.sin_family = AF_INET;
		temp_addr.sin_addr.s_addr = htonl(INADDR_ANY);

		/* setup ctrl socket */
		temp_addr.sin_port = htons(ctrl_port);
		TRY_SYS(ctrl_sock = socket(PF_INET, SOCK_DGRAM, 0));
		TRY_SYS(bind(ctrl_sock, (const struct sockaddr *) &temp_addr, sizeof(temp_addr)));
		setup_multicast_sockopt(ctrl_sock, MCAST_TTL, MCAST_LOOPBACK);

		/* setup mcast socket */
		temp_addr.sin_port = htons(0);
		TRY_TRUE(sockaddr_dotted(&mcast_addr, mcast_dotted, data_port));

		TRY_SYS(mcast_sock = socket(PF_INET, SOCK_DGRAM, 0));
		TRY_SYS(bind(mcast_sock, (const struct sockaddr *) &temp_addr, sizeof(temp_addr)));
		setup_multicast_sockopt(mcast_sock, MCAST_TTL, MCAST_LOOPBACK);

		TRY_SYS(connect(mcast_sock, (struct sockaddr *) &mcast_addr, sizeof(mcast_addr)));

		/* better to keep such things precomputed due to no-copy policy */
		socklen_t addr_len = sizeof(temp_addr);
		TRY_SYS(getsockname(mcast_sock, (struct sockaddr *) &temp_addr, &addr_len));

		ident_init(&pack_my_ident, &mcast_addr, &temp_addr, psize);
		strncpy(pack_my_ident.tune_name, name, sizeof(pack_my_ident.tune_name) - 1);
		strncpy(pack_my_ident.app_name, argv[0], sizeof(pack_my_ident.app_name) - 1);
	}
	/* NOTE: pack_ret_failed must be reinitialized before sending with proper seqno */

	/* setup eventbase */
	TRY_TRUE(base = event_base_new());

	/* setup events */
	TRY_TRUE(stdin_evt = event_new(base, 0, EV_READ|EV_PERSIST, stdin_cb, NULL));
	TRY_SYS(event_add(stdin_evt, NULL));

	TRY_TRUE(ctrl_evt = event_new(base, ctrl_sock, EV_READ|EV_PERSIST, ctrl_cb, NULL));
	TRY_SYS(event_add(ctrl_evt, NULL));

	TRY_TRUE(rtime_evt = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, do_retr_cb, NULL));
	struct timeval rtime_tv = {(rtime / 1000), 1000*(rtime % 1000)};
	TRY_SYS(event_add(rtime_evt, &rtime_tv));

	/* dispatcher loop */
	TRY_SYS(event_base_dispatch(base));

	/* clean exit */
	event_free(stdin_evt);
	event_free(ctrl_evt);
	event_free(rtime_evt);

	event_base_free(base);
	close(mcast_sock);

	sendbuff_free(&packets);

	exit(EXIT_SUCCESS);
}
