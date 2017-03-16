#include "websocket.h"
#include "mevent.h"
#include "base64.h"
#include "util.h"
#include "connection.h"
#include "lock_guard.h"

#include <string>

#include <openssl/sha.h>

namespace mevent {

// http://tools.ietf.org/html/rfc6455#section-5.2  Base Framing Protocol
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-------+-+-------------+-------------------------------+
// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
// | |1|2|3|       |K|             |                               |
// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// |     Extended payload length continued, if payload len == 127  |
// + - - - - - - - - - - - - - - - +-------------------------------+
// |                               |Masking-key, if MASK set to 1  |
// +-------------------------------+-------------------------------+
// | Masking-key (continued)       |          Payload Data         |
// +-------------------------------- - - - - - - - - - - - - - - - +
// :                     Payload Data continued ...                :
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// |                     Payload Data continued ...                |
// +---------------------------------------------------------------+
    
//Reference: https://github.com/dhbaird/easywsclient
    
#define READ_BUFFER_SIZE 2048
    
void WebSocket::Reset() {
    rbuf_.clear();
    std::vector<uint8_t>().swap(rbuf_);
    
    cache_str_.clear();
    std::string().swap(cache_str_);
    
    on_close_func_ = nullptr;
    ping_func_ = nullptr;
    pong_func_ = nullptr;
    on_message_func_ = nullptr;
    
    max_buffer_size_ = 8192;
}

void WebSocket::MakeFrame(std::vector<uint8_t> &frame_data, const std::string &message, uint8_t opcode) {
    std::size_t message_len = message.length();
    
    std::vector<uint8_t> header;
    
    if (message_len < 126) {
        header.assign(2, 0);
    } else if (message_len < 65536) {
        header.assign(4, 0);
    } else {
        header.assign(10, 0);
    }

    header[0] = 0x80 | opcode;
    
    if (message_len < 126) {
        header[1] = (message_len & 0xff);
    } else if (message_len < 65536) {
        header[1] = 126;
        header[2] = (message_len >> 8) & 0xff;
        header[3] = (message_len >> 0) & 0xff;
    } else {
        header[1] = 127;
        header[2] = (message_len >> 56) & 0xff;
        header[3] = (message_len >> 48) & 0xff;
        header[4] = (message_len >> 40) & 0xff;
        header[5] = (message_len >> 32) & 0xff;
        header[6] = (message_len >> 24) & 0xff;
        header[7] = (message_len >> 16) & 0xff;
        header[8] = (message_len >>  8) & 0xff;
        header[9] = (message_len >>  0) & 0xff;
    }
    
    frame_data.insert(frame_data.end(), header.begin(), header.end());
    if (!message.empty()) {
        frame_data.insert(frame_data.end(), message.begin(), message.end());
    }
}

void WebSocket::Handshake(std::string &data, const std::string &sec_websocket_key) {
    std::string sec_str = sec_websocket_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    unsigned char md[SHA_DIGEST_LENGTH];
    
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, sec_str.c_str(), sec_str.length());
    SHA1_Final(md, &ctx);
    
    std::string sec_str_b64 = Base64Encode(static_cast<const unsigned char *>(md), SHA_DIGEST_LENGTH);
    
    data = "HTTP/1.1 101 Switching Protocols" CRLF;
    data += "Server: " SERVER CRLF;
    data += "Date: " + util::GetGMTimeStr() + CRLF;
    data += "Upgrade: websocket" CRLF;
    data += "Connection: Upgrade" CRLF;
    data += "Sec-WebSocket-Accept: " + sec_str_b64 + CRLF;
    data += CRLF;
}

bool WebSocket::Upgrade()
{
    if (conn_->Req()->sec_websocket_key_.empty()) {
        return false;
    }
    
    std::string sec_websocket_key = conn_->Req()->sec_websocket_key_;

    conn_->Req()->Reset();
    conn_->Resp()->Reset();
    
    conn_->Req()->status_ = RequestStatus::UPGRADE;
    
    std::string response_header;
    
    Handshake(response_header, sec_websocket_key);
    
    conn_->WriteString(response_header);
    
    return true;
}
    
ConnStatus WebSocket::ReadData() {
    char buf[READ_BUFFER_SIZE];

    ssize_t n = conn_->Readn(buf, READ_BUFFER_SIZE);

    if (n < 0) {
        return ConnStatus::CLOSE;
    }

    rbuf_.insert(rbuf_.end(), buf, buf + n);
    if (!Parse()) {
        return ConnStatus::ERROR;
    }
    
    return ConnStatus::AGAIN;
}
    
bool WebSocket::Parse() {
    while (true) {
        WebSocketHeader wsh;

        if (rbuf_.size() < 2) {
            return true;
        }

        const uint8_t *data = static_cast<const uint8_t *>(&rbuf_[0]);
        
        wsh.len = 0;
        wsh.fin = (data[0] & 0x80) == 0x80;
        wsh.opcode = static_cast<WebSocketOpcodeType>(data[0] & 0x0f);
        wsh.mask = (data[1] & 0x80) == 0x80;
        wsh.len0 = data[1] & 0x7f;
        wsh.header_size = 2;
        if (wsh.len0 == 126) {
            wsh.header_size += 2;
        } else if (wsh.len0 == 127) {
            wsh.header_size += 8;
        }
        
        if (wsh.mask) {
            wsh.header_size += sizeof(wsh.masking_key);
        }
        
        if (rbuf_.size() < wsh.header_size) {
            return true;
        }

        if (wsh.len0 < 126) {
            wsh.len = wsh.len0;
        } else if (wsh.len0 == 126) {
            wsh.len |= ((uint64_t) data[2]) << 8;
            wsh.len |= ((uint64_t) data[3]) << 0;
        } else if (wsh.len0 == 127) {
            wsh.len |= ((uint64_t) data[2]) << 56;
            wsh.len |= ((uint64_t) data[3]) << 48;
            wsh.len |= ((uint64_t) data[4]) << 40;
            wsh.len |= ((uint64_t) data[5]) << 32;
            wsh.len |= ((uint64_t) data[6]) << 24;
            wsh.len |= ((uint64_t) data[7]) << 16;
            wsh.len |= ((uint64_t) data[8]) << 8;
            wsh.len |= ((uint64_t) data[9]) << 0;
        }
        
        if (wsh.mask) {
            wsh.masking_key[0] = data[wsh.header_size - 4];
            wsh.masking_key[1] = data[wsh.header_size - 3];
            wsh.masking_key[2] = data[wsh.header_size - 2];
            wsh.masking_key[3] = data[wsh.header_size - 1];
        }
        
        if (rbuf_.size() < wsh.header_size + wsh.len) {
            return true;
        }
        
        if (cache_str_.length() + wsh.len > max_buffer_size_) {
            return false;
        }
        
        if (wsh.mask && wsh.len > 0) {
            for (uint64_t i = 0; i != wsh.len; i++) {
                rbuf_[i + wsh.header_size] ^= wsh.masking_key[i & 0x3];
            }
        }
        
        cache_str_.insert(cache_str_.end(), rbuf_.begin() + wsh.header_size, rbuf_.begin() + wsh.header_size + wsh.len);
        
        if (wsh.opcode == WebSocketOpcodeType::BINARY_FRAME
            || wsh.opcode == WebSocketOpcodeType::TEXT_FRAME
            || wsh.opcode == WebSocketOpcodeType::CONTINUATION) {
            if (wsh.fin) {
                conn_->WebSocketTaskPush(wsh.opcode, cache_str_);
                
                cache_str_.clear();
                std::string().swap(cache_str_);
            }
        } else if (wsh.opcode == WebSocketOpcodeType::PING) {
            conn_->WebSocketTaskPush(wsh.opcode, cache_str_);
            cache_str_.clear();
            std::string().swap(cache_str_);
        } else if (wsh.opcode == WebSocketOpcodeType::PONG) {
            conn_->WebSocketTaskPush(wsh.opcode, cache_str_);
        } else if (wsh.opcode == WebSocketOpcodeType::CLOSE) {
            conn_->WebSocketTaskPush(wsh.opcode, cache_str_);
        } else {
            conn_->WebSocketTaskPush(WebSocketOpcodeType::CLOSE, cache_str_);
        }
        
        rbuf_.erase(rbuf_.begin(), rbuf_.begin() + wsh.header_size + wsh.len);
    }
    
    return true;
}
    
void WebSocket::SendPong(const std::string &str) {
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::PONG);
    conn_->WriteData(frame_data);
}

void WebSocket::SendPing(const std::string &str) {
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::PING);
    conn_->WriteData(frame_data);
}
    
void WebSocket::WriteString(const std::string &str) {
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::TEXT_FRAME);
    conn_->WriteData(frame_data);
}
    
bool WebSocket::SendPongSafe(const std::string &str) {
    LockGuard lock_guard(conn_->mtx_);
    
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::PONG);
    conn_->WriteData(frame_data);
    
    ConnStatus status = conn_->Flush();
    if (status == ConnStatus::ERROR && status == ConnStatus::CLOSE) {
        return false;
    }
    
    return true;
}
    
bool WebSocket::SendPingSafe(const std::string &str) {
    LockGuard lock_guard(conn_->mtx_);
    
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::PING);
    conn_->WriteData(frame_data);
    
    ConnStatus status = conn_->Flush();
    if (status == ConnStatus::ERROR && status == ConnStatus::CLOSE) {
        return false;
    }
    
    return true;
}
    
bool WebSocket::WriteStringSafe(const std::string &str) {
    LockGuard lock_guard(conn_->mtx_);
    
    std::vector<uint8_t> frame_data;
    MakeFrame(frame_data, str, WebSocketOpcodeType::TEXT_FRAME);
    conn_->WriteData(frame_data);
    
    ConnStatus status = conn_->Flush();
    if (status == ConnStatus::ERROR && status == ConnStatus::CLOSE) {
        return false;
    }
    
    return true;
}
    
bool WebSocket::WriteRawDataSafe(const std::vector<uint8_t> &data) {
    LockGuard lock_guard(conn_->mtx_);
    
    conn_->WriteData(data);
    
    ConnStatus status = conn_->Flush();
    if (status == ConnStatus::ERROR && status == ConnStatus::CLOSE) {
        return false;
    }
    
    return true;
}
    
Connection *WebSocket::Conn() {
    return conn_;
}
    
void WebSocket::SetOnMessageHandler(WebSocketHandlerFunc func) {
    on_message_func_ = func;
}

void WebSocket::SetPingHandler(WebSocketHandlerFunc func) {
    ping_func_ = func;
}

void WebSocket::SetPongHandler(WebSocketHandlerFunc func) {
    pong_func_ = func;
}

void WebSocket::SetOnCloseHandler(WebSocketCloseHandlerFunc func) {
    on_close_func_ = func;
}

void WebSocket::SetMaxBufferSize(std::size_t size) {
    max_buffer_size_ = size;
}

}//namespace mevent
