#ifndef _RAII_LOCK_GUARD_H
#define _RAII_LOCK_GUARD_H

#include <pthread.h>

namespace mevent {
    
class LockGuard {
public:
    LockGuard(pthread_mutex_t &mutex);
    ~LockGuard();
private:
    pthread_mutex_t &mtx;
};

}//namespace mevent

#endif
