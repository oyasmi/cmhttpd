#ifndef __PARSE
#define __PARSE

#define VALUEBUFSIZE1 16
#define VALUEBUFSIZE2 256
#define RESPHDRBUFSIZE 1024

typedef struct req{
    char method[VALUEBUFSIZE1];
    char URI[VALUEBUFSIZE2];
    char protover[VALUEBUFSIZE1];
    short int keep_alive;
    short int http_code;
    long long file_len;
} req_t;

void to_upper(char* str);
req_t *parse_req(char* buf);

int gen_resp(char* buf, int buflen, req_t* req);

#endif
