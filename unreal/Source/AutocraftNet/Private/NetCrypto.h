// The connection's cryptography (docs/networking.md, "Encryption"). Every
// primitive is Monocypher's (ThirdParty/monocypher); this file only puts
// them together:
//
// - Each end makes an X25519 key pair per connection. The client's public
//   key goes in its answer, the server's in its welcome.
// - The session keys are BLAKE2b-512 of the shared secret, the client's and
//   the server's public keys and the connection's cookie: the first 32 bytes
//   seal what the client sends, the last 32 what the server sends.
// - A packet is sealed with XChaCha20-Poly1305: the header is signed, the
//   payload encrypted, the 16-byte tag appended. The nonce is the direction
//   and the sender's 64-bit packet counter, which never goes back; the wire
//   carries its low 16 bits (the sequence) and the receiver widens them
//   from the newest counter it opened.
// - A counter opened once, or older than the replay window, is dropped.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ac::net::crypto {

inline constexpr size_t kKeyBytes = 32;
inline constexpr size_t kNonceBytes = 24;

enum class Direction : uint8_t { clientToServer = 0, serverToClient = 1 };

using Key = std::array<uint8_t, kKeyBytes>;
using Nonce = std::array<uint8_t, kNonceBytes>;

/// An X25519 key pair; the secret is wiped when it goes.
struct KeyPair {
    Key secret{};
    Key publicKey{};

    KeyPair() = default;
    KeyPair(const KeyPair&) = delete;
    KeyPair& operator=(const KeyPair&) = delete;
    ~KeyPair();
    void wipe();
};

/// A fresh key pair from secureRandom; false if the source failed.
bool makeKeyPair(KeyPair& out);

struct SessionKeys {
    Key clientToServer{};
    Key serverToClient{};

    const Key& sending(Direction d) const { return d == Direction::clientToServer ? clientToServer : serverToClient; }
};

/// Both ends get the same keys from their own secret and the other's public
/// key. False if the shared secret is all zero (a public key of low order).
bool deriveKeys(const Key& mySecret, const Key& theirPublic, const Key& clientPublic, const Key& serverPublic,
                uint64_t cookie, SessionKeys& out);

Nonce nonce(Direction direction, uint64_t counter);

/// `packet` is a header of `headerBytes` and the plain payload after it;
/// `out` becomes the header, the encrypted payload and the tag.
void seal(const Key& key, Direction direction, uint64_t counter, const uint8_t* packet, size_t headerBytes,
          size_t size, std::vector<uint8_t>& out);
/// The other way: false, `out` cleared, unless the tag matches.
bool open(const Key& key, Direction direction, uint64_t counter, const uint8_t* packet, size_t headerBytes,
          size_t size, std::vector<uint8_t>& out);

/// The full counter whose low 16 bits are `sequence`, nearest `newest` (the
/// newest opened so far; 0 before any).
uint64_t widenCounter(uint64_t newest, uint16_t sequence);

/// The counters opened lately: the newest and the 1,024 before it.
class ReplayWindow {
public:
    static constexpr uint64_t kSize = 1024;

    bool any() const { return any_; }
    uint64_t newest() const { return newest_; }
    /// False if `counter` was recorded already or is too old to tell.
    bool fresh(uint64_t counter) const;
    void record(uint64_t counter);

private:
    bool bit(uint64_t counter) const { return (bits_[(counter % kSize) / 64] >> (counter % 64)) & 1; }

    bool any_ = false;
    uint64_t newest_ = 0;
    std::array<uint64_t, kSize / 64> bits_{};
};

/// Wipes a buffer that held a secret.
void wipe(void* data, size_t size);

} // namespace ac::net::crypto
