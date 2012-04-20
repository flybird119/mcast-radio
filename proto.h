#ifndef __PROTO_H
#define __PROTO_H

#include <netinet/in.h>

#include "common.h"

#define PROTO_VERSION 1

#define PROTO_MAX_SIZE (1<<16)

#define PROTO_RETRANSM 0x1
#define PROTO_IDQUERY 0x2
#define PROTO_IDRESP 0x4
#define PROTO_DATA 0x8
#define PROTO_FAIL 0xF
#define PROTO_DORETR 0x10

typedef uint32_t seqno_t;
typedef uint32_t len_t;
typedef uint8_t flags_t; /* byte order agnostic */

struct proto_header {
	seqno_t seqno;
	len_t length;
	flags_t flags;
	uint8_t  version;

	uint16_t __padding; /* padding */
}__attribute__((packed));

struct proto_ident {
	struct proto_header header;
	/* data */
	struct sockaddr_in mcast_addr;
	char app_name[NAME_LEN];
	char tune_name[NAME_LEN];
};

struct proto_packet {
	struct proto_header header;
	/* data */
	char data[];
};

/* NOTE: version has one byte so it's byte order agnostic */
#define check_version(header) ((header)->version == PROTO_VERSION)

void header_init(struct proto_header *header, seqno_t seqno, len_t len, flags_t flags);
void header_ident_init(struct proto_header *header, seqno_t seqno, flags_t flags);

seqno_t header_seqno(struct proto_header *header);

flags_t header_flag_isset(struct proto_header *header, flags_t flag);
void header_flag_set(struct proto_header *header, flags_t flag);
void header_flag_clear(struct proto_header *header, flags_t flag);

len_t packet_length(struct proto_packet *packet);

#endif /* __PROTO_H */
