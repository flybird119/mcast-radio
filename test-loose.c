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

#include "common.h"
#include "err.h"
#include "proto.h"
#include "test-loose.h"

int skip[MAX_SEQNO];
struct event *loose_evt;

void loose_cb(evutil_socket_t sock, short ev, void *arg) {
	UNUSED(sock);
	UNUSED(ev);
	UNUSED(arg);

	int seqno, times;

	fscanf(stdin, "%d %d", &seqno, &times);
	getchar();
	if (feof(stdin)) {
		event_free(loose_evt);
	} else {
		loose_add(seqno, times);
	}
}

void loose_init(struct event_base *base) {
	TRY_TRUE(loose_evt = event_new(base, 0, EV_READ|EV_PERSIST, loose_cb, NULL));
	TRY_SYS(event_add(loose_evt, NULL));
}

void loose_add(seqno_t seqno, int times) {
	if (seqno < MAX_SEQNO) {
		skip[seqno] += times;
		fprintf(stderr, "seqno %d will be lost %d times\n", seqno, times);
	}
}

char loose_drop(struct proto_packet *packet) {
	int seqno = header_seqno(&packet->header);
	if (seqno < MAX_SEQNO && skip[seqno]) {
		--skip[seqno];
		fprintf(stderr, "loosing seqno %d\n", seqno);
		return 1;
	} else
		return 0;
}
