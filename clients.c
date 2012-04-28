#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "clients.h"

void client_init(struct client *cl) {
	cl->sock = NO_SOCK;
	cl->state = STATE_WAIT;
}

char client_isempty(const struct client *cl) {
	return cl->sock == NO_SOCK;
}

char client_isready(const struct client *cl) {
	return (cl->state & STATE_READY1) && (cl->state & STATE_READY2);
}

void clients_list_init(struct clients_list *list) {
	list->capacity = SIZEOF(list->list);

	for (int i = 0; i < list->capacity; ++i) {
		client_init(list->list + i);
	}
}

struct client *clients_list_find(struct clients_list *list, const int sock) {
	int i;
	for (i = 0; i < list->capacity; ++i)
		if (list->list[i].sock == sock)
			return list->list + i;
	return NULL;
}

struct client *clients_list_new(struct clients_list *list) {
	return clients_list_find(list, NO_SOCK);
}

void clients_list_kick(struct clients_list *list, const int sock) {
	struct client *cl = clients_list_find(list, sock);
	if (cl)
		client_init(cl);
}
