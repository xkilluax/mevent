#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include <curl/curl.h>

#include <string>

namespace mevent {
namespace util {

class HTTPClient {
public:
    HTTPClient() {};
    ~HTTPClient() {};
    
    bool Request(const std::string &host);
    
    std::string ResponseData();
    
private:
    static size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
    
    std::string    data_;
};

}//namespace util
}//namespace mevent

#endif
