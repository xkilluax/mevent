#include "http_server.h"
#include "util.h"

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <assert.h>

#include <openssl/conf.h>
#include <openssl/err.h>

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
    ssl_ctx_ = NULL;
}

void *HTTPServer::EventLoopThread(void *arg) {
    HTTPServer *server = (HTTPServer *)arg;
    
    EventLoop *elp = new EventLoop();
    elp->SetHandler(&server->handler_);
    elp->SetSslCtx(server->ssl_ctx_);
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
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
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
        MEVENT_LOG_DEBUG_EXIT(NULL);
    }
    
    if (listen(listen_fd_, LISTENQ) < 0) {
        MEVENT_LOG_DEBUG_EXIT(NULL);
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
            MEVENT_LOG_DEBUG(NULL);
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
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
        
        if (pthread_detach(tid) != 0) {
            MEVENT_LOG_DEBUG_EXIT(NULL);
        }
    }
    
    pause();
}
    
void HTTPServer::ListenAndServeTLS(const std::string &ip, int port, const std::string &cert_file, const std::string &key_file) {
    if (cert_file.empty() || key_file.empty()) {
        MEVENT_LOG_DEBUG_EXIT("cert_file or key_file can't be empty");
    }
    
    ssl_mutex_ = static_cast<pthread_mutex_t *>(malloc(sizeof(pthread_mutex_t) * CRYPTO_num_locks()));
    assert(ssl_mutex_ != NULL);
    for (int i = 0; i < CRYPTO_num_locks(); i++) {
        pthread_mutex_init(&ssl_mutex_[i], NULL);
    }
    
    CRYPTO_set_locking_callback(SSLLockingCb);
    CRYPTO_set_id_callback(SSLIdCb);
    
    OPENSSL_config(NULL);
    
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    
    ssl_ctx_ = SSL_CTX_new(SSLv23_server_method());
    
    if (!ssl_ctx_) {
        MEVENT_LOG_DEBUG_EXIT("Unable to create SSL context");
    }
    
    std::string self_path = util::ExecutablePath();
    std::string ssl_cert_file;
    if (cert_file[0] != '/') {
        ssl_cert_file = self_path + cert_file;
    } else {
        ssl_cert_file = cert_file;
    }
    
    if (SSL_CTX_use_certificate_file(ssl_ctx_, ssl_cert_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        MEVENT_LOG_DEBUG_EXIT("SSL_CTX_use_certificate_file(%s) failed", ssl_cert_file.c_str());
    }
    
    std::string ssl_key_file;
    if (key_file[0] != '/') {
        ssl_key_file = self_path + key_file;
    } else {
        ssl_key_file = key_file;
    }
    
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, ssl_key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        MEVENT_LOG_DEBUG_EXIT("SSL_CTX_use_PrivateKey_file(%s)", ssl_key_file.c_str());
    }
    
    if(SSL_CTX_check_private_key(ssl_ctx_) != 1) {
        MEVENT_LOG_DEBUG_EXIT("SSL_CTX_check_private_key(%s)", ssl_key_file.c_str());
    }
    
    SSL_CTX_set_read_ahead(ssl_ctx_, 1);
    
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv3);
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_TLSv1);
    
    int nid;
    EC_KEY *ecdh;
    
    nid = OBJ_sn2nid("prime256v1");
    
    if (nid == 0) {
        MEVENT_LOG_DEBUG_EXIT("OBJ_sn2nid(prime256v1) failed");
    }
    
    ecdh = EC_KEY_new_by_curve_name(nid);
    if (ecdh == NULL) {
        MEVENT_LOG_DEBUG_EXIT("EC_KEY_new_by_curve_name(%d)", nid);
    }
    
    SSL_CTX_set_options(ssl_ctx_, SSL_OP_SINGLE_ECDH_USE);
    SSL_CTX_set_tmp_ecdh(ssl_ctx_, ecdh);
    
    EC_KEY_free(ecdh);
    
    ListenAndServe(ip, port);

    SSL_CTX_free(ssl_ctx_);
    EVP_cleanup();
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

pthread_mutex_t *HTTPServer::ssl_mutex_ = NULL;

void HTTPServer::SSLLockingCb(int mode, int type, const char *file, int line) {
    (void)file;//avoid unused parameter warning
    (void)line;//avoid unused parameter warning
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&ssl_mutex_[type]);
    } else {
        pthread_mutex_unlock(&ssl_mutex_[type]);
    }
}
    
unsigned long HTTPServer::SSLIdCb() {
    return (unsigned long)pthread_self();
}
    
}//namespace mevent
