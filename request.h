#ifndef _REQUEST_H
#define _REQUEST_H

#include "conn_status.h"

#include <netinet/in.h>
#include <stdint.h>

#include <string>
#include <map>

namespace mevent {
    
enum class RequestMethod : uint8_t {
    GET,
    POST,
    HEAD,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH,
    UNKNOWN
};

enum class RequestStatus : uint8_t {
    HEADER_RECEIVING,
    BODY_RECEIVING,
    BODY_RECEIVED,
    UPGRADE,
};

enum class RequestParseStatus : uint8_t {  
    S_START,
    S_METHOD,
    S_METHOD_P,
    S_PATH,
    S_QUERY_STRING,
    S_CONTENT_LENGTH,
    S_CONTNET_LENGTH_V,
    S_CONTENT_TYPE,
    S_CONTENT_TYPE_V,
    S_SEC_WEBSOCKET_KEY,
    S_SEC_WEBSOCKET_KEY_V,
    S_UPGRADE,
    S_EOL,
    S_HEADER_FIELD,
    S_EOH
};
    
enum class HTTPParserStatus : uint8_t {
    CONTINUE,
    ERROR,
    FINISHED
};
    
class Connection;
class WebSocket;
class EventLoop;

class Request {
public:
    Request(Connection *conn);
    virtual ~Request() {};
    
    void ParseHeader();
    std::string HeaderValue(const std::string &field);
    
    void ParseQueryString();
    std::string QueryStringValue(const std::string &field);
    
    void ParsePostForm();
    std::string PostFormValue(const std::string &field);
    
//    void ParseCookie();
//    void std::string CookieValue(const std::string &field);
    
    uint32_t ContentLength();
    
    std::string Body();
    
    std::string Path();
    std::string QueryString();
    
    RequestMethod Method();
    
private:
    friend class Connection;
    friend class WebSocket;
    friend class EventLoop;
    
    void ParseFormUrlencoded(std::map<std::string, std::string> &m, const std::string &str);
    
    void Reset();
    
    void Keepalive();
    
    ConnStatus ReadData();
    
    HTTPParserStatus Parse();
    
    RequestStatus         status_;
    
    struct in_addr        addr_;
    
    std::string           rbuf_;
    uint32_t              rbuf_len_;
    
    RequestMethod         method_;

    std::string           path_;
    std::string           query_string_;

    uint32_t              content_length_;
    size_t                header_len_;
    
    RequestParseStatus    parse_status_;
    uint32_t              parse_offset_;
    uint32_t              parse_match_;
    
    Connection           *conn_;
    
    int                   error_code_;
    
    std::string           content_type_;
    
    std::string           sec_websocket_key_;
    
    std::map<std::string, std::string> header_map_;
    
    std::map<std::string, std::string> get_form_map_;
    std::map<std::string, std::string> post_form_map_;
};
    
}

#endif
