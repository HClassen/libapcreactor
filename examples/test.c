#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "src/reactor.h"

void mycb(apc_reactor *r, apc_event_watcher *w, unsigned int events){
    if(events & APC_POLLIN)
        printf("callback\n");
}

int main(){
    apc_reactor reactor;
    apc_reactor_init(&reactor);
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = htons(8080);
    bind(sock, (struct sockaddr *) &addr, sizeof(addr));
    apc_event_watcher watcher;
    apc_event_watcher_init(&watcher, mycb, sock);
    apc_event_watcher_register(&reactor, &watcher, APC_POLLIN);

    time_t start = time(NULL);
    apc_reactor_poll(&reactor, 5000);
    time_t end = time(NULL);
    printf("diff: %ld\n", end-start);

    apc_event_watcher_close(&reactor, &watcher);
    apc_reactor_close(&reactor);
    return 0;
}