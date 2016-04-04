#include <stdlib.h>
#include "map.h"

conn_t *map_get(conn_t** map, int fd)
{
    int slot = fd % MAP_SIZE;
    conn_t* ptr = map[slot];
    while(ptr){
        if(ptr->fd == fd)
            return ptr;
        else
            ptr = ptr->next;
    }
    return NULL;
}
int map_set(conn_t** map, conn_t* conn)
{
    int slot = conn->fd % MAP_SIZE;
    conn->next = map[slot];
    map[slot] = conn;
    return 0;
}
int map_del(conn_t** map, int fd)
{
    int slot = fd % MAP_SIZE;
    conn_t* ptr = map[slot];
    conn_t* q = NULL;
    while(ptr){
        if(ptr->fd == fd){
            if(!q){
                map[slot] = ptr->next;
                free(ptr);
            }else{
                q->next = ptr->next;
                free(ptr);
            }
            return 0;
        }else{
            q = ptr;
            ptr = ptr->next;
        }
    }
    return -1;
}

conn_t *new_conn(int fd)
{
    conn_t* conn = (conn_t*)malloc(sizeof(conn_t));
    conn->fd = fd;
    conn->afd = -1;
    conn->read_pos = 0;
    conn->read_len = 0;
    conn->write_pos = 0;
    conn->file_len = 0;
    conn->next = NULL;
    return conn;
}
