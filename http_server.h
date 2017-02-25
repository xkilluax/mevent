#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H

#include "event_loop.h"

#include <pthread.h>
#include <openssl/ssl.h>

namespace mevent {

class HTTPServer {
public:
    HTTPServer();
    ~HTTPServer() {};
 
    void ListenAndServe(const std::string &ip, int port);
    
    void ListenAndServeTLS(const std::string &ip, int port, const std::string &cert_file, const std::string &key_file);
    
    void SetHandler(const std::string &name, HTTPHandleFunc func);
    
    //Settings
    void SetRlimitNofile(int num);
    void SetUser(const std::string &user);
    void SetWorkerThreads(int num);
    void SetMaxWorkerConnections(int num);
    void SetIdleTimeout(int secs);
    
    //Default 8192 bytes
    void SetMaxPostSize(size_t size);
    
    //Default 2048 bytes
    void SetMaxHeaderSize(size_t size);
    
    void Daemonize(const std::string &working_dir);
    
private:
    static void *EventLoopThread(void *arg);
    
    static void SSLLockingCb(int mode, int type, const char* file, int line);
    static unsigned long SSLIdCb();
    
    int          listen_fd_;
    
    HTTPHandler  handler_;
    
    std::string  user_;
    int          rlimit_nofile_;
    int          worker_threads_;
    int          max_worker_connections_;
    int          idle_timeout_;
    size_t       max_post_size_;
    size_t       max_header_size_;
    
    SSL_CTX         *ssl_ctx_;
    static pthread_mutex_t *ssl_mutex_;
};

}//namespace mevent

#endif
