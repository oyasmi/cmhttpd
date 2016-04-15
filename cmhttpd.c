#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include "cmhttpd.h"
#include "map.h"
#include "parse.h"

#define LISTEN_PORT 8000
#define MAX_EVENTS 128

int stop_flag = 0;
void sig_handler(int signum){
    stop_flag = 1;
}

conn_t* map[MAP_SIZE];
struct epoll_event events[MAX_EVENTS];
int epollfd;

int setnonblocking(int fd) {
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        perror("error setnonblocking");
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* setup a listen socket and return */
int setup_server_socket(int port) {
    signal(SIGPIPE, SIG_IGN);
    struct sockaddr_in server_addr;

    int listen_sock;
    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listen_sock) {
        fprintf(stderr, "error create listen socket\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret;
    ret = bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (-1 == ret) {
        fprintf(stderr, "error bind: %s\n", strerror(errno));
        return -1;
    }

    setnonblocking(listen_sock);

    int reuse = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (-1 == listen(listen_sock, 4096)) {
        fprintf(stderr, "error listen\n");
        return -1;
    }
    return listen_sock;
}

void *http_hdl(void* conn)
{
    static char* req_end = "\r\n\r\n";
    conn_t* c = (conn_t*)conn;
    int fd = c->fd;
    char* buf = c->req_buf+c->read_pos;
    int n = read(fd, buf, REQBUFSIZE - c->read_pos);
    if(n == 0){
        close(fd);
        map_del(map, fd);
        return NULL;
    }
    buf[n] = '\0';
    if(!strstr(buf, req_end)){
        if(c->read_pos+n < REQBUFSIZE-1){
            c->read_pos += n;
        }else{
            close(fd);
            map_del(map, fd);
            perror("req_buf overflow");
        }
        return NULL;
    }
    printf("%s", c->req_buf);

    c->req = parse_req(buf);
    req_t* req = c->req;

    regulate_URI(req->URI);
    char* filepath = req->URI + 1;

    struct stat fst;
    int ret = stat(filepath, &fst);
    if(ret == -1)
        req->http_code = 404;
    else{
        req->http_code = 200;
        req->file_len = fst.st_size;
    }
    char resp_hdr[RESPHDRBUFSIZE];
    gen_resp(resp_hdr, RESPHDRBUFSIZE, req);
    
    write(fd, resp_hdr, strlen(resp_hdr));
    if(req->http_code == 200){
        req->afd = open(filepath, O_RDONLY);
        if(req->afd == -1){
            perror("error open target file");
            exit(EXIT_FAILURE);
        }
        long bytes_sent = sendfile(fd, req->afd, (off_t *)&req->write_pos, req->file_len);
        if(-1 == bytes_sent && errno != EAGAIN){
            perror("error sendfile");
            req->keep_alive = 0;
        }else if(0 < bytes_sent){
            if(req->write_pos < req->file_len){
                struct epoll_event wevt;
                wevt.events = EPOLLOUT;
                wevt.data.fd = fd;
                if(-1 == epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &wevt)){
                    perror("epoll_ctl: mod EPOLLIN to EPOLLOUT");
                    exit(EXIT_FAILURE);
                }
                return NULL;
            }else{
                close(req->afd);
                free(c->req);
                c->req = NULL;
            }
        }
    }
    if(req->keep_alive){
        c->read_pos = 0;
    }else{
        close(fd);
        map_del(map, fd);
    }
    return NULL;
}

void *write_hdl(void* conn)
{
    conn_t* c = (conn_t*)conn;
    int fd = c->fd;
    req_t* req = c->req;

    long bytes_sent = sendfile(fd, req->afd, (off_t *)&req->write_pos, req->file_len - req->write_pos);
    if(-1 == bytes_sent && errno != EAGAIN){
        perror("error sendfile");
        close(fd);
        free(c->req);
        map_del(map, fd);
    }else if(0 < bytes_sent){
        if(req->write_pos < req->file_len){
            ;
        }else{
            close(req->afd);
            if(req->keep_alive){
                struct epoll_event revt;
                revt.events = EPOLLIN;
                revt.data.fd = fd;
                if(-1 == epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &revt)){
                    perror("epoll_ctl: mod EPOLLOUT to EPOLLIN");
                    exit(EXIT_FAILURE);
                }
                free(c->req);
                c->req = NULL;
            }else{
                close(fd);
                free(c->req);
                map_del(map, fd);
            }
        }
    }
    return NULL;
}

void *accept_hdl(void* conn)
{
    conn_t* c = (conn_t*)conn;
    int fd = c->fd;
    socklen_t client_addr_len;
    struct sockaddr_in client_addr;
    char chost[NI_MAXHOST], cport[NI_MAXSERV];
    int conn_sock = accept(fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if(-1 == conn_sock){
        perror("accept");
        exit(EXIT_FAILURE);
    }
    getnameinfo((struct sockaddr *)&client_addr, client_addr_len, chost, NI_MAXHOST, cport, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
    printf("Accepted conn from %s:%s\n", chost, cport);
    setnonblocking(conn_sock);
    struct epoll_event evt;
    evt.events = EPOLLIN;
    evt.data.fd = conn_sock;
    if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &evt)){
        perror("epoll_ctl: conn_sock");
        exit(EXIT_FAILURE);
    }
    conn_t* client_conn = new_conn(conn_sock);
    client_conn->hdl_read = http_hdl;
    client_conn->hdl_write = write_hdl;
    map_put(map, client_conn);

    return NULL;
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);
    chdir("www");
    for(int i=0; i<MAP_SIZE; ++i) map[i] = NULL;
    struct epoll_event evt;
    int listen_sock;
    listen_sock = setup_server_socket(LISTEN_PORT);
    
    epollfd = epoll_create1(0);
    if (-1 == epollfd){
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    evt.events = EPOLLIN;
    evt.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &evt) == -1){
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
    conn_t* conn = new_conn(listen_sock);
    conn->hdl_read = accept_hdl;
    map_put(map, conn);

    int nfds;
    for(;;){
        if(stop_flag)
            break;

        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 3000);
        if (-1 == nfds) {
            if(errno == EINTR)
                continue;
            else{
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }
        }

        for(int i=0; i<nfds; ++i){
            if(events[i].events & EPOLLHUP || events[i].events & EPOLLERR){
                printf("DEBUG: close socket");
                map_del(map, events[i].data.fd);
                close(events[i].data.fd);
                continue;
            }
            conn = map_get(map, events[i].data.fd);
            if(events[i].events & EPOLLIN){
                conn->hdl_read(conn);
            }else if(events[i].events & EPOLLOUT){
                conn->hdl_write(conn);
            }else{
                perror("unhandled event");
            }
        }
    }

    close(listen_sock);
    printf("\nserver closed successfully.\n");
    return 0;
}
