#ifndef _WEBSOCKET_H
#define _WEBSOCKET_H

#include "request.h"
#include "conn_status.h"

#include <stdint.h>

#include <vector>
#include <string>
#include <functional>

namespace mevent {

class Connection;
class WebSocket;
    
enum class WebsocketParseStatus : uint8_t {
    AGAIN,
    FINISHED,
    ERROR
};
    
enum WebSocketOpcodeType {
    CONTINUATION  = 0x0,
    TEXT_FRAME    = 0x1,
    BINARY_FRAME  = 0x2,
    CLOSE         = 0x8,
    PING          = 0x9,
    PONG          = 0xa,
};
    
struct WebSocketHeader {
    uint8_t             header_size;
    bool                fin;
    bool                mask;
    WebSocketOpcodeType opcode;
    uint8_t             len0;
    uint64_t            len;
    uint8_t             masking_key[4];
};
    
struct WebSocketTaskItem {
    WebSocketTaskItem(WebSocket *w,
                      WebSocketOpcodeType op,
                      const std::string &m)
    : ws(w), opcode(op), msg(m) {};
    
    WebSocket           *ws;
    WebSocketOpcodeType  opcode;
    std::string          msg;
};
    
typedef std::function<void(WebSocket *, const std::string &)> WebSocketHandlerFunc;
typedef std::function<void(WebSocket *)> WebSocketCloseHandlerFunc;

class WebSocket
{
public:
    WebSocket(Connection *conn) : conn_(conn) { Reset(); };
    ~WebSocket() {};
    
    static void MakeFrame(std::vector<uint8_t> &frame_data, const std::string &message, uint8_t opcode);
    
    bool Upgrade();
    
    void SendPong(const std::string &str);
    void SendPing(const std::string &str);
    
    void WriteString(const std::string &str);
    
    Connection *Conn();
    
    void SetOnMessageHandler(WebSocketHandlerFunc func);
    void SetPingHandler(WebSocketHandlerFunc func);
    void SetPongHandler(WebSocketHandlerFunc func);
    void SetOnCloseHandler(WebSocketCloseHandlerFunc func);
    
    void SetMaxBufferSize(std::size_t size);
    
    //Thread Safe
    bool SendPongSafe(const std::string &str);
    bool SendPingSafe(const std::string &str);
    bool WriteStringSafe(const std::string &str);
    bool WriteRawDataSafe(const std::vector<uint8_t> &data);
    
private:
    friend class EventLoop;
    friend class Connection;
    
    void Reset();
    
    void Handshake(std::string &data, const std::string &sec_websocket_key);

    bool Parse();
    
    ConnStatus ReadData();
    
    Connection                 *conn_;
    
    std::size_t                 max_buffer_size_;
    
    std::vector<uint8_t>        rbuf_;
    std::string                 cache_str_;
    
    WebSocketHandlerFunc        on_message_func_;
    WebSocketHandlerFunc        ping_func_;
    WebSocketHandlerFunc        pong_func_;
    WebSocketCloseHandlerFunc   on_close_func_;
};

}//namespace mevent

#endif
