#include "event_loop.h"
#include "util.h"
#include "lock_guard.h"

#include <sys/types.h>
#include <netinet/tcp.h>
#include <errno.h>

#include <cstddef>

#define set_nonblock(fd) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK)

namespace mevent {

void HTTPHandler::SetHandleFunc(const std::string &path, HTTPHandleFunc func) {
    if (path.empty()) {
        return;
    }
    
    tst::NodeData data;
    data.str = path;
    data.func = func;
    
    func_tree_.Insert(&data, NULL);
}

HTTPHandleFunc HTTPHandler::GetHandleFunc(const std::string &path) {
    HTTPHandleFunc func = nullptr;
    
    if (path.empty()) {
        return nullptr;
    }
    
    tst::NodeData data = func_tree_.SearchOne(path.c_str());

    if (data.func) {
        func = data.func;
    }
    
    return func;
}


//////////////////

EventLoop::EventLoop() {
    worker_threads_ = 1;
    max_worker_connections_ = 1024;
    idle_timeout_ = 30;
    max_post_size_ = 8192;
    max_header_size_ = 2048;
    
    handler_ = nullptr;
    ssl_ctx_ = NULL;
    
    if (pthread_cond_init(&task_cond_, NULL) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (pthread_mutex_init(&task_cond_mtx_, NULL) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (pthread_cond_init(&ws_task_cond_, NULL) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (pthread_mutex_init(&ws_task_cond_mtx_, NULL) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
}
    
void EventLoop::SetHandler(mevent::HTTPHandler *handler) {
    handler_ = handler;
}
    
void EventLoop::SetSslCtx(SSL_CTX *ssl_ctx) {
    ssl_ctx_ = ssl_ctx;
}

void EventLoop::Loop(int listen_fd) {
    listen_fd_ = listen_fd;
    
    conn_pool_ = new ConnectionPool(max_worker_connections_);
//    conn_pool_->Reserve(worker_connections_);
    
    evfd_ = Create();
    
    if (evfd_ < 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (set_nonblock(listen_fd) < 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    listen_c_.fd_ = listen_fd_;
    if (Add(evfd_, listen_fd, MEVENT_IN, &listen_c_) == -1) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    pthread_t tid;
    if (pthread_create(&tid, NULL, CheckConnectionTimeout, (void *)this) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (pthread_detach(tid) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }

    
    for (int i = 0; i < worker_threads_; i++) {
        if (pthread_create(&tid, NULL, WorkerThread, (void *)this) != 0) {
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
        
        if (pthread_detach(tid) != 0) {
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
    }
    
    for (int i = 0; i < worker_threads_; i++) {
        if (pthread_create(&tid, NULL, WebSocketWorkerThread, (void *)this) != 0) {
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
        
        if (pthread_detach(tid) != 0) {
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
    }
    
    int nfds;
    Connection *conn;
    
    while (1) {
        nfds = Poll(evfd_, events_, 512, NULL);
        
        for (int n = 0; n < nfds; n++) {
            conn = (Connection *)events_[n].data.ptr;
            
            if (conn->fd_ == listen_fd) {
                Accept();
            } else if (events_[n].mask & MEVENT_IN) {
                conn_pool_->ActiveListUpdate(conn);
                
                LockGuard lock_guard(conn->mtx_);
                OnRead(conn);
            } else if (events_[n].mask & MEVENT_OUT) {
                LockGuard lock_guard(conn->mtx_);
                OnWrite(conn);
            }
        }
    }
}

void EventLoop::Accept() {
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    
    for (;;) {
        int clifd = accept(listen_fd_, (struct sockaddr *) &cliaddr, &clilen);
        
        if (clifd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                MEVENT_LOG_DEBUG_EXIT(NULL);
                return;
            }
        }
        
        Connection *conn = conn_pool_->FreeListPop();
        
        if (!conn) {
            MEVENT_LOG_DEBUG("worker connections reach max limit");
            close(clifd);
            return;
        }
        
        
        LockGuard lock_guard(conn->mtx_);
        
        conn->active_time_ = time(NULL);
        conn->fd_ = clifd;
        conn->elp_ = this;
        
        Request *req = conn->Req();
        req->addr_ = cliaddr.sin_addr;
        
        bool status = true;
        
        do {
            if (set_nonblock(clifd) < 0) {
                MEVENT_LOG_DEBUG(NULL);
                status = false;
                break;
            }
            
            int enable = 1;
            if (setsockopt(clifd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0) {
                MEVENT_LOG_DEBUG(NULL);
                status = false;
                break;
            }
            
            if (Add(evfd_, clifd, MEVENT_IN, conn) == -1) {
                MEVENT_LOG_DEBUG(NULL);
                status = false;
                break;
            }
            
            if (ssl_ctx_) {
                if (!conn->CreateSSL(ssl_ctx_)) {
                    status = false;
                    break;
                }
            }
        } while (0);

        if (!status) {
            conn->Reset();
            conn_pool_->FreeListPush(conn);
            continue;
        }
        
        conn_pool_->ActiveListPush(conn);
        
        OnAccept(conn);
    }
}

void *EventLoop::CheckConnectionTimeout(void *arg) {
    EventLoop *elp = (EventLoop *)arg;
    
    for (;;) {
        int64_t now = time(NULL);
        
        elp->conn_pool_->ActiveListTimeoutCheck();
        
        Connection *conn = elp->conn_pool_->ActiveListCheckNext();
        
        if (conn) {
            time_t active_time = 0;
            {
                LockGuard lock_guard(conn->mtx_);
                active_time = conn->active_time_;
            }
            if (active_time > 0 && now - active_time < elp->idle_timeout_) {
                sleep(static_cast<int>(elp->idle_timeout_ - (now - active_time)));
                continue;
            }
        } else {
            if (elp->conn_pool_->ActiveListEmpty()) {
                sleep(elp->idle_timeout_);
            }
            continue;
        }
        
        while (conn) {
            {
                LockGuard lock_guard(conn->mtx_);

                if (conn->active_time_ > 0) {
                    if ((now - conn->active_time_) >= elp->idle_timeout_) {
                        if (conn->Req()->status_ == RequestStatus::UPGRADE) {
                            MEVENT_LOG_DEBUG("connection(websocket) timeout");
                        } else {
                            MEVENT_LOG_DEBUG("connection timeout");
                        }
                        elp->ResetConnection(conn);
                    } else {
                        break;
                    }
                }
            }
            
            conn = elp->conn_pool_->ActiveListCheckNext();
        }
    }
    
    return (void *)0;
}

void EventLoop::ResetConnection(Connection *conn) {
    if (conn->fd_ > 0) {
        OnClose(conn);
        
        conn->Reset();
        
        conn_pool_->FreeListPush(conn);
        conn_pool_->ActiveListErase(conn);
    }
}

void EventLoop::SetIdleTimeout(int secs) {
    if (secs < 1) {
        return;
    }
    
    idle_timeout_ = secs;
}

void EventLoop::SetMaxWorkerConnections(int num) {
    if (num < 1) {
        return;
    }
    
    max_worker_connections_ = num;
}
    
void EventLoop::SetMaxPostSize(size_t size) {
    max_post_size_ = size;
}
    
void EventLoop::SetMaxHeaderSize(size_t size) {
    max_header_size_ = size;
}

void EventLoop::OnAccept(Connection *conn) {
    (void)conn;//avoid unused parameter warning
//    conn->elp_ = this;
}

void EventLoop::OnRead(Connection *conn) {
    ConnStatus status = conn->ReadData();
    
    if (status == ConnStatus::ERROR) {
        if (conn->Req()->error_code_ != 0) {
            conn->Resp()->WriteErrorMessage(conn->Req()->error_code_);
            status = conn->Flush();
        }
    }
    
    if (status != ConnStatus::AGAIN) {
        ResetConnection(conn);
    }
}

void EventLoop::OnWrite(Connection *conn) {
    ConnStatus status = conn->Flush();
    if (status != ConnStatus::AGAIN) {
        ResetConnection(conn);
    }
}

void EventLoop::OnClose(Connection *conn) {
    if (conn->Req()->status_ == RequestStatus::UPGRADE) {
        conn->WS()->on_close_func_(conn->WS());
    }
}

void *EventLoop::WorkerThread(void *arg) {
    EventLoop *elp = (EventLoop *)arg;
    
    for (;;) {
        Connection *conn;
        
        {
            LockGuard lock_guard(elp->task_cond_mtx_);
            
            if (elp->task_que_.empty()) {
                pthread_cond_wait(&elp->task_cond_, &elp->task_cond_mtx_);
                continue;
            }
            
            conn = elp->task_que_.front();
            elp->task_que_.pop();
        }
        
        {
            LockGuard lock_guard(conn->mtx_);
            
            if (conn->fd_ < 0) {
                continue;
            }
            
            Request *req = conn->Req();
            Response *resp = conn->Resp();
            
            HTTPHandleFunc func;
            if ((func = elp->handler_->GetHandleFunc(req->path_))) {
                func(conn);
            } else {
                resp->WriteErrorMessage(404);
            }
            
            if (req->status_ != RequestStatus::UPGRADE) {
                resp->Flush();
            }
            
            ConnStatus status = conn->Flush();

            if (status == ConnStatus::AGAIN) {
                if (!conn->ev_writable_) {
                    elp->Modify(elp->evfd_, conn->fd_, MEVENT_IN | MEVENT_OUT, conn);
                    conn->ev_writable_ = true;
                }
            } else if (status != ConnStatus::UPGRADE) {
                elp->ResetConnection(conn);
            }
        }
    }
    
    return (void *)0;
}
    
void *EventLoop::WebSocketWorkerThread(void *arg) {
    EventLoop *elp = (EventLoop *)arg;
    
    for (;;) {
        std::shared_ptr<WebSocketTaskItem> item;
        
        {
            LockGuard lock_guard(elp->ws_task_cond_mtx_);
            
            if (elp->ws_task_que_.empty()) {
                pthread_cond_wait(&elp->ws_task_cond_, &elp->ws_task_cond_mtx_);
                continue;
            }
            
            item = elp->ws_task_que_.front();
            elp->ws_task_que_.pop();
        }
        
        {
            LockGuard lock_guard(item->ws->Conn()->mtx_);
            
            if (item->ws->Conn()->fd_ < 0) {
                continue;
            }
            
            if (item->opcode == WebSocketOpcodeType::BINARY_FRAME
                || item->opcode == WebSocketOpcodeType::CONTINUATION
                || item->opcode == WebSocketOpcodeType::TEXT_FRAME) {
                if (item->ws->on_message_func_) {
                    item->ws->on_message_func_(item->ws, item->msg);
                }
            } else if (item->opcode == WebSocketOpcodeType::PING) {
                if (item->ws->ping_func_) {
                    item->ws->ping_func_(item->ws, item->msg);
                }
            } else if (item->opcode == WebSocketOpcodeType::PONG) {
                if (item->ws->pong_func_) {
                    item->ws->pong_func_(item->ws, item->msg);
                }
            } else if (item->opcode == WebSocketOpcodeType::CLOSE) {
                if (item->ws->on_close_func_) {
                    item->ws->on_close_func_(item->ws);
                }
            }
            
            ConnStatus status = item->ws->Conn()->Flush();

            if (status == ConnStatus::AGAIN) {
                if (!item->ws->Conn()->ev_writable_) {
                    elp->Modify(elp->evfd_, item->ws->Conn()->fd_, MEVENT_IN | MEVENT_OUT, item->ws->Conn());
                    item->ws->Conn()->ev_writable_ = true;
                }
            } else if (status != ConnStatus::UPGRADE) {
                elp->ResetConnection(item->ws->Conn());
            }
        }
    }
    
    return (void *)0;
}

void EventLoop::TaskPush(Connection *conn) {
    LockGuard cond_lock_guard(task_cond_mtx_);
    task_que_.push(conn);
    pthread_cond_signal(&task_cond_);
}

void EventLoop::WebSocketTaskPush(WebSocket *ws, WebSocketOpcodeType opcode, const std::string &msg) {
    LockGuard lock_guard(ws_task_cond_mtx_);
    std::shared_ptr<WebSocketTaskItem> item = std::make_shared<WebSocketTaskItem>(ws, opcode, msg);
    ws_task_que_.push(item);
    pthread_cond_signal(&ws_task_cond_);
}
    
EventLoop::~EventLoop() {
    delete conn_pool_;
}

}//namespace mevent
