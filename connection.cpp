#include "connection.h"
#include "util.h"
#include "websocket.h"
#include "event_loop.h"

#include <errno.h>
#include <openssl/err.h>

namespace mevent {

Connection::Connection() : req_(this), resp_(this), ws_(this) {
    fd_ = -1;
    
    active_time_ = 0;
    
    free_next_ = NULL;
    active_next_ = NULL;
    active_prev_ = NULL;
    elp_ = NULL;
    
    ev_writable_ = false;
    
    if (pthread_mutex_init(&mtx_, NULL) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    ssl_ = NULL;
}

Connection::~Connection() {
    if (pthread_mutex_destroy(&mtx_) != 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
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
        return ConnStatus::UPGRADE;
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
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = NULL;
    }
    
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
    ssize_t nwrite, n = 0;
    
    while (n < (ssize_t)len) {
        if (ssl_) {
            nwrite = SSL_write(ssl_, (char *)buf + n, static_cast<int>(len));
            if (nwrite <= 0) {
                int sslerr = SSL_get_error(ssl_, static_cast<int>(nwrite));
                switch (sslerr) {
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                    case SSL_ERROR_SSL:
                        return n;
                    case SSL_ERROR_SYSCALL:
                        break;
                    default:
                        return -1;
                }
            }
        } else {
            nwrite = write(fd_, (char *)buf + n, len - n);
        }

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
    ssize_t nread, n = 0;
    
    while (n < (ssize_t)len) {
        if (ssl_) {
            nread = SSL_read(ssl_, (char *)buf + n, static_cast<int>(len));
            if (nread <= 0) {
                int sslerr = SSL_get_error(ssl_, static_cast<int>(nread));
                switch (sslerr) {
                    case SSL_ERROR_WANT_READ:
                    case SSL_ERROR_WANT_WRITE:
                    case SSL_ERROR_SSL:
                        return n;
                    case SSL_ERROR_SYSCALL:
                        break;
                    default:
                        return -1;
                }
            }
        } else {
            nread = read(fd_, (char *)buf + n, len - n);
        }

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
    
bool Connection::CreateSSL(SSL_CTX *ssl_ctx) {
    ssl_ = SSL_new(ssl_ctx);
    if (!ssl_) {
        MEVENT_LOG_DEBUG("SSL_new() failed");
        return false;
    }

    if (SSL_set_fd(ssl_, fd_) == 0) {
        MEVENT_LOG_DEBUG("SSL_set_fd() failed");
        return false;
    }
    
    SSL_set_accept_state(ssl_);
    
    return true;
}
    
}//namespace mevent
