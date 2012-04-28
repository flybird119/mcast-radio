#ifndef __PROTO_H
#define __PROTO_H

#include <netinet/in.h>

#include "common.h"

#define PROTO_VERSION 1

#define PROTO_RETQUERY 0x1
#define PROTO_IDQUERY 0x2
#define PROTO_IDRESP 0x4
#define PROTO_DATA 0x8
#define PROTO_FAIL 0xF
#define PROTO_DORETR 0x10

/* NOTE: changing these values imply changing hton[sl] macros in proto.c */
typedef uint32_t seqno_t;
typedef uint16_t len_t;
typedef uint8_t flags_t; /* byte order agnostic */

struct proto_header {
	seqno_t seqno;
	len_t length;
	flags_t flags;
	uint8_t  version;

	uint8_t __padding; /* padding */
}__attribute__((packed));

#define PROTO_MAX_PACKET (1<<15)
#define PROTO_MAX_DATA ((1<<15)-sizeof(struct proto_header))

struct proto_ident {
	struct proto_header header;
	/* data */
	struct sockaddr_in mcast_addr;
	struct sockaddr_in local_addr;
	char tune_name[NAME_LEN];
	char app_name[NAME_LEN];
	len_t psize;
}__attribute__((packed));

struct proto_packet {
	struct proto_header header;
	/* data */
	char data[];
};

void header_init(struct proto_header *header, seqno_t seqno, len_t len, flags_t flags);
void ident_init(struct proto_ident *ident, seqno_t seqno, flags_t flags, len_t psize);

seqno_t header_seqno(struct proto_header *header);

flags_t header_flag_isset(struct proto_header *header, flags_t flag);
void header_flag_set(struct proto_header *header, flags_t flag);
void header_flag_clear(struct proto_header *header, flags_t flag);

len_t packet_length(struct proto_packet *packet);
len_t data_length(struct proto_packet *packet);

len_t ident_psize(struct proto_ident *packet);

char validate_header(struct proto_header *header);
char validate_packet(struct proto_packet *packet, ssize_t rlen);

char header_isident(struct proto_header *header);
char header_isempty(struct proto_header *header);
char header_isdata(struct proto_header *header);

#endif /* __PROTO_H */
