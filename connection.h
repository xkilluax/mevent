#ifndef _CONNECTION_H
#define _CONNECTION_H

#include "request.h"
#include "response.h"
#include "websocket.h"
#include "conn_status.h"

#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <openssl/ssl.h>

#include <vector>
#include <queue>
#include <string>
#include <memory>

namespace mevent {

class EventLoop;
class ConnectionPool;

struct WriteBuffer {
    std::string str;
    std::size_t len;
    std::size_t write_len;
    
    WriteBuffer(const std::string &s) : str(s), len(s.length()), write_len(0) {}
    ~WriteBuffer() {}
};

class Connection {
public:
    Connection();
    virtual ~Connection();
    
    void Close();
    
    Request *Req();
    Response *Resp();
    WebSocket *WS();
    
private:
    friend class ConnectionPool;
    friend class EventLoop;
    friend class Request;
    friend class Response;
    friend class WebSocket;
    
    void WriteString(const std::string &str);
    void WriteData(const std::vector<uint8_t> &data);
    
    ssize_t Writen(const void *buf, size_t len);
    ssize_t Readn(void *buf, size_t len);
    
    void TaskPush();
    
    void WebSocketTaskPush(WebSocketOpcodeType opcode, const std::string &msg);
    
    ConnStatus Flush();
    
    ConnStatus ReadData();
    
    void Keepalive();
    
    void Reset();
    
    void ShutdownSocket(int how);
    
    bool CreateSSL(SSL_CTX *ssl_ctx);
    
    int               fd_;
    
    pthread_mutex_t   mtx_;
    
    std::queue<std::shared_ptr<WriteBuffer>> write_buffer_chain_;
    
    Request           req_;
    Response          resp_;
    WebSocket         ws_;
    
    Connection       *free_next_;
    
    Connection       *active_next_;
    Connection       *active_prev_;
    
    EventLoop        *elp_;
    
    time_t            active_time_;
    
    bool              ev_writable_;
    
    SSL              *ssl_;
};

}//namespace mevent
    
#endif
