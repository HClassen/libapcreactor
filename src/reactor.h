#ifndef APC_REACTOR_HEADER
#define APC_REACTOR_HEADER

#define APC_REACTOR_POLLIN  1
#define APC_REACTOR_POLLOUT 2

typedef struct apc_reactor_ apc_reactor;
typedef struct apc_event_watcher_ apc_event_watcher;

struct apc_reactor_{
    apc_event_watcher **event_watchers;
    int nwatchers;
    int nfds;
    int backend_fd;
    void *watcher_queue[2];
};

typedef void (*event_watcher_cb)(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
struct apc_event_watcher_{
    void *watcher_queue[2];
    int fd;
    unsigned int events;
    unsigned int registered;
    event_watcher_cb cb;
};

int apc_reactor_init(apc_reactor *reactor);
void apc_reactor_close(apc_reactor *reactor);
int apc_event_watcher_init(apc_event_watcher *w, event_watcher_cb cb, int fd);
void apc_event_watcher_register(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
void apc_event_watcher_deregister(apc_reactor *reactor, apc_event_watcher *w, unsigned int events);
void apc_event_watcher_close(apc_reactor *reactor, apc_event_watcher *w);
int apc_event_watcher_active(const apc_event_watcher *w, unsigned int events);
void apc_reactor_poll(apc_reactor *reactor, int timeout);

#endif