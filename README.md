# libapcreactor

libapcreactor is an implementation of the reactor pattern in C. It is used in my other project
[libapc](https://github.com/HClassen/libapc).

libapcreactor provides an uniformly interface for OS specific poll mechanisms. On Linux `epoll` is used, 
on FreeBSD/OpenBSD/NetBSD `kqueue` is used an on Solaris `dev/poll`. Other plattforms are not supported. 
However the implementation for Linux is best tested whereas `kqueue` is only tested on FreeBSD and the Solaris implementation isn't tested at all.

## Usage

A Makefile is provided to build the library as well as three example programms.

libapcreactor provides two struct definitions:
```C
    struct apc_reactor{
        apc_event_watcher **event_watchers;
        int nwatchers;
        int nfds;
        int backend_fd;
        void *watcher_queue[2];
    };
```
and
```C
    struct apc_event_watcher{
        int fd;
        unsigned int events;
        unsigned int registered;
        event_watcher_cb cb;
        void *watcher_queue[2];
    };
```
.

The `apc_reactor` is the core element, where `apc_event_watcher`s get registrated for specific events and their callbacks get called. `apc_event_watcher`s are associated with a file descriptor an have their callback called when the specified event(s) occur.
An `event_watcher_cb` is a function with the signiture
```C 
    void cb(apc_reactor *reactor, apc_event_watcher *w, unsigned int events)
```
.

The following four event types are provided an can be combined with an or:
```C
    APC_POLLIN,
    APC_POLLOUT,
    APC_POLLERR,
    APC_POLLHUP
```
.

Internally thy get converted to the poll mechanism specific flags.

On these basic types you shold only operate with the the folowing functions:
Initialize an `apc_reactor` with:
```C
    int apc_reactor_init(apc_reactor *reactor)
```
This function creates a backend file descripter and sets it to `close on exec`. 

Close an `apc_reactor` with:
```C
    void apc_reactor_close(apc_reactor *reactor)
```
No file descriptor other then the backend file descripter is closed during this function nor does memory used by `apc_event_watcher`s is freed.

The core function of this library:
```C
    void apc_reactor_poll(apc_reactor *reactor, int timeout);
```
The timeout parameter specifies a timeout in milliseconds. Passing -1 means an infinite timeout.

Initalize an `apc_event_watcher` with:
```C
    int apc_event_watcher_init(apc_event_watcher *w, event_watcher_cb cb, int fd)
```
The file descriptor passed needs to be valid (fd > -1).

Register an `apc_event_watcher`  for `events` with:
```C
    void apc_event_watcher_register(apc_reactor *reactor, apc_event_watcher *w, unsigned int events)
```

Degister an `apc_event_watcher`  for `events` with:
```C
    void apc_event_watcher_deregister(apc_reactor *reactor, apc_event_watcher *w, unsigned int events)
```

Close an `apc_event_watcher` with:
```C
    void apc_event_watcher_close(apc_reactor *reactor, apc_event_watcher *w)
```
The file descriptor associated with `w` isn't closed in this function and no memory is freed.

Returns wether `w` is registered for `events`
```C
    int apc_event_watcher_active(const apc_event_watcher *w, unsigned int events)
```

A macro to check if `reactor` has active `apc_event_watcher`s
```C
    apc_reactor_is_active(reactor)
```
`reactor` is expected to be a pointer.
