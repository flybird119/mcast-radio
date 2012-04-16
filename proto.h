#ifndef __PROTO_H
#define __PROTO_H

#include <netinet/in.h>

typedef uint32_t seqno_t;
typedef uint32_t len_t;

#define PROTO_RETRANSM 0x1
#define PROTO_IDQUERY 0x2
#define PROTO_IDRESP 0x4
#define PROTO_DATA 0x8
#define PROTO_FAIL 0xF

#define init_header(hdr, seq, ln, fl) \
	hdr.seqno = seq; \
	hdr.len = ln; \
	hdr.flags = fl; \

#define init_header_p(hdrp, seq, ln, fl) \
	hdrp->seqno = seq; \
	hdrp->len = ln; \
	hdrp->flags = fl; \

#define data_len(pack) \
	(sizeof(pack) - sizeof(pack.header))

#define data_len_p(pack) \
	(sizeof(*pack) - sizeof(*pack.header))

#define packet_len(pack) \
	(sizeof(pack.header) + pack.header.len)

#define packet_len_p(pack) \
	(sizeof(pack->header) + pack->header.len)

struct proto_header {
	seqno_t seqno;
	len_t len;
	uint32_t flags;
};

struct proto_ident {
	struct proto_header header;
	struct sockaddr_in mcast_addr;
	char app_name[NAME_LEN];
	char tune_name[NAME_LEN];
};

struct proto_packet {
	struct proto_header header;
	char data[];
};

#endif /* __PROTO_H */
