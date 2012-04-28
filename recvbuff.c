#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "proto.h"
#include "common.h"
#include "err.h"
#include "recvbuff.h"

void packet_desc_init(struct packet_desc *desc) {
	desc->rcount = 0;
	desc->rdelay = 0;
	desc->length = 0; /* invalid packet */
}

void recvbuff_init(struct recvbuff *rbuff, const int bsize, const int psize) {
	ASSERT(bsize > 0 && psize >= 0);

	if (psize > 0) {
		rbuff->buff = realloc(rbuff->buff, bsize);

		rbuff->capacity = bsize / psize + ((bsize % psize) ? 1 : 0);
		rbuff->map = realloc(rbuff->map, sizeof(struct packet_desc) * rbuff->capacity);

		for (int i = 0; i < rbuff->capacity; ++i) {
			packet_desc_init(rbuff->map + i);
		}
	} else {
		rbuff->capacity = 0;
	}

	rbuff->psize = psize;

	rbuff->end = 0;
	rbuff->consistient = 0;
	rbuff->fseqno = 0;
}
// TODO remember to check against buffer overflow this will
// prevent fake station from receiving anything

void recvbuff_free(struct recvbuff *rbuff) {
	free(rbuff->buff);
	free(rbuff->map);

	rbuff->buff = NULL;
	rbuff->map = NULL;
}

int recvbuff_index(struct recvbuff *rbuff, seqno_t seqno) {
	int i = seqno - rbuff->fseqno;
	return i;
}

uint8_t *recvbuff_buf_get(const struct recvbuff *rbuff, const int index) {
	if (0 <= index && index < rbuff->capacity)
		return rbuff->buff + index * (int) rbuff->psize;
	else
		return NULL;
}

struct packet_desc *recvbuff_map_get(const struct recvbuff *rbuff, const int index) {
	if (0 <= index && index < rbuff->capacity)
		return rbuff->map + index;
	else
		return NULL;
}

void recvbuff_flush(struct recvbuff *rbuff, const int fd, const int pcount) {
	int i;
	/* write pcount packets from beginning of the buffer to file descriptor fd */
	if (fd >= 0) {
		for (i = 0; i < pcount; ++i) {
			struct packet_desc *d = rbuff->map + i;
			uint8_t *data = recvbuff_buf_get(rbuff, i); /* ugly */
			/* write */
			TRY_SYS(write(fd, data, d->length) == (int) d->length);
		}
	}
	/* determine how many packets left in buffer */
	int ncount = rbuff->end - pcount;
	ASSERT(ncount >= 0);
	/* shift buffer */
	memmove(rbuff->buff, rbuff->buff + pcount, ncount * rbuff->psize);
	/* remove written packets and reinit free slots in packets map */
	/* move part of packets map */
	memmove(rbuff->map, rbuff->map + pcount, ncount * sizeof(struct packet_desc));
	/* reinit slots after */
	for (i = ncount; i < rbuff->end; ++i) {
		packet_desc_init(rbuff->map + i);
	}
	/* udpate buffer end */
	rbuff->end -= pcount;
	ASSERT(rbuff->end == ncount);

	rbuff->consistient -= pcount;
	/* first seqno propagates */
	rbuff->fseqno += pcount;
}
