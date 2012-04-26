#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "common.h"
#include "proto.h"
#include "stations.h"

void station_desc_init(struct station_desc *st, struct proto_ident *packet, struct sockaddr_in *addr) {
	st->expiry_ticks = DISCOVER_KICK_THRESH;
	memcpy(&st->mcast_addr, &packet->mcast_addr, sizeof(st->mcast_addr));
	memcpy(&st->local_addr, &packet->local_addr, sizeof(st->local_addr));
	memcpy(&st->ctrl_addr, addr, sizeof(st->ctrl_addr));
	strncpy(st->tune_name, packet->tune_name, sizeof(st->tune_name) - 1);
	st->psize = ident_psize(packet);
}

int stations_equal(struct station_desc *st, struct proto_ident *ident) {
	/* compare everything you know */
	return (memcmp(&st->mcast_addr, &ident->mcast_addr, sizeof(st->mcast_addr)) == 0)
		&& (memcmp(&st->local_addr, &ident->local_addr, sizeof(st->local_addr)) == 0)
		&& (st->local_addr.sin_addr.s_addr == ident->local_addr.sin_addr.s_addr)
		&& (strcmp(st->tune_name, ident->tune_name) == 0)
		&& (st->psize = ident_psize(ident));
}

void stations_list_init(struct stations_list *list) {
	list->capacity = SIZEOF(list->stations);
	list->current = 0;

	/* create fake station */
	memset(list->stations, 0, sizeof(struct station_desc));
	list->stations[0].expiry_ticks = EXPIRY_INFTY;
}

struct station_desc *stations_list_new(struct stations_list *list) {
	int i;
	for (i = 0; i < list->capacity; ++i)
		if (list->stations[i].expiry_ticks == 0)
			break;
	if (i < list->capacity)
		return (list->stations + i);
	else
		return NULL;
}

struct station_desc *stations_list_current(struct stations_list *list) {
	return stations_list_get(list, list->current);
}

struct station_desc *stations_list_get(struct stations_list *list, int index) {
	if (index >= 0 && index < list->capacity)
		return list->stations + index;
	else
		return NULL;
}

struct station_desc *stations_list_find(struct stations_list *list,
		struct proto_ident *ident) {
	int i;
	for (i = 0; i < list->capacity; ++i)
		if (list->stations[i].expiry_ticks
				&& stations_equal(list->stations + i, ident))
			break;
	if (i < list->capacity)
		return list->stations + i;
	else
		return NULL;
}

void stations_list_set_current(struct stations_list *list,
		struct station_desc *station) {
	list->current = station - list->stations;
	if (list->current < 0 || list->current > list->capacity)
		list->current = 0;
}

int stations_list_render(struct stations_list *list, char *buff, int buffsz) {
	int n = 0;
	n += snprintf(buff + n, buffsz - n,
			"+------------------------------Znalezione stacje:------------------------------+\r\n|\r\n");
	/* print stations list */
	n += snprintf(buff + n, buffsz - n, "| %c WYLACZONY \r\n",
			(list->current == 0) ? '>' : ' ');
	for (int i = 1; i < list->capacity; ++i) {
		struct station_desc *st = list->stations + i;
		if (st->expiry_ticks) {
			n += snprintf(buff + n, buffsz - n,
					"| %c %d %s\r\n",
					(list->current == i) ? '>' : ' ',
					i,
					st->tune_name);
			/* additional info for current station */
			if (list->current == i) {
				n += snprintf(buff + n, buffsz - n,
						"|\t psize %d \t\t\t local %s:%d\r\n",
						st->psize,
						inet_ntoa(st->local_addr.sin_addr),
						ntohs(st->local_addr.sin_port));
				n += snprintf(buff + n, buffsz - n,
						"|\t mcast %s:%d ",
						inet_ntoa(st->mcast_addr.sin_addr),
						ntohs(st->mcast_addr.sin_port));
				n += snprintf(buff + n, buffsz - n,
						"\t ctrl %s:%d\r\n",
						inet_ntoa(st->ctrl_addr.sin_addr),
						ntohs(st->ctrl_addr.sin_port));
			}
		}
	}
	n += snprintf(buff + n, buffsz - n, "+------------------------------------------------------------------------------+\r\n");

	return n;
}

char next_station(struct stations_list *list) {
	int i = list->current;
	do {
		i = (i+1) % list->capacity;
	} while (list->stations[i].expiry_ticks == 0 && i != list->current);

	if (i == list->current) {
		return 0;
	} else {
		list->current = i;
		return 1;
	}
}

char prev_station(struct stations_list *list) {
	int i = list->current;
	do {
		--i;
		if (i < 0)
			i = list->capacity - 1;
	} while (list->stations[i].expiry_ticks == 0 && i != list->current);

	if (i == list->current) {
		return 0;
	} else {
		list->current = i;
		return 1;
	}
}
