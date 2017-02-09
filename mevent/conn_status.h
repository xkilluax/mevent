#ifndef _CONN_STATUS_H
#define _CONN_STATUS_H

#include <stdint.h>

namespace mevent {

enum class ConnStatus : uint8_t {
    AGAIN,
    END,
    ERROR,
    CLOSE
};
    
}//namespace mevent

#endif
