#include <netinet/in.h>

#include "proto.h"

/* NOTE: version has one byte so it's byte order agnostic */
void header_init(struct proto_header *header, seqno_t seqno, len_t len, flags_t flags) {
	header->seqno = htonl(seqno);
	header->length = htonl(len);
	header->flags = flags;
	header->version = PROTO_VERSION;
}

void header_ident_init(struct proto_header *header, seqno_t seqno, flags_t flags) {
	header_init(header, seqno, 0, flags);
	header->length = sizeof(struct proto_ident);
}

seqno_t header_seqno(struct proto_header *header) {
	return ntohl(header->seqno);
}

/* NOTE: flags has one byte so it's byte order agnostic */
flags_t header_flag_isset(struct proto_header *header, flags_t flag) {
	return header->flags & flag;
}

/* NOTE: flags has one byte so it's byte order agnostic */
void header_flag_set(struct proto_header *header, flags_t flag) {
	header->flags |= flag;
}

/* NOTE: flags has one byte so it's byte order agnostic */
void header_flag_clear(struct proto_header *header, flags_t flag) {
	header->flags &= ~flag;
}

len_t packet_length(struct proto_packet *packet) {
	return sizeof(packet->header) + ntohl(packet->header.length);
}
