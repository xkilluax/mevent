#include "lock_guard.h"
#include "util.h"

namespace mevent {
    
LockGuard::LockGuard(pthread_mutex_t &mutex) : mtx(mutex) {
    if (pthread_mutex_lock(&mtx) != 0) {
        LOG_DEBUG(-1, NULL);
    }
}

LockGuard::~LockGuard() {
    if (pthread_mutex_unlock(&mtx) != 0) {
        LOG_DEBUG(-1, NULL);
    }
}

}//namespace mevent