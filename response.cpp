#include "response.h"
#include "mevent.h"
#include "util.h"
#include "connection.h"

namespace mevent {
    
#define HTTP_200_HEAD "HTTP/1.1 200 OK" CRLF
#define HTTP_400_HEAD "HTTP/1.1 400 Bad Request" CRLF
#define HTTP_403_HEAD "HTTP/1.1 403 Forbidden" CRLF
#define HTTP_404_HEAD "HTTP/1.1 404 Not Found" CRLF
#define HTTP_500_HEAD "HTTP/1.1 500 Internal Server Error" CRLF
    
#define HTTP_400_MSG "<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1><hr><address>" SERVER "</address></body></html>"
#define HTTP_403_MSG "<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1><hr><address>" SERVER "</address></body></html>"
#define HTTP_404_MSG "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><hr><address>" SERVER "</address></body></html>"
#define HTTP_500_MSG "<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1><hr><address>" SERVER "</address></body></html>"

Response::Response(Connection *conn) : conn_(conn) {
    Reset();
}
    
void Response::Reset() {
    header_map_.clear();
    std::map<std::string, std::string>().swap(header_map_);
    
    finish_ = false;
}
    
void Response::WriteErrorMessage(int code) {
    std::string str;
    
    switch (code) {
        case 400:
            str = HTTP_400_HEAD "Server: " SERVER CRLF "Content-Type: text/html" CRLF "Content-Length: " + std::to_string(sizeof(HTTP_403_MSG) - 1) + CRLF "Date: " + util::GetGMTimeStr() + CRLF "Connection: close" CRLF CRLF HTTP_400_MSG;
            break;
        case 403:
            str = HTTP_403_HEAD "Server: " SERVER CRLF "Content-Type: text/html" CRLF "Content-Length: " + std::to_string(sizeof(HTTP_403_MSG) - 1) + CRLF "Date: " + util::GetGMTimeStr() + CRLF "Connection: close" CRLF CRLF HTTP_403_MSG;
            break;
        case 404:
            str = HTTP_404_HEAD "Server: " SERVER CRLF "Content-Type: text/html" CRLF "Content-Length: " + std::to_string(sizeof(HTTP_404_MSG) - 1) + CRLF "Date: " + util::GetGMTimeStr() + CRLF "Connection: close" CRLF CRLF HTTP_404_MSG;
            break;
        default:
            str = HTTP_500_HEAD "Server: " SERVER CRLF "Content-Type: text/html" CRLF "Content-Length: " + std::to_string(sizeof(HTTP_500_MSG) - 1) + CRLF "Date: " + util::GetGMTimeStr() + CRLF "Connection: close" CRLF CRLF HTTP_500_MSG;
            break;
    }
    
    conn_->WriteString(str);
}
    
void Response::WriteData(const std::vector<uint8_t> &data) {
    std::string s = HTTP_200_HEAD;
    
    auto it = header_map_.find("Server");
    if (it != header_map_.end()) {
        s += "Server: " + it->second + CRLF;
    } else {
        s += "Server: " SERVER CRLF;
    }
    
    it = header_map_.find("Date");
    if (it != header_map_.end()) {
        s += "Date: " + it->second + CRLF;
    } else {
        s += "Date: " + util::GetGMTimeStr() + CRLF;
    }
    
    it = header_map_.find("Content-Type");
    if (it != header_map_.end()) {
        s += "Content-Type: " + it->second + CRLF;
    } else {
        s += "Content-Type: application/octet-stream" CRLF;
    }
    
    
    s += "Content-Length: " + std::to_string(data.size()) + CRLF;
    s += "Connection: close" CRLF;
    s += CRLF;
    
    //body
    s.insert(s.end(), data.begin(), data.end());
    
    conn_->WriteString(s);
    
    finish_ = true;
}
    
void Response::WriteString(const std::string &str) {
    std::string s = HTTP_200_HEAD;
    
    auto it = header_map_.find("Server");
    if (it != header_map_.end()) {
        s += "Server: " + it->second + CRLF;
    } else {
        s += "Server: " SERVER CRLF;
    }
    
    it = header_map_.find("Date");
    if (it != header_map_.end()) {
        s += "Date: " + it->second + CRLF;
    } else {
        s += "Date: " + util::GetGMTimeStr() + CRLF;
    }
    
    it = header_map_.find("Content-Type");
    if (it != header_map_.end()) {
        s += "Content-Type: " + it->second + CRLF;
    } else {
        s += "Content-Type: application/octet-stream" CRLF;
    }
    
    s += "Content-Length: " + std::to_string(str.length()) + CRLF;
    s += "Connection: close" CRLF;
    s += CRLF;
    
    //body
    s += str;
    
    conn_->WriteString(s);
    
    finish_ = true;
}
    
void Response::WriteRawData(const std::vector<uint8_t> &data) {
    conn_->WriteData(data);
}
    
void Response::SetHeader(const std::string &field, const std::string &value) {
    if (field.empty() || value.empty()) {
        return;
    }
    
    header_map_[field] = value;
}
    
void Response::Finish() {
    finish_ = true;
}

}//namespace mevent