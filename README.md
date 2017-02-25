# mevent
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/looyao/mevent/master/LICENSE.md)
[![Build Status](https://travis-ci.org/looyao/mevent.svg?branch=master)](https://travis-ci.org/looyao/mevent)

Mevent is a tiny HTTP/WebSocket server library, can be used on Linux and OSX.

## Features

- TLS (https/wss) support
- `ping`/`pong` support
- Asynchronous and non-blocking, I/O Multiplexing using epoll and kqueue
- Supports Linux, OSX
- Thread-safe

## Integration

You should have libssl, libcurl installed into your system, and your compiler should support C++11. 

#### Debian and Ubuntu users
```
apt-get install libssl-dev libcurl4-gnutls-dev
```

#### Fedora and RedHat users
```
yum install openssl-devel libcurl-devel
```

#### OSX users
```
brew update
brew install openssl curl
```

#### Example

```cpp
#include "mevent/http_server.h"

using namespace mevent;

class HelloWorld {
public:
    void Index(Connection *conn) {
        conn->Resp()->SetHeader("Content-Type", "text/html");
        conn->Resp()->WriteString("hello world!");
    }
};

int main() {
    HelloWorld hello;
    
    HTTPServer *server = new HTTPServer();
    server->SetHandler("/", std::bind(&::HelloWorld::Index, &hello, std::placeholders::_1));

    server->SetWorkerThreads(4);
    server->SetIdleTimeout(60);
    server->SetMaxWorkerConnections(8192);
    
    server->ListenAndServe("0.0.0.0", 80);
    
    return 0;
}
```

More examples can be found in examples directory.

## Integration
