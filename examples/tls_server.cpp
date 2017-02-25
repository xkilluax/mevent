#include <stdio.h>

#include "../http_server.h"
#include "../util.h"

using namespace mevent;

class TLS {
public:
    void Index(Connection *conn) {
        conn->Resp()->SetHeader("Content-Type", "text/html");
        conn->Resp()->WriteString("hello world!");
    }
};

int main() {
    TLS tls;
    
    HTTPServer *server = new HTTPServer();
    server->SetHandler("/", std::bind(&::TLS::Index, &tls, std::placeholders::_1));
    
    server->SetWorkerThreads(4);
    server->SetIdleTimeout(60);
    server->SetMaxWorkerConnections(8192);
    
    server->ListenAndServeTLS("0.0.0.0", 443, "host.crt", "host.key");

    fflush(NULL);
    return 0;
}
