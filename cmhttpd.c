#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_EVENTS 128

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

int handle_http(int fd)
{
    char buf[4096];
    int n = 0;
    /* while((n=read(fd, buf, sizeof(buf))) != 0){ */
    /*     printf("%s", buf); */
    /*     write(fd, "Hello World", 11); */
    /*     if(-1 == n){ */
    /*         if(errno == EAGAIN || errno == EWOULDBLOCK){ */
    /*             break; */
    /*         }else{ */
    /*             perror("read"); */
    /*             close(fd); */
    /*             return -1; */
    /*         } */
    /*     } */
    /* } */
    read(fd, buf, sizeof(buf));
    printf("%s", buf);
    write(fd, "Hello World", 11);
    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    struct epoll_event evt, events[MAX_EVENTS];
    int epollfd, conn_sock, listen_sock;
    listen_sock = setup_server_socket(8000);
    
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

    int nfds;
    socklen_t client_addr_len;
    struct sockaddr_in client_addr;
    char chost[NI_MAXHOST], cport[NI_MAXSERV];
    for(;;){
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (-1 == nfds) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for(int i=0; i<nfds; ++i){
            if(events[i].events & EPOLLHUP || events[i].events & EPOLLERR){
                printf("DEBUG: close socket");
                close(events[i].data.fd);
                continue;
            }
            if(events[i].data.fd == listen_sock){
                conn_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
                if(-1 == conn_sock){
                    perror("accept");
                    exit(EXIT_FAILURE);
                }
                getnameinfo((struct sockaddr *)&client_addr, client_addr_len, chost, sizeof(chost), cport, sizeof(cport), NI_NUMERICHOST | NI_NUMERICSERV);
                printf("Accepted conn from %s:%s\n", chost, cport);
                setnonblocking(conn_sock);
                evt.events = EPOLLIN;
                evt.data.fd = conn_sock;
                if(-1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &evt)){
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            }else{
                handle_http(events[i].data.fd);
            }
        }
    }
    return 0;
}
