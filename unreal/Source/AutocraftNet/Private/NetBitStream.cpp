#include "NetBitStream.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ac::net {

namespace {

constexpr double kTurn = 6.283185307179586;

uint64_t lowBits(int bits) { return bits >= 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1; }

} // namespace

int bitsForRange(uint64_t range) {
    int bits = 0;
    while (bits < 64 && (range >> bits) != 0) bits += 1;
    return bits;
}

uint32_t quantizeFloat(float value, float min, float max, int bits) {
    const double steps = static_cast<double>(lowBits(bits));
    if (!(max > min)) return 0;
    double t = (static_cast<double>(value) - min) / (static_cast<double>(max) - min);
    if (!(t > 0)) t = 0;  // NaN goes to min
    if (t > 1) t = 1;
    return static_cast<uint32_t>(std::floor(t * steps + 0.5));
}

float dequantizeFloat(uint32_t quantized, float min, float max, int bits) {
    const double steps = static_cast<double>(lowBits(bits));
    const double t = std::min(1.0, static_cast<double>(quantized) / steps);
    return static_cast<float>(min + t * (static_cast<double>(max) - min));
}

uint16_t quantizeAngle(float radians) {
    if (!std::isfinite(radians)) return 0;
    double turns = static_cast<double>(radians) / kTurn;
    turns -= std::floor(turns);
    return static_cast<uint16_t>(static_cast<uint32_t>(std::floor(turns * 65536.0 + 0.5)) & 0xFFFF);
}

float dequantizeAngle(uint16_t quantized) { return static_cast<float>(quantized * (kTurn / 65536.0)); }

void BitWriter::writeBits(uint64_t value, int bits) {
    if (bits <= 0) return;
    if (bits > 64) {
        // Past 64 the value has no more bits: zeros.
        writeBits(value, 64);
        writeBits(0, bits - 64);
        return;
    }
    value &= lowBits(bits);
    // The bytes past `bits_` are zero (only these writes add to them): the
    // value's bytes ORed in, the first at the bit offset, in one pass.
    const size_t end = bits_ + static_cast<size_t>(bits);
    bytes_.resize((end + 7) / 8, 0);
    uint8_t* b = bytes_.data() + bits_ / 8;
    const int offset = static_cast<int>(bits_ % 8);
    *b |= static_cast<uint8_t>(value << offset);
    for (int done = 8 - offset; done < bits; done += 8) *++b |= static_cast<uint8_t>(value >> done);
    bits_ = end;
}

void BitWriter::writeBytes(const uint8_t* data, size_t size) {
    if (bits_ % 8 == 0) {
        bytes_.insert(bytes_.end(), data, data + size);
        bits_ += size * 8;
        return;
    }
    for (size_t i = 0; i < size; i++) writeBits(data[i], 8);
}

void BitWriter::writeInt(int64_t value, int64_t min, int64_t max) {
    if (max < min) max = min;
    if (value < min) value = min;
    if (value > max) value = max;
    const uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min);
    writeBits(static_cast<uint64_t>(value) - static_cast<uint64_t>(min), bitsForRange(range));
}

void BitWriter::align() {
    if (bits_ % 8 != 0) writeBits(0, static_cast<int>(8 - bits_ % 8));
}

uint64_t BitReader::readBits(int bits) {
    if (bits <= 0) return 0;
    if (!ok_ || static_cast<size_t>(bits) > bitsLeft()) {
        ok_ = false;
        bit_ = size_ * 8;
        return 0;
    }
    // The bytes the bits lie in, no further, low bits first.
    const uint8_t* b = data_ + bit_ / 8;
    const int offset = static_cast<int>(bit_ % 8);
    uint64_t value = static_cast<uint64_t>(*b) >> offset;
    for (int done = 8 - offset; done < bits; done += 8) value |= static_cast<uint64_t>(*++b) << done;
    bit_ += static_cast<size_t>(bits);
    return value & lowBits(bits);
}

bool BitReader::readBytes(uint8_t* out, size_t size) {
    if (!ok_ || size * 8 > bitsLeft()) {
        ok_ = false;
        bit_ = size_ * 8;
        return false;
    }
    if (bit_ % 8 == 0) {
        if (size > 0) std::memcpy(out, data_ + bit_ / 8, size);
        bit_ += size * 8;
        return true;
    }
    for (size_t i = 0; i < size; i++) out[i] = readU8();
    return true;
}

int64_t BitReader::readInt(int64_t min, int64_t max) {
    if (max < min) max = min;
    const uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min);
    const uint64_t raw = readBits(bitsForRange(range));
    return static_cast<int64_t>(static_cast<uint64_t>(min) + std::min(raw, range));
}

void BitReader::align() {
    if (bit_ % 8 != 0) readBits(static_cast<int>(8 - bit_ % 8));
}

} // namespace ac::net
