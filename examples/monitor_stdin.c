#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "../reactor.h"

void mycb(apc_reactor *r, apc_event_watcher *w, unsigned int events){
    if(events & APC_POLLIN){
        char buffer[256] = {0};
        ssize_t n = read(w->fd, buffer, 255);
        if(buffer[0] != 'q'){
            printf("read %ld bytes: %s\n", n, buffer);
            return;
        }
    }

    apc_event_watcher_close(r, w);
}

int main(){
    apc_reactor reactor;
    apc_reactor_init(&reactor);
    apc_event_watcher watcher;

    apc_event_watcher_init(&watcher, mycb, STDIN_FILENO);
    apc_event_watcher_register(&reactor, &watcher, APC_POLLIN);
    while(apc_reactor_is_active(&reactor)){
        printf("type something or quit with 'q'\n");
        apc_reactor_poll(&reactor, -1);
    }

    apc_event_watcher_close(&reactor, &watcher);
    apc_reactor_close(&reactor);
    return 0;
}