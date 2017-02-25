#include "util.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/signal.h>

#ifdef __APPLE__
#include <libproc.h>
#endif

#include <vector>

namespace mevent {
namespace util {
    
static const unsigned char hexchars[] = "0123456789ABCDEF";
    
pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;
    
std::string GetGMTimeStr() {
    char buf[128];
    time_t t = time(NULL);
    struct tm stm;
    gmtime_r(&t, &stm);
    strftime(buf, 128, "%a, %d %b %Y %H:%M:%S GMT", &stm);
    return std::string(buf);
}
    
void LogDebug(const char *file, int line, int opt, const char *fmt, ...) {
    if (pthread_mutex_lock(&log_mtx) != 0) {
        fprintf(stderr, "DEBUG file:%s line:%d %s\n", __FILE__, __LINE__, strerror(errno));
    }
    
    do {
        time_t now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);
        
        char buf[32];
        if (strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t) == 0) {
            fprintf(stderr, "DEBUG file:%s line:%d %s\n", file, __LINE__, strerror(errno));
            break;
        }
        
        if (fmt) {
            fprintf(stderr, "%s DEBUG file:%s line:%d ", buf, file, line);
            
            va_list argptr;
            va_start(argptr, fmt);
            vfprintf(stderr, fmt, argptr);
            va_end(argptr);
            
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "%s DEBUG file:%s line:%d %s\n", buf, file, line, strerror(errno));
        }
        fflush(stderr);
    } while (0);
        
    if (pthread_mutex_unlock(&log_mtx) != 0) {
        fprintf(stderr, "DEBUG file:%s line:%d %s\n", __FILE__, __LINE__, strerror(errno));
    }
    
    if (opt) {
        exit(opt);
    }
}
    
int GetAddrFromCIDR(int cidr, struct in_addr *addr) {
    int ocets;
        
    if (cidr < 0 || cidr > 32) {
        errno = EINVAL;
        return -1;
    }
    ocets = (cidr + 7) / 8;
        
    addr->s_addr = 0;
    if (ocets > 0) {
        memset(&addr->s_addr, 255, (size_t)ocets - 1);
        memset((unsigned char *)&addr->s_addr + (ocets - 1), (256 - (1 << (32 - cidr) % 8)), 1);
    }
        
    return 0;
}
    
bool IsInSubNet(const struct in_addr *addr, const struct in_addr *netaddr, const struct in_addr *netmask) {
    if ((addr->s_addr & netmask->s_addr) == (netaddr->s_addr & netmask->s_addr)) {
        return true;
    }
    return false;
}
    
void RC4(const std::string &key, const std::string &src, std::string &dest) {
    int x = 0, y = 0;
    uint8_t tmp;
    std::size_t key_len = key.length();
    std::size_t src_len = src.length();
    
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), src.begin(), src.end());
        
    unsigned char s[256] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
        16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
        32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
        48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
        80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
        128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
        144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
        160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
        176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
        192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
        208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
        224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
        240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
    };
        
    for (int i = 0; i < 256; i++) {
        y = (key[x] + s[i] + y) % 256;
        
        tmp = s[i];
        s[i] = s[y];
        s[y] = tmp;
        
        x = (x + 1) % key_len;
    }
        
    x = 0;
    y = 0;
        
    for (std::size_t i = 0; i < src_len; i++) {
        x = (x + 1) % 256;
        y = (s[x] + y) % 256;
            
        tmp = s[x];
        s[x] = s[y];
        s[y] = tmp;
            
        buf[i] ^= s[(s[x] + s[y]) % 256];
    }
        
    dest.insert(dest.end(), buf.begin(), buf.end());
}
    
    
void Daemonize(const std::string &working_dir) {
    pid_t pid;
//        struct rlimit rl;
//        int i;
//        int fd0, fd1, fd2;
        
    umask(0);
    
    if ((pid = fork()) < 0) {
        fprintf(stderr, "fork error!");
        exit(-1);
    } else if (pid > 0) {
        exit(0);
    }
    
    setsid();
    
    signal(SIGHUP, SIG_IGN);
    
    if ((pid = fork()) < 0) {
        exit(-1);
    } else if (pid > 0) {
        exit(0);
    }
    
//    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
//        fprintf(stderr, "can't get file limit.");
//    }
//    
//    if (rl.rlim_max == RLIM_INFINITY) {
//        rl.rlim_max = 1024;
//    }
//    
//    for (i = 0; i < (int)rl.rlim_max; i++) {
//        close(i);
//    }
//    
//    fd0 = open(log_file, O_RDWR);
//    fd1 = dup(0);
//    fd2 = dup(0);
    
    if (working_dir.empty()) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            chdir(cwd);
        } else {
            chdir("/");
        }
    } else {
        chdir(working_dir.c_str());
    }
}
    
int GetSysUid(char const *name) {
    if (!name) {
        return -1;
    }
    long const buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (-1 == buflen) {
        return -1;
    }
    // requires c99
    char buf[buflen];
    struct passwd pwbuf, *pwbufp;
    if (0 != getpwnam_r(name, &pwbuf, buf, buflen, &pwbufp) || !pwbufp) {
        return -1;
    }
    return pwbufp->pw_uid;
}
    
int GetSysGid(char const *name) {
    if (!name) {
        return -1;
    }
    long const buflen = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (-1 == buflen) {
        return -1;
    }
    // requires c99
    char buf[buflen];
    struct group grbuf, *grbufp;
    if (0 != getgrnam_r(name, &grbuf, buf, buflen, &grbufp) || !grbufp) {
        return -1;
    }
    return grbufp->gr_gid;
}
    
static int Htoi(const char *s) {
    int value;
    int c;
    
    c = ((unsigned char *)s)[0];
    if (isupper(c))
        c = tolower(c);
    value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

    c = ((unsigned char *)s)[1];
    if (isupper(c))
        c = tolower(c);
    value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;
    
    return value;
}

std::string URLDecode(const std::string &str) {
    std::string dest;
    
    const char *data = str.c_str();
    
    std::size_t len = str.length();
    while (len--) {
        if (*data == '+') {
            dest.push_back(' ');
        } else if (*data == '%'
            && len >= 2
            && isxdigit(*(data + 1))
            && isxdigit(*(data + 2))) {
            dest.push_back(Htoi(data + 1));
            data += 2;
            len -= 2;
        } else {
            dest.push_back(*data);
        }
        data++;
    }
    
    return dest;
}
    
std::string URLEncode(const std::string &str) {
    std::string dest;
    
    unsigned char c;
    
    std::size_t len = str.length();
    for (std::size_t i = 0; i < len; i++) {
        c = str[i];
        
        if (c == ' ') {
            dest.push_back('+');
        } else if ((c < '0' && c != '-' && c != '.')
                   || (c < 'A' && c > '9')
                   || (c > 'Z' && c < 'a' && c != '_')
                   || (c > 'z')) {
            dest.push_back('%');
            dest.push_back(hexchars[toascii(c) << 4]);
            dest.push_back(hexchars[toascii(c) & 0xf]);
        } else {
            dest.push_back(c);
        }
    }
    
    return dest;
}
    
std::string ExecutablePath() {
    char buf[1024] = {0};
#ifdef __linux__
    readlink("/proc/self/exe", buf, sizeof(buf));
#endif
    
#ifdef __FreeBSD__
    readlink("/proc/curproc/file", buf, sizeof(buf));
#endif
    
#ifdef __APPLE__
    proc_pidpath(getpid(), buf, sizeof(buf));
#endif
    
    char *p = strrchr(buf, '/');
    if (NULL == p) {
        return "";
    }
    *(p + 1) = '\0';
    
    return std::string(buf);
}
    
}//namespace util
}//namespace mevent
