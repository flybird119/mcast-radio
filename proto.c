#include <string.h>

#include <netinet/in.h>

#include "proto.h"

_Static_assert(sizeof(union proto_addr) == PROTO_ADDR_SZ, "union proto_addr size invalid");

/* NOTE: version has one byte so it's byte order agnostic */
void header_init(struct proto_header *header, seqno_t seqno, len_t len, flags_t flags) {
	header->seqno = htonl(seqno);
	header->length = htons(len);
	header->flags = flags;
	header->version = PROTO_VERSION;
}

void ident_init(struct proto_ident *ident, struct sockaddr_in *mcast,
		struct sockaddr_in *local, len_t psize) {
	header_init(&ident->header, 0,
			(sizeof(struct proto_ident) - sizeof(struct proto_header)), PROTO_IDRESP);
	memcpy(&ident->mcast.addr, mcast, sizeof(*mcast));
	memcpy(&ident->local.addr, local, sizeof(*local));
	ident->psize = htons(psize);
}

seqno_t header_seqno(struct proto_header *header) {
	return ntohl(header->seqno);
}

/* NOTE: flags has one byte so it's byte order agnostic */
char header_flag_isonly(struct proto_header *header, flags_t flag) {
	return header->flags == flag;
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

len_t data_length(struct proto_packet *packet) {
	return ntohs(packet->header.length);
}

len_t packet_length(struct proto_packet *packet) {
	return sizeof(packet->header) + data_length(packet);
}

len_t ident_psize(struct proto_ident *packet) {
	return ntohs(packet->psize);
}

/* NOTE: version has one byte so it's byte order agnostic */
char validate_header(struct proto_header *header) {
	return header->version == PROTO_VERSION;
}

char validate_packet(struct proto_packet *packet, ssize_t rlen) {
	return validate_header(&packet->header) && (ssize_t) packet_length(packet) == rlen;
}

char header_isident(struct proto_header *header) {
	return packet_length((struct proto_packet *) header) == sizeof(struct proto_ident) && header_flag_isonly(header, PROTO_IDRESP);
}

char header_isempty(struct proto_header *header) {
	return packet_length((struct proto_packet *) header) == sizeof(struct proto_header);
}

char header_isdata(struct proto_header *header) {
	return header_flag_isonly(header, PROTO_DATA);
}
