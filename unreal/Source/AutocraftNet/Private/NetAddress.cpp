#include "NetSocket.h"

#include <cstdio>

namespace ac::net {

std::string Address::toString() const {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%u.%u.%u.%u:%u", unsigned(ip >> 24), unsigned((ip >> 16) & 0xFF),
                  unsigned((ip >> 8) & 0xFF), unsigned(ip & 0xFF), unsigned(port));
    return buf;
}

} // namespace ac::net
