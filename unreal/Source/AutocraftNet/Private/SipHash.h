// SipHash-2-4, the keyed hash of the handshake's cookie and the connection
// ids: a short, well-known pseudo-random function, written out from its
// reference description and checked against its published test vectors.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ac::net {

uint64_t sipHash24(const uint8_t key[16], const uint8_t* data, size_t size);

} // namespace ac::net
