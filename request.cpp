#include "request.h"
#include "connection.h"
#include "util.h"
#include "event_loop.h"

#include <arpa/inet.h>

#include <string.h>

namespace mevent {
    
//https://github.com/nodejs/http-parser/blob/master/http_parser.c
static const char tokens[256] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0,       0,       '\n',    0,       0,       '\r',    0,       0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0,       0,       0,       0,       0,       0,       0,       0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0,      '!',      0,      '#',     '$',     '%',     '&',    '\'',
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    '(',     ')',   '*',     '+',    ',',      '-',     '.',      '/',
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    '8',     '9',    ':',     ';',     '<',     '=',     '>',     '?',
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    '@',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    'x',     'y',     'z',    '[',       0,     ']',      '^',     '_',
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    'x',     'y',     'z',    '{',      '|',    '}',      '~',       0 };
    
    
    
static const uint8_t normal_url_char[32] = {
    /*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
    0    |   0   |    0    |   0    |    0   |   0    |   0    |   0,
    /*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
    0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
    /*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
    0    |   2    |   4    |   0    |   16   |   32   |   64   |  128,
    /*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |   0,
    /*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
    /* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
    1    |   2    |   4    |   8    |   16   |   32   |   64   |   0, };
    
#define TOKEN(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
    
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
    
#define CONTENT_LENGTH      "content-length"
#define UPGRADE             "upgrade"
#define SEC_WEBSOCKET_KEY   "sec-websocket-key"
#define CONTENT_TYPE        "content-type"
    
#define METHOD_GET            "get"
#define METHOD_POST           "post"
#define METHOD_HEAD           "head"
#define METHOD_PUT            "put"
#define METHOD_DELETE         "delete"
#define METHOD_CONNECT        "connect"
#define METHOD_OPTION         "options"
#define METHOD_TRACE          "trace"
#define METHOD_PATCH          "patch"
    
#define CR                  '\r'
#define LF                  '\n'
    
#ifndef BIT_AT
# define BIT_AT(a, i)                                                \
(!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
(1 << ((unsigned int) (i) & 7))))
#endif
    
#define IS_URL_CHAR(c)      (BIT_AT(normal_url_char, (unsigned char)c))

    
#define READ_BUFFER_SIZE 2048

Request::Request(Connection *conn) : conn_(conn) {
    Reset();
}
    
void Request::ParseHeader() {
    char *pos = strchr(const_cast<char *>(rbuf_.c_str()), '\n');
    bool is_field = true;

    std::string field;
    std::string value;
    for (; *pos; pos++) {
        if (*pos == ':' && is_field) {
            is_field = false;
            continue;
        } else if (*pos == '\r') {
            continue;
        } else if (*pos == '\n') {
            if (!field.empty() && !value.empty()) {
                header_map_[field] = value;
            }
            field.clear();
            value.clear();
            is_field = true;
            continue;
        }
        
        if (is_field) {
            field.push_back(*pos);
        } else {
            if (value.empty() && *pos == ' ') { //trim
                continue;
            }
            value.push_back(*pos);
        }
    }
}
    
std::string Request::HeaderValue(const std::string &field) {
    std::string value;
    
    auto it = header_map_.find(field);
    if (it != header_map_.end()) {
        value = it->second;
    }
    
    return value;
}
    
void Request::ParseQueryString() {
    if (query_string_.empty()) {
        return;
    }
    
    ParseFormUrlencoded(get_form_map_, query_string_);
}
    
std::string Request::QueryStringValue(const std::string &field) {
    std::string value;
    
    auto it = get_form_map_.find(field);
    if (it != get_form_map_.end()) {
        value = it->second;
    }
    
    return value;
}
    
void Request::ParsePostForm() {
    if (content_type_ != "application/x-www-form-urlencoded") {
        return;
    }
    
    ParseFormUrlencoded(post_form_map_, rbuf_.substr(header_len_));
}
    
std::string Request::PostFormValue(const std::string &field) {
    std::string value;
    
    auto it = post_form_map_.find(field);
    if (it != post_form_map_.end()) {
        value = it->second;
    }
    
    return value;
}
    
uint32_t Request::ContentLength() {
    return content_length_;
}
    
std::string Request::Body() {
    return rbuf_.substr(header_len_);
}
    
std::string Request::Path() {
    return path_;
}
    
std::string Request::QueryString() {
    return query_string_;
}
    
RequestMethod Request::Method() {
    return method_;
}
    
std::string Request::RemoteAddr() {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_, buf, INET_ADDRSTRLEN);
    return std::string(buf);
}
    
void Request::ParseFormUrlencoded(std::map<std::string, std::string> &m, const std::string &str) {
    bool is_field = true;
    
    std::string field;
    std::string value;
    const char *pos = str.c_str();
    for (; *pos; pos++) {
        if (*pos == '=') {
            is_field = false;
            continue;
        } else if (*pos == '&') {
            if (!field.empty() && !value.empty()) {
                m[field] = util::URLDecode(value);
            }
            field.clear();
            value.clear();
            is_field = true;
            continue;
        }
        
        if (is_field) {
            field.push_back(*pos);
        } else {
            value.push_back(*pos);
        }
    }
    
    if (!field.empty() && !value.empty()) {
        m[field] = util::URLDecode(value);
    }
}

void Request::Reset() {
    status_ = RequestStatus::HEADER_RECEIVING;
    
    bzero(&addr_, sizeof(addr_));
    
    rbuf_.clear();
    std::string().swap(rbuf_);
    rbuf_len_ = 0;
    
    method_ = RequestMethod::UNKNOWN;
    
    path_.clear();
    std::string().swap(path_);
    query_string_.clear();
    std::string().swap(query_string_);
    
    content_length_ = 0;
    header_len_ = 0;
    
    content_type_.clear();
    std::string().swap(content_type_);
    
    parse_status_ = RequestParseStatus::S_START;
    parse_offset_ = 0;
    parse_match_ = 0;
    
    error_code_ = 0;
    
    sec_websocket_key_.clear();
    std::string().swap(sec_websocket_key_);
    
    header_map_.clear();
    std::map<std::string, std::string>().swap(header_map_);
    get_form_map_.clear();
    std::map<std::string, std::string>().swap(get_form_map_);
    post_form_map_.clear();
    std::map<std::string, std::string>().swap(post_form_map_);
}
    
void Request::Keepalive()
{
    status_ = RequestStatus::HEADER_RECEIVING;
    rbuf_.clear();
    rbuf_len_ = 0;
}
    
ConnStatus Request::ReadData() {
    char buf[READ_BUFFER_SIZE];
    ssize_t n;
    
    if (status_ != RequestStatus::HEADER_RECEIVING && status_ != RequestStatus::BODY_RECEIVING) {
        return ConnStatus::ERROR;
    }
    
    if (status_ == RequestStatus::HEADER_RECEIVING) {
        n = conn_->Readn(buf, READ_BUFFER_SIZE);
        do {
            if (n > 0) {
                rbuf_len_ += n;
                if (rbuf_len_ > conn_->elp_->max_header_size_) {
                    error_code_ = 500;
                    return ConnStatus::ERROR;
                }
                
                rbuf_.append(buf, n);
                
                HTTPParserStatus parse_status = Parse();
                if (parse_status == HTTPParserStatus::FINISHED) {
                    status_ = RequestStatus::BODY_RECEIVING;
                    break;
                } else if (parse_status == HTTPParserStatus::ERROR) {
                    error_code_ = 400;
                    return ConnStatus::ERROR;
                }
            } else if (n < 0) {
                return ConnStatus::CLOSE;
            }
        } while (n == READ_BUFFER_SIZE);
    }
    
    if (status_ == RequestStatus::BODY_RECEIVING) {
        if (content_length_ > conn_->elp_->max_post_size_) {
            error_code_ = 500;
            return ConnStatus::ERROR;
        }
        
        if (rbuf_len_ - header_len_ >= content_length_) {
            status_ = RequestStatus::BODY_RECEIVED;
        } else {
            n = conn_->Readn(buf, READ_BUFFER_SIZE);
            do {
                if (n > 0) {
                    rbuf_len_ += n;
                    rbuf_.append(buf, n);
                    
                    if (rbuf_len_ - header_len_ >= content_length_) {
                        status_ = RequestStatus::BODY_RECEIVED;
                        break;
                    }
                } else if (n < 0) {
                    return ConnStatus::CLOSE;
                }
            } while (n == READ_BUFFER_SIZE);
        }
    }
    
    if (status_ == RequestStatus::BODY_RECEIVED) {
        conn_->TaskPush();
    }
    
    return ConnStatus::AGAIN;
}
    
HTTPParserStatus Request::Parse() {
    HTTPParserStatus status = HTTPParserStatus::CONTINUE;
    
    while (parse_offset_ < rbuf_len_) {
        char ch = rbuf_[parse_offset_];
        
        char c = TOKEN(ch);
        if (!c) {
            status = HTTPParserStatus::ERROR;
            break;
        }
        
        if (parse_status_ == RequestParseStatus::S_EOL) {
            if (c == LF) {
                if (rbuf_[parse_offset_ - 1] == CR) {
                    parse_status_ = RequestParseStatus::S_HEADER_FIELD;
                    parse_match_ = parse_offset_;
                } else {
                    status = HTTPParserStatus::ERROR;
                    break;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_START) {
            parse_status_ = RequestParseStatus::S_METHOD;
            if (c == 'g') {
                method_ = RequestMethod::GET;
            } else if (c == 'p') {
                parse_status_ = RequestParseStatus::S_METHOD_P;
            } else if (c == 'h') {
                method_ = RequestMethod::HEAD;
            } else if (c == 'd') {
                method_ = RequestMethod::DELETE;
            } else if (c == 'c') {
                method_ = RequestMethod::CONNECT;
            } else if (c == 'o') {
                method_ = RequestMethod::OPTIONS;
            } else if (c == 't') {
                method_ = RequestMethod::TRACE;
            } else if (c == 'p') {
                method_ = RequestMethod::PATCH;
            } else {
                parse_status_ = RequestParseStatus::S_START;
                status = HTTPParserStatus::ERROR;
                break;
            }
        } else if (parse_status_ == RequestParseStatus::S_METHOD) {
            const char *method = NULL;
            if (method_ == RequestMethod::GET) {
                method = METHOD_GET;
            } else if (method_ == RequestMethod::POST) {
                method = METHOD_POST;
            } else if (method_ == RequestMethod::HEAD) {
                method = METHOD_HEAD;
            } else if (method_ == RequestMethod::PUT) {
                method = METHOD_PUT;
            } else if (method_ == RequestMethod::DELETE) {
                method = METHOD_DELETE;
            } else if (method_ == RequestMethod::CONNECT) {
                method = METHOD_CONNECT;
            } else if (method_ == RequestMethod::OPTIONS) {
                method = METHOD_OPTION;
            } else if (method_ == RequestMethod::TRACE) {
                method = METHOD_TRACE;
            } else if (method_ == RequestMethod::PATCH) {
                method = METHOD_PATCH;
            } else {
                status = HTTPParserStatus::ERROR;
                break;
            }
            
            size_t len = strlen(method);
            
            if (parse_offset_ > len - 1) {
                if (c != ' ') {
                    if (parse_offset_ == len) {
                        status = HTTPParserStatus::ERROR;
                        break;
                    } else {
                        parse_status_ = RequestParseStatus::S_PATH;
                        parse_match_ = parse_offset_;
                        continue;
                    }
                }
            } else {
                if (method[parse_offset_ - parse_match_] != c) {
                    status = HTTPParserStatus::ERROR;
                    break;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_METHOD_P) {
            parse_status_ = RequestParseStatus::S_METHOD;
            if (c == 'o') {
                method_ = RequestMethod::POST;
            } else if (c == 'u') {
                method_ = RequestMethod::PUT;
            } else if (c == 'a') {
                method_ = RequestMethod::PATCH;
            } else {
                status = HTTPParserStatus::ERROR;
                break;
            }
        } else if (parse_status_ == RequestParseStatus::S_PATH) {
            if (ch != ' ') {
                if (IS_URL_CHAR(ch)) {
                    path_.push_back(ch);
                } else {
                    if (ch == '?') {
                        parse_status_ = RequestParseStatus::S_QUERY_STRING;
                        parse_offset_++;
                        continue;
                    } else {
                        status = HTTPParserStatus::ERROR;
                        break;
                    }
                }
            } else {
                if (!path_.empty()) {
                    parse_status_ = RequestParseStatus::S_EOL;
                } else {
                    status = HTTPParserStatus::ERROR;
                    break;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_QUERY_STRING) {
            if (ch != ' ') {
                if (IS_URL_CHAR(ch)) {
                    query_string_.push_back(ch);
                } else {
                    status = HTTPParserStatus::ERROR;
                    break;
                }
            } else {
                parse_status_ = RequestParseStatus::S_EOL;
            }
        } else if (parse_status_ == RequestParseStatus::S_HEADER_FIELD) {
            if (c == CR) {
                parse_status_ = RequestParseStatus::S_EOH;
            } else if (c == 'c') {
                if (rbuf_len_ - parse_offset_ < 9) {
                    parse_offset_++;
                    continue;
                }
                
                if (TOKEN(rbuf_[parse_offset_ + 1]) == 'o'
                    && TOKEN(rbuf_[parse_offset_ + 2]) == 'n'
                    && TOKEN(rbuf_[parse_offset_ + 3]) == 't'
                    && TOKEN(rbuf_[parse_offset_ + 4]) == 'e'
                    && TOKEN(rbuf_[parse_offset_ + 5]) == 'n'
                    && TOKEN(rbuf_[parse_offset_ + 6]) == 't'
                    && TOKEN(rbuf_[parse_offset_ + 7]) == '-') {
                    
                    if (TOKEN(rbuf_[parse_offset_ + 8]) == 'l') {
                        parse_status_ = RequestParseStatus::S_CONTENT_LENGTH;
                        parse_match_ = parse_offset_;
                    } else if (TOKEN(rbuf_[parse_offset_ + 8]) == 't') {
                        parse_status_ = RequestParseStatus::S_CONTENT_TYPE;
                        parse_match_ = parse_offset_;
                    } else {
                        parse_status_ = RequestParseStatus::S_EOL;
                    }
                } else {
                    parse_status_ = RequestParseStatus::S_EOL;
                }
            } else if (c == 's') {
                parse_status_ = RequestParseStatus::S_SEC_WEBSOCKET_KEY;
                parse_match_ = parse_offset_;
            } else {
                parse_status_ = RequestParseStatus::S_EOL;
            }
        } else if (parse_status_ == RequestParseStatus::S_EOH) {
            if (c == LF) {
                header_len_ = parse_offset_ + 1;
                status = HTTPParserStatus::FINISHED;
            } else {
                status = HTTPParserStatus::ERROR;
            }
            break;
        } else if (parse_status_ == RequestParseStatus::S_CONTENT_LENGTH) {
            if (parse_offset_ - parse_match_ > 13) {
                if (c != ' ' && c != ':') {
                    parse_status_ = RequestParseStatus::S_CONTNET_LENGTH_V;
                    parse_match_ = parse_offset_;
                    continue;
                }
            } else {
                if (CONTENT_LENGTH[parse_offset_ - parse_match_] != c) {
                    parse_status_ = RequestParseStatus::S_EOL;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_CONTNET_LENGTH_V) {
            if (c == ' ' || c == CR) {
                parse_status_ = RequestParseStatus::S_EOL;
            } else {
                if (!IS_NUM(c)) {
                    status = HTTPParserStatus::ERROR;
                    break;
                }
                
                content_length_ *= 10;
                content_length_ += c - '0';
            }
        } else if (parse_status_ == RequestParseStatus::S_SEC_WEBSOCKET_KEY) {
            if (parse_offset_ - parse_match_ > 16) {
                if (c != ' ' && c != ':') {
                    parse_status_ = RequestParseStatus::S_SEC_WEBSOCKET_KEY_V;
                    parse_match_ = parse_offset_;
                    continue;
                }
            } else {
                if (SEC_WEBSOCKET_KEY[parse_offset_ - parse_match_] != c) {
                    parse_status_ = RequestParseStatus::S_EOL;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_SEC_WEBSOCKET_KEY_V) {
            if (c == ' ' || c == CR) {
                parse_status_ = RequestParseStatus::S_EOL;
            } else {
                sec_websocket_key_.push_back(ch);
            }
        } else if (parse_status_ == RequestParseStatus::S_CONTENT_TYPE) {
            if (parse_offset_ - parse_match_ > 11) {
                if (c != ' ' && c != ':') {
                    parse_status_ = RequestParseStatus::S_CONTENT_TYPE_V;
                    parse_match_ = parse_offset_;
                    continue;
                }
            } else {
                if (CONTENT_TYPE[parse_offset_ - parse_match_] != c) {
                    parse_status_ = RequestParseStatus::S_EOL;
                }
            }
        } else if (parse_status_ == RequestParseStatus::S_CONTENT_TYPE_V) {
            if (c == ' ' || c == CR) {
                parse_status_ = RequestParseStatus::S_EOL;
            } else {
                content_type_.push_back(ch);
            }
        }
        
        parse_offset_++;
    }
    
    return status;
}
    
}//namespace mevent
