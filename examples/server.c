#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../reactor.h"

void readcb(apc_reactor *r, apc_event_watcher *w, unsigned int events){
    if(events & APC_POLLIN){
        char buffer[256] = {0};
        ssize_t n = recv(w->fd, buffer, 255, 0);
        printf("read %ld bytes:\n\t%s\n", n, buffer);
    }

    if(events & (APC_POLLERR | APC_POLLHUP)){
        apc_event_watcher_close(r, w);
        close(w->fd);
        free(w);
    }
}

void mycb(apc_reactor *r, apc_event_watcher *w, unsigned int events){
    if(events & APC_POLLIN){
        int client = accept(w->fd, NULL, NULL);
        assert(client > -1);
        apc_event_watcher *watcher = malloc(sizeof(apc_event_watcher));
        apc_event_watcher_init(watcher, readcb, client);
        apc_event_watcher_register(r, watcher, APC_POLLIN);
    }
}

int main(){
    apc_reactor reactor;
    apc_reactor_init(&reactor);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(8080);
    bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    listen(sock, 10);
    
    apc_event_watcher watcher;
    apc_event_watcher_init(&watcher, mycb, sock); 
    apc_event_watcher_register(&reactor, &watcher, APC_POLLIN);
    while(1){
        apc_reactor_poll(&reactor, -1);
    }
    
    apc_event_watcher_close(&reactor, &watcher);
    apc_reactor_close(&reactor);
    return 0;
}