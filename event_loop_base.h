#ifndef _EVENT_LOOP_BASE_H
#define _EVENT_LOOP_BASE_H

#include <sys/time.h>

namespace mevent {

#define MEVENT_IN    1
#define MEVENT_OUT   2
#define MEVENT_ERR   4

class EventLoopBase {
protected:
    struct Event {
        int    mask;
        union {
            int    fd;
            void  *ptr;
        } data;
    };

    int Create();
    int Add(int evfd, int fd, int mask, void *data);
    int Modify(int evfd, int fd, int mask, void *data);
    
    int Poll(int evfd, Event *events, int size, struct timeval *tv);
};

}//namespace mevent

#endif