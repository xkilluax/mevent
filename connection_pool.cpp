#include "connection_pool.h"
#include "util.h"
#include "lock_guard.h"

namespace mevent {
    
ConnectionPool::ConnectionPool(int max_conn)
    : free_list_head_(NULL),
      active_list_head_(NULL),
      active_list_tail_(NULL),
      active_list_lookup_cursor_(NULL),
      count_(0),
      max_conn_(max_conn) {
    
    if (pthread_mutex_init(&free_list_mtx_, NULL) != 0) {
        LOG_DEBUG(-1, NULL);
    }
    
    if (pthread_mutex_init(&active_list_mtx_, NULL) != 0) {
        LOG_DEBUG(-1, NULL);
    }
}

void ConnectionPool::Reserve(int n) {
    if (n < 1) {
        return;
    }
    
    Connection *conn = new Connection();
    
    free_list_head_ = conn;
    
    Connection *tail = conn;
    count_++;
    for (int i = 1; i < n; i++, count_++) {
        conn = new Connection();
        
        tail->free_next_ = conn;
        tail = conn;
    }
}

Connection *ConnectionPool::FreeListPop() {
    LockGuard lock_guard(free_list_mtx_);
    
    Connection *conn = NULL;
    
    if (free_list_head_) {
        conn = free_list_head_;
        free_list_head_ = free_list_head_->free_next_;
    } else {
        if (count_ > max_conn_) {
            return NULL;
        }
        
        conn = new Connection();
        count_++;
    }
    
    return conn;
}

void ConnectionPool::FreeListPush(Connection *conn) {
    LockGuard lock_guard(free_list_mtx_);
    
    conn->free_next_ = free_list_head_;
    free_list_head_ = conn;
}

void ConnectionPool::ActiveListPush(Connection *conn) {
    LockGuard lock_guard(active_list_mtx_);
    
    if (NULL == active_list_tail_) {
        active_list_tail_ = conn;
        active_list_head_ = active_list_tail_;
    } else {
        active_list_tail_->active_next_ = conn;
        conn->active_prev_ = active_list_tail_;
        conn->active_next_ = NULL;
        active_list_tail_ = conn;
    }
}

void ConnectionPool::ActiveListUpdate(Connection *conn) {
    ActiveListErase(conn);
    ActiveListPush(conn);
}
    
bool ConnectionPool::ActiveListEmpty() {
    LockGuard lock_guard(active_list_mtx_);
    if (active_list_head_) {
        return false;
    }
    
    return true;
}

void ConnectionPool::ActiveListErase(Connection *conn) {
    LockGuard lock_guard(active_list_mtx_);
    
    if (conn == active_list_lookup_cursor_) {
        active_list_lookup_cursor_ = conn->active_next_;
    }
    
    if (active_list_tail_ == active_list_head_) {
        active_list_tail_ = NULL;
        active_list_head_ = NULL;
    } else {
        if (conn->active_prev_) {
            conn->active_prev_->active_next_ = conn->active_next_;
        } else {
            active_list_head_ = conn->active_next_;
            if (active_list_head_) {
                active_list_head_->active_prev_ = NULL;
            }
        }
        
        if (conn->active_next_) {
            conn->active_next_->active_prev_ = conn->active_prev_;
        } else {
            active_list_tail_ = conn->active_prev_;
            if (active_list_tail_) {
                active_list_tail_->active_next_ = NULL;
            }
        }
    }
    
    conn->active_next_ = NULL;
    conn->active_prev_ = NULL;
}

void ConnectionPool::ActiveListTimeoutCheck() {
    LockGuard lock_guard(active_list_mtx_);
    active_list_lookup_cursor_ = active_list_head_;
}

Connection *ConnectionPool::ActiveListCheckNext() {
    LockGuard lock_guard(active_list_mtx_);
    
    Connection *conn = active_list_lookup_cursor_;
    if (active_list_lookup_cursor_) {
        active_list_lookup_cursor_ = active_list_lookup_cursor_->active_next_;
    }

    return conn;
}

ConnectionPool::~ConnectionPool() {
    pthread_mutex_destroy(&free_list_mtx_);
    pthread_mutex_destroy(&active_list_mtx_);
}
    
}//namespace mevent
