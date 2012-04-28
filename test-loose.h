#ifndef __LOOSE_H
#define __LOOSE_H

#include <event2/event.h>

#include "proto.h"

#define MAX_SEQNO 1024

void loose_cb(evutil_socket_t sock, short ev, void *arg);

void loose_init(struct event_base *base);

void loose_add(seqno_t seqno, int times);
char loose_drop(struct proto_packet *packet);

#endif /* __LOOSE_H */
