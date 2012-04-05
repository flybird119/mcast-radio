#ifndef __PROTO_H
#define __PROTO_H

#include <netinet/in.h>

#include "proto.h"

#define PROTO_MAXSIZE 64 * (1<<10)

#define PROTO_RET 0x1
#define PROTO_IDQ 0x2
#define PROTO_IDR 0x4
#define PROTO_DAT 0x8

struct proto_header {
	ssize_t seqno;
	ssize_t len;
	char flags;
};

struct proto_ident {
	struct proto_header header;
	struct sockaddr_in mcast_addr;
	char app_name[NAME_LEN];
	char tune_name[NAME_LEN];
};

struct proto_big {
	struct proto_header header;
	char data[PROTO_MAXSIZE];
};

#endif /* __PROTO_H */
