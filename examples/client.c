#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../reactor.h"

void mycb(apc_reactor *r, apc_event_watcher *w, unsigned int events){
    if(events & APC_POLLOUT){
        char *msg = "hallo welt!";
        ssize_t n = send(w->fd, msg, 12, 0);
        printf("%ld bytes written\n", n);
    }

    if(events & APC_POLLERR){
        apc_event_watcher_close(r, w);
        close(w->fd);
    }
}

int main(){
    apc_reactor reactor;
    apc_reactor_init(&reactor);
    apc_event_watcher watcher;

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int err = connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    assert(err == 0);
    apc_event_watcher_init(&watcher, mycb, sock);
    apc_event_watcher_register(&reactor, &watcher, APC_POLLOUT);

    apc_reactor_poll(&reactor, -1);

    apc_event_watcher_close(&reactor, &watcher);
    close(watcher.fd);
    apc_reactor_close(&reactor);
    return 0;
}