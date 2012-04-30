#ifndef __RECVBUFF_H
#define __RECVBUFF_H

#include "proto.h"

struct packet_desc {
	char rcount; /* how many times we can ask for this packet */
	char rdelay; /* delay before next retransmission request (times RTIME) */
	len_t length; /* length must be > 0 to consider packet as valid */
};

void packet_desc_init(struct packet_desc *desc);

struct recvbuff {
	uint8_t *buff;
	len_t psize;

	struct packet_desc *map;
	int capacity;
	int end;
	int consistient;
	seqno_t fseqno;
};

void recvbuff_init(struct recvbuff *rbuff, const int bsize, const int psize);
void recvbuff_reset(struct recvbuff *rbuff);
void recvbuff_free(struct recvbuff *rbuff);

int recvbuff_seqno_dist(const struct recvbuff *rbuff, const seqno_t seqno);
int recvbuff_index(struct recvbuff *rbuff, seqno_t seqno);
uint8_t *recvbuff_buf_get(const struct recvbuff *rbuff, const int index);
struct packet_desc *recvbuff_map_get(const struct recvbuff *rbuff, const int index);

int recvbuff_mark_retrans(struct recvbuff *rbuff, int index, const char rdelay,
		const char rcount);
int recvbuff_update_consistient(struct recvbuff *rbuff);
void recvbuff_flush(struct recvbuff *rbuff, const int fd, int pcount);

#endif /* __RECVBUFF_H */
