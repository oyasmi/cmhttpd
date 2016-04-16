#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
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
    char* resp_hdr = "%s %s\r\nServer: cmhttpd(pre-release, SJTU)\r\nContent-Type: %s\r\nConnection: %s\r\nContent-Length: %d\r\n\r\n";
    char *http_code, *keep_alive, *mime_type;
    if(req->http_code == 200)
        http_code = "200 OK";
    else if(req->http_code == 404)
        http_code = "404 Not Found";
    if(req->keep_alive)
        keep_alive = "keep-alive";
    else
        keep_alive = "close";
    mime_type = get_mime_type(req->URI);

    snprintf(buf, buflen, resp_hdr, req->protover, http_code, mime_type, keep_alive, req->file_len);

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

mime_t* new_mime_t(char* ext, char* mime_type, struct mime_s* next)
{
    mime_t* t = (mime_t*)malloc(sizeof(mime_t));
    int len = strlen(ext);
    t->ext = (char*)malloc(len+1);
    strncpy(t->ext, ext, len+1);
    len = strlen(mime_type);
    t->mime_type = (char*)malloc(len+1);
    strncpy(t->mime_type, mime_type, len+1);
    t->next = next;

    return t;
}

mime_t* build_mime_type_list(char* mime_type_filepath)
{
    mime_t* p = NULL;
    char buf[70000];
    int fd = open(mime_type_filepath, O_RDONLY);
    int n;
    if((n=read(fd, buf, 70000)) == -1){
        perror("error read");
        exit(EXIT_FAILURE);
    }
    close(fd);
    int i=0, j=0;
    char *ext, *mime_type;
    while(j <= n){
        if(buf[j] != '\n')
            ++j;
        else{
            buf[j] = '\0';
            if(buf[i] == '#'){
                ++j;
                i = j;
                continue;
            }
            mime_type = strtok(buf+i, " \t");
            while((ext = strtok(NULL, " \t"))){
                p = new_mime_t(ext, mime_type, p);
            }
            ++j;
            i = j;
        }
    }
    return p;
}

char* get_mime_type(char* URI)
{
    static mime_t* mime_type_list = NULL;
    if(!mime_type_list){
        /* p = new_mime_t("gz", "application/x-gzip", NULL); */
        /* p = new_mime_t("jpeg", "image/jpeg", p); */
        /* p = new_mime_t("jpg", "image/jpeg", p); */
        /* p = new_mime_t("png", "image/png", p); */
        /* p = new_mime_t("gif", "image/gif", p); */
        /* p = new_mime_t("txt", "text/plain", p); */
        /* p = new_mime_t("ico", "image/x-icon", p); */
        /* p = new_mime_t("ogv", "video/ogg", p); */
        /* p = new_mime_t("js", "application/x-javascript", p); */
        /* p = new_mime_t("css", "text/css", p); */
        /* p = new_mime_t("htm", "text/html", p); */
        /* p = new_mime_t("html", "text/html", p); */
        /* mime_type_list = p; */
        mime_type_list = build_mime_type_list("../mime.types");
    }
    
    
    int n = strlen(URI) - 1;
    char* ext;
    for(; n>=0; --n){
        if(URI[n] == '.')
            ext = URI + n + 1;
    }
    n = strlen(ext);
    int pos = 0;
    mime_t* p = mime_type_list;
    mime_t* q = p;
    while(p){
        if(strncmp(ext, p->ext, n) == 0){
            if(pos > 6)
            {
                q->next = p->next;
                p->next = mime_type_list;
                mime_type_list = p;
            }
            return p->mime_type;
        }
        else{
            q = p;
            p = p->next;
            ++pos;
        }
    }
    return "application/octet-stream";
}
