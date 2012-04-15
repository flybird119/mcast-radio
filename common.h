#ifndef __COMMON_H
#define __COMMON_H

// TODO
struct ring_buffer {
	char *mstart, *mend; /* first and after-last byte of undelying mem chunk */
	char *start, *end;
	ssize_t seqno; /* sequential number of byte at begin */
};

int sockaddr_dotted(struct sockaddr_in* sockaddr, char* dotted_addr,
		in_port_t port);

#endif /* __COMMON_H */
