#include "http_client.h"

namespace mevent {
namespace util {
    
#define REQUEST_TIMEOUT 30

bool HTTPClient::Request(const std::string &host) {
    CURL *curl_ = curl_easy_init();
    
    if (!curl_) {
        return false;
    }
    
    if (curl_easy_setopt(curl_, CURLOPT_URL, host.c_str()) != CURLE_OK) {
        return false;
    }
    
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, REQUEST_TIMEOUT);
    
    curl_easy_setopt(curl_, CURLOPT_NOSIGNAL, 1L);
    
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &data_);
    
    curl_easy_perform(curl_);
    
    curl_easy_cleanup(curl_);
    
    return true;
}

std::string HTTPClient::ResponseData() {
	return data_;
}
    
size_t HTTPClient::WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t len = size * nmemb;
    static_cast<std::string *>(userdata)->append(ptr, len);
    return len;
}

}//namespace util
}//namespace mevent