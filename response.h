#ifndef _RESPONSE_H
#define _RESPONSE_H

#include "conn_status.h"

#include <stdint.h>

#include <vector>
#include <string>
#include <map>

namespace mevent {
    
class Connection;

class Response {
public:
    Response(Connection *conn);
    ~Response() {}
    
    void Reset();
    
    void WriteErrorMessage(int code);
    
    void WriteString(const std::string &str);
    void WriteData(const std::vector<uint8_t> &data);
    void WriteRawData(const std::vector<uint8_t> &data);
    
    void SetHeader(const std::string &field, const std::string &value);
    void AddHeader(const std::string &field, const std::string &value);
    void DelHeader(const std::string &field);
    
private:
    friend class Connection;
    
    std::string MakeHeader();
    
    Connection *conn_;
    std::map<std::string, std::vector<std::string>> header_map_;
    
    bool finish_;
};
    
}//namespace mevent

#endif
