#include "event_loop_base.h"

#include <sys/epoll.h>
#include <stdio.h>
#include <pthread.h>

namespace mevent {

int EventLoopBase::Create() {
    return epoll_create(512);
}

int EventLoopBase::Add(int evfd, int fd, int mask, void *data) {
    struct epoll_event ev;
    ev.events = 0;

    if (mask & MEVENT_IN) {
        ev.events |= EPOLLIN;
    }

    if (mask & MEVENT_OUT) {
        ev.events |= EPOLLOUT;
    }
    
    ev.events |= EPOLLET;
    
    ev.data.ptr = data;
    
    return epoll_ctl(evfd, EPOLL_CTL_ADD, fd, &ev);
}
    
int EventLoopBase::Modify(int evfd, int fd, int mask, void *data) {
    struct epoll_event ev;
    ev.events = 0;
    
    if (mask & MEVENT_IN) {
        ev.events |= EPOLLIN;
    }
    
    if (mask & MEVENT_OUT) {
        ev.events |= EPOLLOUT;
    }
    
    ev.events |= EPOLLET;
    
    ev.data.ptr = data;
    
    return epoll_ctl(evfd, EPOLL_CTL_MOD, fd, &ev);
}

int EventLoopBase::Poll(int evfd, EventLoopBase::Event *events, int size, struct timeval *tv) {
    struct epoll_event evs[size];
    int nfds;

    if (tv) {
        nfds = epoll_wait(evfd, evs, size, (tv->tv_sec * 1000) + (tv->tv_usec / 1000));
    } else {
        nfds = epoll_wait(evfd, evs, size, -1);
    }
    
    if (nfds > 0) {
        for (int i = 0; i < nfds; i++) {
            events[i].data.ptr = evs[i].data.ptr;

            events[i].mask = 0;
            
            if (evs[i].events & EPOLLIN) {
                events[i].mask |= MEVENT_IN;
            }
            
            if (evs[i].events & EPOLLHUP) {
                events[i].mask |= MEVENT_OUT;
            }
            
            if (evs[i].events & EPOLLERR) {
                events[i].mask |= MEVENT_OUT;
            }
            
            if (evs[i].events & EPOLLOUT) {
                events[i].mask |= MEVENT_OUT;
            }
        }
    }
    
    return nfds;
}

}//namespace mevent