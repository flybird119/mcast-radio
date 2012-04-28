#ifndef __RECVBUFF_H
#define __RECVBUFF_H

#include "proto.h"

struct sendbuff {
	uint8_t *buff;

	int capacity;
	int begin;
	int end;
	int hpsize;
};

void sendbuff_init(struct sendbuff *sbuff, const int fsize, const int psize);
void sendbuff_free(struct sendbuff *sbuff);

struct proto_packet *sendbuff_getnth(struct sendbuff *sbuff, const int n);
struct proto_packet *sendbuff_getseqno(struct sendbuff *sbuff, const seqno_t seqno);

struct proto_packet *sendbuff_back(const struct sendbuff *sbuff);
void sendbuff_next(struct sendbuff *sbuff);

#endif /* __RECVBUFF_H */
