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

struct proto_ident pack_my_ident;
struct proto_packet pack_ret_failed;

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
seqno_t last_seqno = 0;

/* retransmit requests */
/* if packet in buffer needs to be retransmitted its PROTO_DORETR flag is set */

/* buffered packets */
char *packets_buf = NULL;
int packets_buf_cap = 0; /* highest available index */
int packets_buf_begin = 0; /* first byte index */
int packets_buf_end = 0; /* next to last byte index */
int packet_sz = 0; /* bytes */

struct proto_packet * packets_buf_get(const seqno_t seqno) {
	if (packets_buf_begin == packets_buf_end)
		return NULL;
	struct proto_header *header =
		(struct proto_header *) (packets_buf + packets_buf_begin * packet_sz);
	int index = seqno - header->seqno;
	if (index < 0)
		return NULL;
	index = (packets_buf_begin + index) % packets_buf_cap;
	header = (struct proto_header *) (packets_buf + index * packet_sz);
	if (seqno != header->seqno)
		return NULL;
	return (struct proto_packet *) header;
}

struct proto_packet * packets_buf_back() {
	return (struct proto_packet *) (packets_buf + packets_buf_end * packet_sz);
}

void packets_buf_next() {
	++packets_buf_end;
	if (packets_buf_end == packets_buf_begin) {
		/* won't fire first time, when buffer size is 1 */
		++packets_buf_begin;
		packets_buf_begin %= packets_buf_cap;
	}
	packets_buf_end %= packets_buf_cap;
}

/* callbacks */
void stdin_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(ev);
	UNUSED(arg);

	static ssize_t len = 0, r = 0;

	struct proto_packet *packet = packets_buf_back();
	struct proto_header *header = &packet->header;

	TRY_SYS(r = read(sock, packet->data + len, psize - len));
	if (r) {
		len += r;
		if (len == psize) {
			/* packet is full, init */
			init_header_p(header, last_seqno++, len, PROTO_DATA);
			ASSERT(psize + (int) sizeof(struct proto_header) == packet_sz);
			ASSERT(packet_sz == (int) packet_len_p(packet));
			/* send */
			EXPECT(write(mcast_sock, packet, packet_sz) == packet_sz,
					"Sending streming data failed.");
			/* start new packet */
			len = 0;
			packets_buf_next();
		}
	} else {
		if (len) {
			/* end of input, init packet as is */
			init_header_p(header, last_seqno++, len, PROTO_DATA);
			/* send */
			ASSERT(len + sizeof(struct proto_header));
			len = packet_len_p(packet);
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
	if (r == sizeof(header)) {
		if (PROTO_RETRANSM & header.flags) {
			/* mark packet in buffer with PROTO_DORETR flag */
			struct proto_packet *pack = packets_buf_get(header.seqno);
			if (pack) {
				pack->header.flags |= PROTO_DORETR;
			}
			/* else: ignore packet */
			// TODO what should we do with invalid retransmission request */
		}
		/* whatever we've done req->addr is still valid */
		if (PROTO_IDQUERY & header.flags) {
			/* NOTE: we don't want to fail here actually */
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

	for (int i = packets_buf_begin; i < packets_buf_end; i = (i+1) % packets_buf_cap) {
		struct proto_packet *packet = (struct proto_packet*) (packets_buf + i * packet_sz);

		if (packet->header.flags & PROTO_DORETR) {
			packet->header.flags &= ~PROTO_DORETR;
			EXPECT(write(mcast_sock, packet, packet_sz) == packet_sz,
					"Sending retransmission response failed.");
		}
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
				if (psize <= 0)
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

	if (errflg) {
		fprintf(stderr, "Usage: %s ... \n", argv[0]); // TODO
		exit(EXIT_FAILURE);
	}

	/* initialize packets buffer */
	packets_buf_cap = fsize / psize + ((fsize % psize) ? 1 : 0);
	packet_sz = sizeof(struct proto_header) + psize;
	packets_buf = (char *) malloc(packets_buf_cap * packet_sz);

	/* setup ctrl socket */
	local_addr.sin_family = AF_INET;
	local_addr.sin_family = htonl(INADDR_ANY);
	local_addr.sin_port = htons(ctrl_port);

	TRY_SYS(ctrl_sock = socket(PF_INET, SOCK_DGRAM, 0));
	TRY_SYS(bind(ctrl_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)));

	/* setup mcast socket */
	TRY_TRUE(sockaddr_dotted(&mcast_addr, mcast_dotted, data_port) == 1);
	TRY_SYS(mcast_sock = socket(PF_INET, SOCK_DGRAM, 0));
	{
		int optval = 1;
		TRY_SYS(setsockopt(mcast_sock, SOL_SOCKET, SO_BROADCAST,
					(void*) &optval, sizeof(optval)));
	}
	TRY_SYS(connect(mcast_sock, (struct sockaddr *) &mcast_addr, sizeof(mcast_addr)));

	/* better to keep such things precomputed due to no-copy policy */
	init_header(pack_my_ident.header, 0, data_len(pack_my_ident), PROTO_IDRESP);
	memcpy(&pack_my_ident.mcast_addr, &mcast_addr, sizeof(pack_my_ident.mcast_addr));
	strncpy(pack_my_ident.app_name, argv[0], sizeof(pack_my_ident.app_name) - 1);
	strncpy(pack_my_ident.tune_name, name, sizeof(pack_my_ident.tune_name) - 1);

	init_header(pack_ret_failed.header, 0, 0, PROTO_FAIL);

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

	exit(EXIT_SUCCESS);
}
