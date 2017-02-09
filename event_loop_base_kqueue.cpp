#include "event_loop_base.h"

#include <sys/event.h>
#include <stdlib.h>

namespace mevent {

int EventLoopBase::Create() {
    return kqueue();
}

int EventLoopBase::Add(int evfd, int fd, int mask, void *data) {
    struct kevent ev;
    
    if (mask & MEVENT_IN) {
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, data);
        kevent(evfd, &ev, 1, NULL, 0, NULL);
    }
    
    if (mask & MEVENT_OUT) {
        EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, data);
        kevent(evfd, &ev, 1, NULL, 0, NULL);
    }
    
    return 0;
}

int EventLoopBase::Modify(int evfd, int fd, int mask, void *data) {
    return Add(evfd, fd, mask, data);
}
    
int EventLoopBase::Poll(int evfd, EventLoopBase::Event *events, int size, struct timeval *tv) {
    struct kevent evs[size];
    int nfds;
    
    if (tv) {
        struct timespec timeout;
        timeout.tv_sec = tv->tv_sec;
        timeout.tv_nsec = tv->tv_usec * 1000;
        
        nfds = kevent(evfd, NULL, 0, evs, size, &timeout);
    } else {
        nfds = kevent(evfd, NULL, 0, evs, size, NULL);
    }
    
    if (nfds > 0) {
        for (int i = 0; i < nfds; i++) {
            events[i].data.ptr = evs[i].udata;
            events[i].mask = 0;
            
            if (evs[i].filter == EVFILT_READ) {
                events[i].mask |= MEVENT_IN;
            }
            
            if (evs[i].filter == EVFILT_WRITE) {
                events[i].mask |= MEVENT_OUT;
            }
        }
    }
    
    return nfds;
}

}//namespace mevent