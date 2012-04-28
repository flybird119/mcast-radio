#ifndef __CLIENTS_H
#define __CLIENTS_H

#include "proto.h"

#define CLIENTS_MAX 32

#define NO_SOCK (-1)
#define STATE_WAIT	0
#define STATE_READY1 2
#define STATE_READY2 4

struct client {
	int sock;
	char state;
};

void client_init(struct client *cl);
char client_isempty(const struct client *cl);
char client_isready(const struct client *cl);

struct clients_list {
	struct client list[CLIENTS_MAX];

	int capacity;
};

void clients_list_init(struct clients_list *list);

struct client *clients_list_find(struct clients_list *list, const int sock);
struct client *clients_list_new(struct clients_list *list);

void clients_list_kick(struct clients_list *list, const int sock);

#endif /* __CLIENTS_H */
