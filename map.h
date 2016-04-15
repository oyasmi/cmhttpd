#ifndef __MAP
#define __MAP

#include "cmhttpd.h"

conn_t *map_get(conn_t** map, int fd);
int map_put(conn_t** map, conn_t* conn);
int map_del(conn_t** map, int fd);
conn_t *new_conn(int fd);

#endif
