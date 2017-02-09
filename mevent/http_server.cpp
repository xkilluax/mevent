#include "http_server.h"
#include "util.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <curl/curl.h>

#ifdef __APPLE__
#include <sys/syslimits.h>
#endif

#define LISTENQ      512

namespace mevent {

HTTPServer::HTTPServer() {
    rlimit_nofile_ = 0;
    worker_threads_ = 1;
    max_worker_connections_ = 1024;
    idle_timeout_ = 30;
    max_header_size_ = 2048;
    max_post_size_ = 8192;
}

void *HTTPServer::EventLoopThread(void *arg) {
    HTTPServer *server = (HTTPServer *)arg;
    
    EventLoop *elp = new EventLoop(&server->handler_);
    elp->SetMaxWorkerConnections(server->max_worker_connections_);
    elp->SetIdleTimeout(server->idle_timeout_);
    elp->SetMaxHeaderSize(server->max_header_size_);
    elp->SetMaxPostSize(server->max_post_size_);
    
    elp->Loop(server->listen_fd_);
    return (void *)0;
}

void HTTPServer::ListenAndServe(const std::string &ip, int port) {
    curl_global_init(CURL_GLOBAL_ALL);
    
    signal(SIGPIPE, SIG_IGN);
    
    struct sockaddr_in servaddr;
    int reuse = 1;
    
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        LOG_DEBUG(-1, NULL);
    }
    
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LOG_DEBUG(-1, NULL);
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (ip.empty()) {
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr);
    }
    
    if (bind(listen_fd_, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        LOG_DEBUG(-1, NULL);
    }
    
    if (listen(listen_fd_, LISTENQ) < 0) {
        LOG_DEBUG(-1, NULL);
    }
    
    rlimit_nofile_ = worker_threads_ * max_worker_connections_;

    if (rlimit_nofile_ > 0) {
        struct rlimit limit;
        limit.rlim_cur = rlimit_nofile_;
#ifdef __APPLE__
        if (limit.rlim_cur > OPEN_MAX) {
            limit.rlim_cur = OPEN_MAX;
        }
#endif
        limit.rlim_max = limit.rlim_cur;
        
        if (setrlimit(RLIMIT_NOFILE, &limit) < 0) {
            LOG_DEBUG(0, NULL);
        }
    }
    
    if (!user_.empty()) {
        int gid = util::GetSysGid(user_.c_str());
        if (gid != -1) {
            setgid(gid);
        }
        
        int uid = util::GetSysUid(user_.c_str());
        if (uid != -1) {
            setuid(uid);
        }
    }
    
    pthread_t tid;
    
    for (int i = 0; i < worker_threads_; i++) {
        if (pthread_create(&tid, NULL, EventLoopThread, (void *)this) != 0) {
            LOG_DEBUG(-1, NULL);
        }
        
        if (pthread_detach(tid) != 0) {
            LOG_DEBUG(-1, NULL);
        }
    }
    
    pause();
}

void HTTPServer::SetHandler(const std::string &name, HTTPHandleFunc func) {
    handler_.SetHandleFunc(name, func);
}

void HTTPServer::SetRlimitNofile(int num) {
    rlimit_nofile_ = num;
}
    
void HTTPServer::SetUser(const std::string &user) {
    user_ = user;
}

void HTTPServer::SetWorkerThreads(int num) {
    if (num < 1) {
        return;
    }
    worker_threads_ = num;
}

void HTTPServer::SetMaxWorkerConnections(int num) {
    if (num < 1) {
        return;
    }
    
    max_worker_connections_ = num;
}

void HTTPServer::SetIdleTimeout(int secs) {
    if (secs < 1) {
        return;
    }
    
    idle_timeout_ = secs;
}

void HTTPServer::SetMaxHeaderSize(size_t size) {
    max_header_size_ = size;
}

void HTTPServer::SetMaxPostSize(size_t size) {
    max_post_size_ = size;
}

void HTTPServer::Daemonize(const std::string &working_dir) {
    util::Daemonize(working_dir);
}
    
}//namespace mevent
