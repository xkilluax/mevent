#include "connection.h"
#include "util.h"
#include "websocket.h"
#include "event_loop.h"

#include <errno.h>

namespace mevent {

Connection::Connection() : req_(this), resp_(this), ws_(this) {
    fd_ = -1;
    
    active_time_ = time(NULL);
    
    free_next_ = NULL;
    active_next_ = NULL;
    active_prev_ = NULL;
    elp_ = NULL;
    
    ev_writable_ = false;
    
    if (pthread_mutex_init(&mtx_, NULL) != 0) {
        LOG_DEBUG(-1, NULL);
    }
}

Connection::~Connection() {
    if (pthread_mutex_destroy(&mtx_) != 0) {
        LOG_DEBUG(-1, NULL);
    }
}

ConnStatus Connection::Flush() {
    if (fd_ < 0) {
        return ConnStatus::CLOSE;
    }
    
    while (!write_buffer_chain_.empty()) {
        auto wb = write_buffer_chain_.front();

        ssize_t n = Writen(wb->str.c_str() + wb->write_len, wb->len - wb->write_len);
        if (n >= 0) {
            wb->write_len += n;
            if (wb->write_len == wb->len) {
                write_buffer_chain_.pop();
            } else {
                return ConnStatus::AGAIN;
            }
        } else {
            return ConnStatus::ERROR;
        }
    }
    
    if (req_.status_ == RequestStatus::UPGRADE) {
        return ConnStatus::AGAIN;
    }
    
    if (resp_.finish_) {
        return ConnStatus::END;
    }
    
    return ConnStatus::AGAIN;
}

ConnStatus Connection::ReadData() {
    if (fd_ < 0) {
        return ConnStatus::CLOSE;
    }
    
    active_time_ = time(NULL);
    
    ConnStatus status = ConnStatus::AGAIN;
    
    if (req_.status_ == RequestStatus::UPGRADE) {
        status = ws_.ReadData();
    } else {
        status = req_.ReadData();
    }
    
    return status;
}

void Connection::Keepalive() {
    if (fd_ < 0) {
        return;
    }
    
    req_.Keepalive();
    
    resp_.Reset();
}

void Connection::Close() {
    elp_->ResetConnection(this);
}

void Connection::Reset() {
    if (fd_ > 0) {
        close(fd_);
        fd_ = -1;
    }
    
    active_time_ = 0;
    
    elp_ = NULL;
    
    req_.Reset();
    resp_.Reset();
    ws_.Reset();
    
    write_buffer_chain_ = {};
    
    ev_writable_ = false;
}
    
void Connection::ShutdownSocket(int how) {
    if (fd_ > 0) {
        shutdown(fd_, how);
    }
}
    
void Connection::WriteString(const std::string &str) {
    if (str.empty()) {
        return;
    }
    
    if (fd_ < 0) {
        return;
    }
    
    std::shared_ptr<WriteBuffer> wb = std::make_shared<WriteBuffer>(str);
    write_buffer_chain_.push(wb);
}
    
void Connection::WriteData(const std::vector<uint8_t> &data) {
    if (data.empty()) {
        return;
    }
    
    if (fd_ < 0) {
        return;
    }
    
    std::string str;
    str.insert(str.end(), data.begin(), data.end());
    
    std::shared_ptr<WriteBuffer> wb = std::make_shared<WriteBuffer>(str);
    write_buffer_chain_.push(wb);
}

ssize_t Connection::Writen(const void *buf, size_t len) {
    ssize_t nwrite, n;
    
    n = 0;
    
    while (n < (ssize_t)len) {
        nwrite = write(fd_, (char *)buf + n, len);
        if (nwrite > 0) {
            n += nwrite;
        } else if (nwrite < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return n;
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    return n;
}
    
ssize_t Connection::Readn(void *buf, size_t len) {
    ssize_t nread, n;
    
    n = 0;
    
    while (n < (ssize_t)len) {
        nread = read(fd_, (char *)buf + n, len);
        if (nread > 0) {
            n += nread;
        } else if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return n;
            } else {
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    return n;
}
    
void Connection::TaskPush() {
    elp_->TaskPush(this);
}
    
void Connection::WebSocketTaskPush(WebSocketOpcodeType opcode, const std::string &msg) {
    elp_->WebSocketTaskPush(&ws_, opcode, msg);
}
    
Request *Connection::Req() {
    return &req_;
}
    
Response *Connection::Resp() {
    return &resp_;
}
    
WebSocket *Connection::WS() {
    return &ws_;
}
    
}//namespace mevent