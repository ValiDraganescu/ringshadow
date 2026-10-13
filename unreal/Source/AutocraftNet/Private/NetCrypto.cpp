#include "NetCrypto.h"

#include "NetPacket.h"
#include "NetSocket.h"

#include "../ThirdParty/monocypher/monocypher.h"

#include <algorithm>

namespace ac::net::crypto {

KeyPair::~KeyPair() { wipe(); }

void KeyPair::wipe() { crypto_wipe(secret.data(), secret.size()); }

bool makeKeyPair(KeyPair& out) {
    if (!secureRandom(out.secret.data(), out.secret.size())) return false;
    crypto_x25519_public_key(out.publicKey.data(), out.secret.data());
    return true;
}

bool deriveKeys(const Key& mySecret, const Key& theirPublic, const Key& clientPublic, const Key& serverPublic,
                uint64_t cookie, SessionKeys& out) {
    uint8_t shared[32];
    crypto_x25519(shared, mySecret.data(), theirPublic.data());
    uint8_t any = 0;
    for (uint8_t b : shared) any |= b;
    if (any == 0) {
        crypto_wipe(shared, sizeof shared);
        return false;
    }
    uint8_t cookieBytes[8];
    for (int i = 0; i < 8; i++) cookieBytes[i] = static_cast<uint8_t>(cookie >> (8 * i));
    crypto_blake2b_ctx ctx;
    crypto_blake2b_init(&ctx, 64);
    crypto_blake2b_update(&ctx, shared, sizeof shared);
    crypto_blake2b_update(&ctx, clientPublic.data(), clientPublic.size());
    crypto_blake2b_update(&ctx, serverPublic.data(), serverPublic.size());
    crypto_blake2b_update(&ctx, cookieBytes, sizeof cookieBytes);
    uint8_t hash[64];
    crypto_blake2b_final(&ctx, hash);
    for (size_t i = 0; i < kKeyBytes; i++) {
        out.clientToServer[i] = hash[i];
        out.serverToClient[i] = hash[kKeyBytes + i];
    }
    crypto_wipe(shared, sizeof shared);
    crypto_wipe(hash, sizeof hash);
    return true;
}

Nonce nonce(Direction direction, uint64_t counter) {
    Nonce n{};
    n[0] = static_cast<uint8_t>(direction);
    for (int i = 0; i < 8; i++) n[8 + i] = static_cast<uint8_t>(counter >> (8 * i));
    return n;
}

void seal(const Key& key, Direction direction, uint64_t counter, const uint8_t* packet, size_t headerBytes,
          size_t size, std::vector<uint8_t>& out) {
    const size_t text = size - headerBytes;
    out.resize(size + kMacBytes);
    std::copy(packet, packet + headerBytes, out.begin());
    const Nonce n = nonce(direction, counter);
    crypto_aead_lock(out.data() + headerBytes, out.data() + size, key.data(), n.data(), packet, headerBytes,
                     packet + headerBytes, text);
}

bool open(const Key& key, Direction direction, uint64_t counter, const uint8_t* packet, size_t headerBytes,
          size_t size, std::vector<uint8_t>& out) {
    out.clear();
    if (size < headerBytes + kMacBytes) return false;
    const size_t text = size - headerBytes - kMacBytes;
    out.resize(text);
    const Nonce n = nonce(direction, counter);
    if (crypto_aead_unlock(out.data(), packet + headerBytes + text, key.data(), n.data(), packet, headerBytes,
                           packet + headerBytes, text) != 0) {
        out.clear();
        return false;
    }
    return true;
}

uint64_t widenCounter(uint64_t newest, uint16_t sequence) {
    uint64_t c = (newest & ~uint64_t(0xFFFF)) | sequence;
    if (c > newest + 0x8000 && c >= 0x10000) c -= 0x10000;
    else if (c + 0x8000 < newest) c += 0x10000;
    return c;
}

bool ReplayWindow::fresh(uint64_t counter) const {
    if (!any_ || counter > newest_) return true;
    if (newest_ - counter >= kSize) return false;
    return !bit(counter);
}

void ReplayWindow::record(uint64_t counter) {
    if (!any_ || counter > newest_) {
        if (!any_ || counter - newest_ >= kSize) {
            bits_.fill(0);
        } else {
            for (uint64_t c = newest_ + 1; c < counter; c++) bits_[(c % kSize) / 64] &= ~(uint64_t(1) << (c % 64));
        }
        any_ = true;
        newest_ = counter;
    }
    bits_[(counter % kSize) / 64] |= uint64_t(1) << (counter % 64);
}

void wipe(void* data, size_t size) { crypto_wipe(data, size); }

} // namespace ac::net::crypto
