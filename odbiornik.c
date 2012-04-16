#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "err.h"
#include "common.h"
#include "proto.h"

/* receiver configuration */
char discover_dotted[ADDR_LEN] = DISCOVER_ADDR;
in_port_t data_port = DATA_PORT;
in_port_t ctrl_port = CTRL_PORT;
int ui_port = UI_PORT;
int bsize = BSIZE;
int rtime = RTIME;
char tune_name[NAME_LEN] = "";

/* receiver state */
struct sockaddr_in local_addr;
struct sockaddr_in discover_addr;

int discover_sock;


int main(int argc, char **argv) {
	/* parse options */
	{
		int errflg = 0;
		extern char *optarg;
		extern int optind, optopt;
		int c;
		while ((c = getopt(argc, argv, "d:P:C:b:R:n:")) != -1) {
			switch(c) {
				case 'd':
					strncpy(discover_dotted, optarg, sizeof(discover_dotted));
					break;
				case 'P':
					data_port = (in_port_t) atoi(optarg);
					break;
				case 'C':
					ctrl_port = (in_port_t) atoi(optarg);
					break;
				case 'b':
					bsize = atoi(optarg);
					break;
				case 'R':
					rtime = atoi(optarg);
					break;
				case 'n':
					strncpy(tune_name, optarg, sizeof(tune_name));
					break;
				default:
					errflg++;
					break;
			}
		}

		if (errflg) {
			fprintf(stderr, "Usage: %s ... \n", argv[0]); // TODO
			exit(EXIT_FAILURE);
		}
	}

	/* setup addresses  */
	local_addr.sin_family = AF_INET;
	local_addr.sin_family = htonl(INADDR_ANY);
	local_addr.sin_port = htons(0);

	TRY_TRUE(sockaddr_dotted(&discover_addr, discover_dotted, ctrl_port) == 1);

	/* setup sockets */
	TRY_SYS(discover_sock = socket(PF_INET, SOCK_DGRAM, 0));
	{
		int optval = 1;
		TRY_SYS(setsockopt(discover_sock, SOL_SOCKET, SO_BROADCAST,
					(void*) &optval, sizeof(optval)));
	}
	TRY_SYS(bind(discover_sock, (struct sockaddr *) &local_addr, sizeof(local_addr)));
	TRY_SYS(connect(discover_sock, (struct sockaddr *) &discover_addr,
				sizeof(discover_addr)));

	write(discover_sock, "bbb", 3);

	close(discover_sock);
	exit(EXIT_SUCCESS);
}
