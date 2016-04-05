#ifndef __MAP
#define __MAP

#define REQBUFSIZE 4096
#define PATHBUFSIZE 256
#define MAP_SIZE 64

typedef void* (*hdl_func_t) (void* conn);

typedef struct conn_entry{
    int fd;
    int afd;
    unsigned int read_pos;
    unsigned int read_len;
    unsigned int write_pos;
    long long file_len;
    char req_buf[REQBUFSIZE];
    char path[PATHBUFSIZE];
    hdl_func_t hdl_read;
    hdl_func_t hdl_write;
    struct conn_entry* next;
} conn_t;

/*conn_t* map[MAP_SIZE];*/

conn_t *map_get(conn_t** map, int fd);
int map_set(conn_t** map, conn_t* conn);
int map_del(conn_t** map, int fd);
conn_t *new_conn(int fd);

#endif
