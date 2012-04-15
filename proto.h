#ifndef __PROTO_H
#define __PROTO_H

#include <netinet/in.h>


#define ADDR_LEN 32
#define NAME_LEN 32
#define ALBUMNO 305678
#define DISCOVER_ADDR "255.255.255.255"
#define DATA_PORT 20000 + (ALBUMNO % 10000)
#define CTRL_PORT 30000 + (ALBUMNO % 10000)
#define UI_PORT 10000 + (ALBUMNO % 10000)
#define PSIZE 512
#define BSIZE 64 * (1<<10)
#define FSIZE 128 * (1<<10)
#define RTIME 250

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

struct proto_pack {
	struct proto_header header;
	char data[0];
};

#endif /* __PROTO_H */
