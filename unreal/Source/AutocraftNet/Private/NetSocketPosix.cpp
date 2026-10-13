// UdpSocket and secureRandom on POSIX sockets: the Mac and Linux. Windows
// gets its own file on Winsock; until then the socket does not exist there.
#if !defined(_WIN32)

#include "NetSocket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <cstdlib>
#else
#include <sys/random.h>
#endif

namespace ac::net {

namespace {

sockaddr_in toSockaddr(const Address& a) {
    sockaddr_in s;
    std::memset(&s, 0, sizeof s);
    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(a.ip);
    s.sin_port = htons(a.port);
#if defined(__APPLE__)
    s.sin_len = sizeof s;
#endif
    return s;
}

Address fromSockaddr(const sockaddr_in& s) { return Address{ntohl(s.sin_addr.s_addr), ntohs(s.sin_port)}; }

std::string errnoText(const char* what) { return std::string(what) + ": " + std::strerror(errno); }

} // namespace

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(const Address& local) {
    close();
    const int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        error_ = errnoText("socket");
        return false;
    }
    // Room for a burst from many clients between two ticks.
    const int buffer = 1 << 20;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof buffer);
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer, sizeof buffer);
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        error_ = errnoText("fcntl");
        ::close(fd);
        return false;
    }
    const sockaddr_in addr = toSockaddr(local);
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof addr) < 0) {
        error_ = errnoText("bind");
        ::close(fd);
        return false;
    }
    sockaddr_in bound;
    socklen_t len = sizeof bound;
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &len) < 0) {
        error_ = errnoText("getsockname");
        ::close(fd);
        return false;
    }
    handle_ = fd;
    local_ = fromSockaddr(bound);
    error_.clear();
    return true;
}

void UdpSocket::close() {
    if (handle_ >= 0) ::close(static_cast<int>(handle_));
    handle_ = -1;
}

bool UdpSocket::send(double, const Address& to, const uint8_t* data, size_t size) {
    if (handle_ < 0) return false;
    const sockaddr_in addr = toSockaddr(to);
    const ssize_t sent =
        ::sendto(static_cast<int>(handle_), data, size, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof addr);
    if (sent < 0) {
        error_ = errnoText("sendto");
        return false;
    }
    return static_cast<size_t>(sent) == size;
}

bool UdpSocket::receive(double, Address& from, uint8_t* buffer, size_t capacity, size_t& size) {
    if (handle_ < 0) return false;
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    const ssize_t got =
        ::recvfrom(static_cast<int>(handle_), buffer, capacity, 0, reinterpret_cast<sockaddr*>(&addr), &len);
    if (got < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) error_ = errnoText("recvfrom");
        return false;
    }
    from = fromSockaddr(addr);
    size = static_cast<size_t>(got);
    return true;
}

bool secureRandom(uint8_t* out, size_t size) {
#if defined(__APPLE__)
    arc4random_buf(out, size);
    return true;
#else
    while (size > 0) {
        const ssize_t got = ::getrandom(out, size, 0);
        if (got < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        out += got;
        size -= static_cast<size_t>(got);
    }
    return true;
#endif
}

} // namespace ac::net

#endif
