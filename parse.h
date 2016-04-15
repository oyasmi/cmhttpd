#ifndef __PARSE
#define __PARSE

#include "cmhttpd.h"

void to_upper(char* str);
req_t *parse_req(char* buf);

int gen_resp(char* buf, int buflen, req_t* req);
int regulate_URI(char* URI);

typedef struct mime_s{
    char* ext;                  /* file extension */
    char* mime_type;
    struct mime_s* next;
} mime_t;

mime_t* new_mime_t(char* ext, char* mime_type, struct mime_s* next);
char* get_mime_type(char* URI);

#endif
