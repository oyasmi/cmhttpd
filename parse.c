#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"

req_t *parse_req(char* buf)
{
    static char newline[3] = "\n";
    req_t* req = (req_t*)malloc(sizeof(req_t));
    char* line;
    line = strtok(buf, newline);

    char *p=line, *q=line;
    while(*p != ' ' && *p != '\0') ++p;
    *p = '\0';
    strncpy(req->method, q, VALUEBUFSIZE1);
    ++p;
    q = p;
    while(*p != ' ' && *p != '\0') ++p;
    *p = '\0';
    strncpy(req->URI, q, VALUEBUFSIZE2);
    ++p;
    q = p;
    strncpy(req->protover, q, VALUEBUFSIZE1);
    p = req->protover;
    while(*p != '\r' && *p != '\0') ++p;
    if(*p == '\r')
        *p = '\0';

    to_upper(req->method);
    to_upper(req->protover);
    
    if(strncmp("HTTP/1.1", req->protover, 8) == 0)
        req->keep_alive = 1;    /* true */
    else
        req->keep_alive = 0;
    while((line=strtok(NULL, newline))){
        to_upper(line);
        if(strstr(line, "CONNECTION:")){
            if(strstr(line, "KEEP-ALIVE"))
                req->keep_alive = 1;
            else if(strstr(line, "CLOSE"))
                req->keep_alive = 0;
        }
    }
    req->http_code = 403;
    req->file_len = 0;
    req->write_pos = 0;
    req->afd = -1;
    return req;
}

void to_upper(char* str)
{
    char *p = str;
    while(*p != '\0'){
        if(*p >= 'a' && *p <='z')
            *p = *p - ('a'-'A');
        ++p;
    }
}

int gen_resp(char* buf, int buflen, req_t* req)
{
    char* resp_hdr;
    if(req->http_code == 200){
        resp_hdr = "%s 200 OK\r\nServer: cmhttpd\r\nContent-Type: text/html; charset=UTF-8\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n";
        snprintf(buf, buflen, resp_hdr, req->protover, req->file_len);
    }else if(req->http_code == 404){
        resp_hdr = "%s 404 Not Found\r\nServer:cmhttpd\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n";
        snprintf(buf, buflen, resp_hdr, req->protover, 0);
    }
    return 0;
}

int regulate_URI(char* URI)
{
    int len = strlen(URI);
    char buf[VALUEBUFSIZE2];

    int i=0, j=0;
    while(URI[i] != '\0'){
        if(URI[i] == '.'){
            if(URI[i+1] == '.' || URI[i+1] == '/'){
                ++i;
                continue;
            }
        }

        buf[j++] = URI[i++];
    }
    if(buf[j-1] == '/'){
        strncpy(buf+j, "/index.html", 12);
    }else{
        buf[j] = '\0';
    }
    len = strlen(buf);
    strncpy(URI, buf, len+1);
    
    return 0;
}
