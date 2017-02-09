#ifndef _BASE64_H
#define _BASE64_H

#include <string>

namespace mevent {

std::string Base64Encode(unsigned char const* , unsigned int len);
std::string Base64Decode(std::string const& s);

}//namespace mevent

#endif
