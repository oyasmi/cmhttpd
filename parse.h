#ifndef __PARSE
#define __PARSE

#include "cmhttpd.h"

void to_upper(char* str);
req_t *parse_req(char* buf);

int gen_resp(char* buf, int buflen, req_t* req);
int regulate_URI(char* URI);

#endif
