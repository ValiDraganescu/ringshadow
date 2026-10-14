// Where packets come from and go to. The endpoints only know `PacketIo`: a
// UDP socket (`UdpSocket`, the one piece of platform code, POSIX on the Mac
// and Linux; Winsock drops in beside it), the lag simulator wrapping one,
// or the tests' in-memory network. Every call takes the time from the
// caller: nothing in the library reads a clock.
#pragma once

#include "AutocraftNetApi.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace ac::net {

/// An IPv4 address and port, both in host byte order.
struct Address {
    uint32_t ip = 0;
    uint16_t port = 0;

    static constexpr Address ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) {
        return Address{(uint32_t(a) << 24) | (uint32_t(b) << 16) | (uint32_t(c) << 8) | uint32_t(d), port};
    }
    static constexpr Address loopback(uint16_t port) { return ipv4(127, 0, 0, 1, port); }
    /// One number per address, for maps.
    constexpr uint64_t key() const { return (uint64_t(ip) << 16) | port; }
    constexpr bool operator==(const Address& o) const { return ip == o.ip && port == o.port; }
    constexpr bool operator!=(const Address& o) const { return !(*this == o); }
    /// "127.0.0.1:4000".
    AUTOCRAFTNET_API std::string toString() const;
};

class AUTOCRAFTNET_API PacketIo {
public:
    virtual ~PacketIo() = default;
    /// Sends one datagram; false if it could not go out.
    virtual bool send(double now, const Address& to, const uint8_t* data, size_t size) = 0;
    /// The next datagram that has arrived, if any. Never waits.
    virtual bool receive(double now, Address& from, uint8_t* buffer, size_t capacity, size_t& size) = 0;
};

/// A non-blocking UDP socket.
class AUTOCRAFTNET_API UdpSocket final : public PacketIo {
public:
    UdpSocket() = default;
    ~UdpSocket() override;
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    /// Binds to `local` (port 0: the system picks a free port). False, with
    /// error() saying why, if it cannot.
    bool open(const Address& local);
    void close();
    bool isOpen() const { return handle_ >= 0; }
    /// The address bound, with the port the system picked.
    const Address& localAddress() const { return local_; }
    const std::string& error() const { return error_; }

    bool send(double now, const Address& to, const uint8_t* data, size_t size) override;
    bool receive(double now, Address& from, uint8_t* buffer, size_t capacity, size_t& size) override;
    /// Waits up to `seconds` for a datagram to arrive (true: one is there to
    /// read). The idle world server sleeps on it (S2).
    bool wait(double seconds) const;

private:
    intptr_t handle_ = -1;
    Address local_;
    std::string error_;
};

/// `size` bytes from the system's secure random source: the server's secret,
/// the local server's one-time token. False if the source failed.
AUTOCRAFTNET_API bool secureRandom(uint8_t* out, size_t size);

} // namespace ac::net
