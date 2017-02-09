#ifndef _CONNECTION_POOL_H
#define _CONNECTION_POOL_H

#include "connection.h"

#include <pthread.h>

namespace mevent {

class ConnectionPool {
public:
    ConnectionPool(int max_conn);
    virtual ~ConnectionPool();
    
    void Reserve(int n);
    
    Connection *FreeListPop();
    void FreeListPush(Connection *c);
    
    void ActiveListPush(Connection *c);
    void ActiveListErase(Connection *c);
    void ActiveListUpdate(Connection *c);
    bool ActiveListEmpty();
    
    void ActiveListTimeoutCheck();
    Connection *ActiveListCheckNext();
    
    void ResetConnection(Connection *c);
    
private:
    pthread_mutex_t   free_list_mtx_;
    Connection       *free_list_head_;
    
    pthread_mutex_t   active_list_mtx_;
    Connection       *active_list_head_;
    Connection       *active_list_tail_;
    Connection       *active_list_lookup_cursor_;
    
    int               count_;
    int               max_conn_;
};

}//namespace mevent

#endif
