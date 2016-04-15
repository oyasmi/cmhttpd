#ifndef __CMHTTPD
#define __CMHTTPD

#define VALUEBUFSIZE1 16
#define VALUEBUFSIZE2 256
#define RESPHDRBUFSIZE 1024

#define REQBUFSIZE 4096
#define PATHBUFSIZE 256
#define MAP_SIZE 64

typedef struct req{
    int afd;
    char method[VALUEBUFSIZE1];
    char URI[VALUEBUFSIZE2];
    char protover[VALUEBUFSIZE1];
    short int keep_alive;
    short int http_code;
    long long file_len;
    long long write_pos;
} req_t;

typedef void* (*hdl_func_t) (void* conn);

typedef struct conn_entry{
    int fd;
    long read_pos;
    long read_len;
    char req_buf[REQBUFSIZE];
    char path[PATHBUFSIZE];
    hdl_func_t hdl_read;
    hdl_func_t hdl_write;
    struct req* req;
    struct conn_entry* next;
} conn_t;


#endif
