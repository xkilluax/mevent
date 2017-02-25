#ifndef _EVENT_LOOP
#define _EVENT_LOOP

#include "connection_pool.h"
#include "event_loop_base.h"
#include "ternary_search_tree.h"

#include <openssl/ssl.h>

#include <string>
#include <functional>
#include <queue>
#include <memory>

namespace mevent {

class HTTPHandler {
public:
    HTTPHandler() {};
    virtual ~HTTPHandler() {};
    
    void SetHandleFunc(const std::string &path, HTTPHandleFunc func);
    HTTPHandleFunc GetHandleFunc(const std::string &path);
    
private:
    tst::TernarySearchTree   func_tree_;
};

class EventLoop : public EventLoopBase {
public:
    EventLoop();
    virtual ~EventLoop();
    
    void ResetConnection(Connection *conn);
    
    void Loop(int listen_fd);
    
    void SetHandler(HTTPHandler *handler);
    
    void SetSslCtx(SSL_CTX *ssl_ctx);
    
    void SetMaxWorkerConnections(int num);
    void SetIdleTimeout(int secs);
    void SetMaxPostSize(size_t size);
    void SetMaxHeaderSize(size_t size);
    
    void TaskPush(Connection *conn);
    
    void WebSocketTaskPush(WebSocket *ws, WebSocketOpcodeType opcode, const std::string &msg);
    
private:
    friend class Request;
    friend class Connection;
    
    void OnClose(Connection *conn);
    
    void OnRead(Connection *conn);
    void OnWrite(Connection *conn);
    void OnAccept(Connection *conn);
    
    void Accept();
    
    static void *CheckConnectionTimeout(void *arg);
    
    static void *WorkerThread(void *arg);
    static void *WebSocketWorkerThread(void *arg);

    int                 evfd_;
    int                 listen_fd_;
    Connection          listen_c_;
    
    SSL_CTX            *ssl_ctx_;
    
    pthread_mutex_t     task_cond_mtx_;
    pthread_cond_t      task_cond_;
    std::queue<Connection *> task_que_;
    
    HTTPHandler        *handler_;
    
    EventLoopBase::Event  events_[512];
    
    int                 worker_threads_;
    int                 max_worker_connections_;
    int                 idle_timeout_;
    size_t              max_post_size_;
    size_t              max_header_size_;
    
    ConnectionPool     *conn_pool_;
    
    pthread_mutex_t     ws_task_cond_mtx_;
    pthread_cond_t      ws_task_cond_;
    std::queue<std::shared_ptr<WebSocketTaskItem>> ws_task_que_;
};

}//namespace mevent

#endif
