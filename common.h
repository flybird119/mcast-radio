#ifndef __COMMON_H
#define __COMMON_H

#define UNUSED(x) ((void) (x))
#define SIZEOF(x) (sizeof(x) / sizeof(x[0]))

#define ADDR_LEN 32
#define NAME_LEN 32
#define ALBUMNO 305678
#define DISCOVER_ADDR "255.255.255.255"
#define DATA_PORT 20000 + (ALBUMNO % 10000)
#define CTRL_PORT 30000 + (ALBUMNO % 10000)
#define UI_PORT 10000 + (ALBUMNO % 10000)
#define PSIZE 512
#define BSIZE 64 * (1<<10)
#define FSIZE 128 * (1<<10)
#define RTIME 250

int sockaddr_dotted(struct sockaddr_in* sockaddr, char* dotted_addr,
		in_port_t port);

#endif /* __COMMON_H */
