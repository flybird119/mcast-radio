#ifndef __STATIONS_H
#define __STATIONS_H

#include "common.h"

#define STATIONS_MAX 32
#define EXPIRY_INFTY (-1)
#define DISCOVER_KICK_THRESH 4

struct station_desc {
	char expiry_ticks;
	struct sockaddr_in mcast_addr;
	struct sockaddr_in local_addr;
	struct sockaddr_in ctrl_addr;
	char tune_name[NAME_LEN];
	len_t psize;
};

void station_desc_init(struct station_desc *st, struct proto_ident *packet,
		struct sockaddr_in *addr);

int stations_equal(struct station_desc *st, struct proto_ident *ident);

struct stations_list {
	struct station_desc stations[STATIONS_MAX];
	int capacity;
	int current;
};

void stations_list_init(struct stations_list *list);

struct station_desc *stations_list_new(struct stations_list *list);
struct station_desc *stations_list_current(struct stations_list *list);
struct station_desc *stations_list_get(struct stations_list *list, int index);
struct station_desc *stations_list_find(struct stations_list *list,
		struct proto_ident *ident);
void stations_list_set_current(struct stations_list *list,
		struct station_desc *station);

int stations_list_render(struct stations_list *list, char *buff, int buffsz);

char next_station(struct stations_list *list);
char prev_station(struct stations_list *list);

#endif /* __STATIONS_H */
