#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "proto.h"
#include "common.h"
#include "err.h"
#include "sendbuff.h"

void sendbuff_init(struct sendbuff *sbuff, const int fsize, const int psize) {
	sbuff->capacity = fsize / psize + ((fsize % psize) ? 1 : 0);
	sbuff->hpsize = sizeof(struct proto_header) + psize;
	sbuff->buff = (uint8_t *) malloc(sbuff->capacity * sbuff->hpsize);
}

void sendbuff_free(struct sendbuff *sbuff) {
	free(sbuff->buff);
}

struct proto_packet *sendbuff_getnth(struct sendbuff *sbuff, const int n) {
	int index = (sbuff->begin + n) % sbuff->capacity;
	if (sbuff->begin <= sbuff->end) {
		if (index < sbuff->begin || sbuff->end <= index)
			return NULL;
	} else {
		if (sbuff->end <= index && index < sbuff->begin)
			return NULL;
	}
	return (struct proto_packet *) (sbuff->buff + index * sbuff->hpsize);
}

struct proto_packet *sendbuff_getseqno(struct sendbuff *sbuff, const seqno_t seqno) {
	if (sbuff->begin == sbuff->end)
		return NULL;
	struct proto_header *header =
		(struct proto_header *) (sbuff->buff + sbuff->begin * sbuff->hpsize);
	int index = seqno - header_seqno(header);
	if (index < 0)
		return NULL;
	index = (sbuff->begin + index) % sbuff->capacity;
	header = (struct proto_header *) (sbuff->buff + index * sbuff->hpsize);
	if (seqno != header_seqno(header))
		return NULL;
	return (struct proto_packet *) header;
}

struct proto_packet *sendbuff_back(const struct sendbuff *sbuff) {
	return (struct proto_packet *) (sbuff->buff + sbuff->end * sbuff->hpsize);
}

void sendbuff_next(struct sendbuff *sbuff) {
	++sbuff->end;
	if (sbuff->end == sbuff->begin) {
		/* won't fire first time, when buffer size is 1 */
		++sbuff->begin;
		sbuff->begin %= sbuff->capacity;
	}
	sbuff->end %= sbuff->capacity;
}
