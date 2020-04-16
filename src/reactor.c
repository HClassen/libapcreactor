#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
    #include <sys/epoll.h>
    static int conversion[] = {EPOLLIN, EPOLLOUT, EPOLLERR, EPOLLHUP};
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
    #include <sys/types.h>
    #include <sys/event.h>
    static int conversion[] = {EVFILT_READ, EVFILT_WRITE, EV_ERROR, EV_EOF};
#elif defined(sun) || defined(__sun)
    #if defined(__SVR4) || defined(__svr4__)
    #define solaris
    #include <sys/devpoll.h>
    #include <sys/ioctl.h>
    static int conversion[] = {POLLIN, POLLOUT, POLLERR, POLLHUP};
    #endif
#endif

#include "reactor.h"
#include "queue.h"

#define container_of(ptr, type, member) ((type *)  ((char *) ptr - offsetof(type, member)))
#define MAX_EVENTS 1024

#define update_timeout_(now, base, timeout)     \
	assert((timeout) > 0);                      \
	time_t diff = ((now)->tv_sec * 1000 + 		\
		(now)->tv_nsec / 1e+6) - 				\
		((base)->tv_sec * 1000 + 				\
		(base)->tv_nsec / 1e+6);				\
	(timeout) -= (int) diff;          			\
	if((timeout) <= 0){                         \
		return;                                 \
	}                                           \

static unsigned int convert(unsigned int events){
    unsigned int converted = 0;
    if(events & APC_POLLIN){
        converted |= conversion[0];
    }
    if(events & APC_POLLOUT){
        converted |= conversion[1];
    }
    if(events & APC_POLLERR){
        converted |= conversion[2];
    }
    if(events & APC_POLLHUP){
        converted |= conversion[3];
    }
    return converted;
}

static unsigned int restore(unsigned int events){
    unsigned int restored = 0;
    if(events & conversion[0]){
        restored |= APC_POLLIN;
    }
    if(events & conversion[1]){
        restored |= APC_POLLOUT;
    }
    if(events & conversion[2]){
        restored |= APC_POLLERR;
    }
    if(events & conversion[3]){
        restored |= APC_POLLHUP;
    }
    return restored;
}

static int set_non_blocking(int fd){
	int flags = fcntl(fd, F_GETFL, 0);
	if(flags == -1){
		return -1;
	}
    if(flags & O_NONBLOCK){
        return 0;
    }

	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int set_close_on_exec(int fd){
	int flags = fcntl(fd, F_GETFD, 0);
	if(flags == -1){
		return -1;
	}

	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static void maybe_resize(apc_reactor *reactor, int len){
	apc_event_watcher **watchers = NULL;

	if(len <= reactor->nwatchers){
		return;
	}

	int nwatchers = len + 10;
	watchers = /*apc_*/realloc(reactor->event_watchers, nwatchers * sizeof(apc_event_watcher));

	if(watchers == NULL){
		abort();
	}

	reactor->event_watchers = watchers;

	for(int i = reactor->nwatchers; i<nwatchers; i++){
		reactor->event_watchers[i] = NULL;
	}

	reactor->nwatchers = nwatchers;
}

static int create_backend_fd(){
    int pollfd = -1;
#if defined(__linux__) 
	pollfd = epoll_create1(EPOLL_CLOEXEC);
    if(pollfd > 0){
        return pollfd;
    }
    if(pollfd == -1 && errno == ENOSYS){
        pollfd = epoll_create(256);
    }
#elif defined(__NetBSD__)
    pollfd = kqueue1(O_CLOEXEC);
    if(pollfd > 0){
        return pollfd;
    }
    if(pollfd == -1 && errno == ENOSYS){
        pollfd = kqueue();
    }
#elif defined(__OpenBSD__) || defined(__FreeBSD__)
	pollfd = kqueue();
#elif defined(solaris)
    pollfd = open("/dev/poll", O_RDWR);
#endif
    if(pollfd > -1){
        int err = set_close_on_exec(pollfd);
        if(err != 0){
            close(pollfd);
            pollfd = -1;
        }
    }
	return pollfd;
}

int apc_reactor_init(apc_reactor *reactor){
    assert(reactor != NULL);

    reactor->event_watchers = NULL;
    reactor->nwatchers = 0;
    reactor->nfds = 0;
    reactor->backend_fd = create_backend_fd();
    QUEUE_INIT(&reactor->watcher_queue);
	return -(reactor->backend_fd < 0);
}

void apc_reactor_close(apc_reactor *reactor){
	assert(reactor != NULL);

	if(reactor->backend_fd != -1){
		close(reactor->backend_fd);
		reactor->backend_fd = -1;
	}
	if(reactor->event_watchers != NULL){
		/*apc_*/free(reactor->event_watchers);
		reactor->event_watchers = NULL;
	}
	reactor->nwatchers = 0;
	reactor->nfds = 0;
	QUEUE_INIT(&reactor->watcher_queue);
}

int apc_event_watcher_init(apc_event_watcher *w, event_watcher_cb cb, int fd){
	assert(cb != NULL);
	assert(fd > -1);
	QUEUE_INIT(&w->watcher_queue);

	w->cb = cb;
	w->fd = fd;
	w->events = 0;
	w->registered = 0;
    int err = set_non_blocking(fd);
    if(err != 0){
        w->fd = -1;
        return -1;
    }
	return 0;
}

void apc_event_watcher_register(apc_reactor *reactor, apc_event_watcher *w, unsigned int events){
	assert(0 == (events & ~(APC_POLLIN | APC_POLLOUT)));
	assert(0 != events);
	assert(w->fd >= 0);
	assert(w->fd < INT_MAX);

	w->events |= convert(events);
	maybe_resize(reactor, w->fd + 1);

	if(QUEUE_EMPTY(&w->watcher_queue)){
		QUEUE_ADD_TAIL(&reactor->watcher_queue, &w->watcher_queue);
	}

	if(reactor->event_watchers[w->fd] == NULL){
		reactor->event_watchers[w->fd] = w;
		reactor->nfds += 1;
	}
}

void apc_event_watcher_deregister(apc_reactor *reactor, apc_event_watcher *w, unsigned int events){
	assert(0 == (events & ~(APC_POLLIN | APC_POLLOUT)));
  	assert(0 != events);

	if(w->fd == -1){
		return;
	}

	assert(w->fd >= 0);

	if(w->fd >= reactor->nwatchers){
		return;
	}

	w->events &= ~convert(events);

	if(w->events == 0){
		QUEUE_REMOVE(&w->watcher_queue);
		QUEUE_INIT(&w->watcher_queue);

		if(reactor->event_watchers[w->fd] != NULL){
			assert(reactor->event_watchers[w->fd] == w);
			assert(reactor->nfds > 0);
			reactor->event_watchers[w->fd] = NULL;
			reactor->nfds -= 1;
			w->registered = 0;
		}
	}else if(QUEUE_EMPTY(&w->watcher_queue)){
		QUEUE_ADD_TAIL(&reactor->watcher_queue, &w->watcher_queue);
	}
}

static void invalidate_fd(apc_reactor *reactor, int fd){
    assert(reactor->event_watchers != NULL);
    assert(fd >= 0);

    if(reactor->backend_fd >= 0){
#if defined(__linux__)
        struct epoll_event dummy = {0};
        epoll_ctl(reactor->backend_fd, EPOLL_CTL_DEL, fd, &dummy);
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
		struct kevent dummy = {0};
		EV_SET(&dummy, fd, 0, EV_DELETE, 0, 0, 0);
		kevent(reactor->backend_fd, &dummy, 1, NULL, 0, NULL);
#elif defined(solaris)
        struct pollfd dummy = {fd, POLLREMOVE, 0};
        write(reactor->backend_fd, &dummy, sizeof(struct pollfd));
#endif
    }
}

void apc_event_watcher_close(apc_reactor *reactor, apc_event_watcher *w){
	apc_event_watcher_deregister(reactor, w, APC_POLLIN | APC_POLLOUT);

	if(w->fd >= 0){
		invalidate_fd(reactor, w->fd);
	}
}

int apc_event_watcher_active(const apc_event_watcher *w, unsigned int events){
  assert(0 == (events & ~(APC_POLLIN | APC_POLLOUT)));
  assert(0 != events);
  return 0 != (w->events & convert(events));
}

void apc_reactor_poll(apc_reactor *reactor, int timeout){
	assert(reactor != NULL);
	assert(timeout >= -1);
	assert(reactor->event_watchers != NULL);
    assert(reactor->backend_fd > -1);

    if(reactor->nfds == 0){
        assert(QUEUE_EMPTY(&reactor->watcher_queue));
        return;
    }
    
    queue *q = NULL;
    apc_event_watcher* w = NULL;
#if defined(__linux__)
    struct epoll_event e = (struct epoll_event) {0};
    int op;
    while(!QUEUE_EMPTY(&reactor->watcher_queue)){
        q = QUEUE_NEXT(&reactor->watcher_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        w = container_of(q, apc_event_watcher, watcher_queue);
        assert(w->fd >= 0);
        assert(w->events != 0);
        assert(w->fd < reactor->nwatchers);

        e.events = w->events;
        e.data.fd = w->fd;
 
        if(w->registered == 0){
            op = EPOLL_CTL_ADD;
        }else{
            op = EPOLL_CTL_MOD;
        }

        if(epoll_ctl(reactor->backend_fd, op, w->fd, &e)){
            if(errno != EEXIST){
                abort();
            }

            assert(op == EPOLL_CTL_ADD);

            if (epoll_ctl(reactor->backend_fd, EPOLL_CTL_MOD, w->fd, &e)){
                abort();
            }
        }

        w->registered = 1;
    }
    
	struct timespec base;
	clock_gettime(CLOCK_REALTIME, &base);
    struct epoll_event events[MAX_EVENTS];
	int count = 32;
    while(count-- > 0){
        int nfds = epoll_wait(reactor->backend_fd, events, MAX_EVENTS, timeout);
        struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
        if(nfds == 0) {
            assert(timeout != -1);
            if(timeout == 0){
                return;
            }
            
            update_timeout_(&now, &base, timeout)
            continue;
        }
        if(nfds == -1){
            if(errno != EINTR){
                abort();
            }

            if(timeout == -1){
                continue;
            }

            if(timeout == 0){
                return;
            }

            update_timeout_(&now, &base, timeout)
            continue;
        }

        int nevents = 0;
        for(int i = 0; i < nfds; i++){
            struct epoll_event *pe = events + i;
            int fd = pe->data.fd;

            if(fd == -1){
                continue;
            }

            assert(fd >= 0);
            assert(fd < reactor->nwatchers);

            w = reactor->event_watchers[fd];
            if(w == NULL){
                epoll_ctl(reactor->backend_fd, EPOLL_CTL_DEL, fd, pe);
                continue;
            }

            pe->events &= w->events | EPOLLERR | EPOLLHUP;
            if(pe->events != 0){
                w->cb(reactor, w, restore(pe->events));
            }
            nevents += 1;
        }

        if(nevents != 0){
            if(nfds == MAX_EVENTS){
                timeout = 0;
                continue;
            }
            return;
        }
        if(timeout == 0){
            return;
        }
        if(timeout == -1){
            continue;
        }
    }

#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__)
    struct kevent e = {0};
    while(!QUEUE_EMPTY(&reactor->watcher_queue)){
        q = QUEUE_NEXT(&reactor->watcher_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);
        w = container_of(q, apc_event_watcher, watcher_queue);
        assert(w->fd >= 0);
        assert(w->events != 0);
        assert(w->fd < reactor->nwatchers);

        EV_SET(&e, w->fd, w->events, EV_ADD, 0, 0, 0);
        if(kevent(reactor->backend_fd, &e, 1, NULL, 0, NULL) == -1){
            if(errno != EEXIST){
                abort();
            }
        }
        w->registered = 1;
    }

	struct timespec base, maybe_timer = {0, 0}, *timer = NULL;
	clock_gettime(CLOCK_REALTIME, &base);
    struct kevent events[MAX_EVENTS];
    int count = 32;
    while(count-- > 0){
        if(timeout > -1){
			maybe_timer.tv_sec = timeout / 1000;
			maybe_timer.tv_nsec = (timeout % 1000) * 1e+6;
			timer = &maybe_timer;
		}
		int nfds = kevent(reactor->backend_fd, NULL, 0, events, MAX_EVENTS, timer);
        if (nfds == 0) {
            assert(timeout != -1);
            if (timeout == 0){
                return;
            }
        }
        if(nfds == -1){
            if(errno != EINTR){
                abort();
            }

            if(timeout == -1){
                continue;
            }

            if(timeout == 0){
                return;
            }
        }

		int nevents = 0;
		for(int i = 0; i<nfds; i++){
			struct kevent *pe = events + i; 
			int fd = pe->ident;

            if(fd == -1){
                continue;
            }

            assert(fd >= 0);
            assert(fd < reactor->nwatchers);

            w = reactor->event_watchers[fd];
            if(w == NULL){
				struct kevent dummy;
				EV_SET(&dummy, fd, 0, EV_DELETE, 0, 0, 0);
				kevent(reactor->backend_fd, &dummy, 1, NULL, 0, NULL);
                continue;
            }

            int filter = (pe->filter & w->events) | (pe->flags & (EV_ERROR | EV_EOF));
            if(filter != 0){
                w->cb(reactor, w, restore(filter));
            }
            nevents += 1;
		}
        
		if(nevents != 0){
            if(nfds == MAX_EVENTS){
                timeout = 0;
                continue;
            }
            return;
        }
        if(timeout == 0){
            return;
        }
        if(timeout == -1){
            continue;
        }
    }
#elif defined(solaris)
    struct pollfd e = (struct pollfd) {0};
    int op;
    while(!QUEUE_EMPTY(&reactor->watcher_queue)){
        q = QUEUE_NEXT(&reactor->watcher_queue);
        QUEUE_REMOVE(q);
        QUEUE_INIT(q);

        w = container_of(q, apc_event_watcher, watcher_queue);
        assert(w->fd >= 0);
        assert(w->events != 0);
        assert(w->fd < reactor->nwatchers);
        
        e.fd = w->fd;
        e.events = w->events;
        e.revents = 0;
 
        if(w->registered == 0){
            op = EPOLL_CTL_ADD;
        }else{
            op = EPOLL_CTL_MOD;
        }

        if(write(reactor->backend_fd, &e, sizeof(struct pollfd)) != sizeof(struct pollfd)){
            abort();
        }

        w->registered = 1;
    }
    
    struct timespec base;
	clock_gettime(CLOCK_REALTIME, &base);
    struct pollfd events[MAX_EVENTS];
    struct dvpoll dvpoll;
	int count = 32;
    while(count-- > 0){
        dvpoll = {events, MAX_EVENTS, timeout};
        int nfds = ioctl(reactor->backend_fd, DP_POLL, &dvpoll);
        struct timespec now;
		clock_gettime(CLOCK_REALTIME, &now);
        if(nfds == 0) {
            assert(timeout != -1);
            if(timeout == 0){
                return;
            }
            
            update_timeout_(&now, &base, timeout)
            continue;
        }
        if(nfds == -1){
            if(errno != EINTR){
                abort();
            }

            if(timeout == -1){
                continue;
            }

            if(timeout == 0){
                return;
            }

            update_timeout_(&now, &base, timeout)
            continue;
        }

        int nevents = 0;
        for(int i = 0; i < nfds; i++){
            struct pollfd *pe = events + i;
            int fd = pe->data.fd;

            if(fd == -1){
                continue;
            }

            assert(fd >= 0);
            assert(fd < reactor->nwatchers);

            w = reactor->event_watchers[fd];
            if(w == NULL){
                struct pollfd dummy = {fd, POLLREMOVE, 0};
                write(reactor->backend_fd, &dummy, sizeof(struct pollfd))
                continue;
            }

            pe->revents &= w->events | POLLERR | POLLHUP;
            if(pe->events != 0){
                w->cb(reactor, w, restore(pe->events));
            }
            nevents += 1;
        }

        if(nevents != 0){
            if(nfds == MAX_EVENTS){
                timeout = 0;
                continue;
            }
            return;
        }
        if(timeout == 0){
            return;
        }
        if(timeout == -1){
            continue;
        }
    }
#endif
}