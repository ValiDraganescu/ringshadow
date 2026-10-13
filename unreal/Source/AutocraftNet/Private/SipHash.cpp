#include "SipHash.h"

namespace ac::net {

namespace {

constexpr uint64_t rotl(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }

uint64_t load64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

struct State {
    uint64_t v0, v1, v2, v3;

    void round() {
        v0 += v1;
        v1 = rotl(v1, 13);
        v1 ^= v0;
        v0 = rotl(v0, 32);
        v2 += v3;
        v3 = rotl(v3, 16);
        v3 ^= v2;
        v0 += v3;
        v3 = rotl(v3, 21);
        v3 ^= v0;
        v2 += v1;
        v1 = rotl(v1, 17);
        v1 ^= v2;
        v2 = rotl(v2, 32);
    }

    void absorb(uint64_t m) {
        v3 ^= m;
        round();
        round();
        v0 ^= m;
    }
};

} // namespace

uint64_t sipHash24(const uint8_t key[16], const uint8_t* data, size_t size) {
    const uint64_t k0 = load64(key), k1 = load64(key + 8);
    State s{k0 ^ 0x736f6d6570736575ULL, k1 ^ 0x646f72616e646f6dULL, k0 ^ 0x6c7967656e657261ULL,
            k1 ^ 0x7465646279746573ULL};
    const size_t whole = size - size % 8;
    for (size_t i = 0; i < whole; i += 8) s.absorb(load64(data + i));
    uint64_t last = uint64_t(size & 0xFF) << 56;
    for (size_t i = 0; i < size % 8; i++) last |= uint64_t(data[whole + i]) << (8 * i);
    s.absorb(last);
    s.v2 ^= 0xFF;
    for (int i = 0; i < 4; i++) s.round();
    return s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
}

} // namespace ac::net
