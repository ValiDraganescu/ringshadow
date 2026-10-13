// The bit-packing writer and reader every packet and message is written
// with (docs/networking.md, "The messages"): values take only the bits they
// need, bits go in least significant first. The reader never reads past its
// end: a read that would returns 0 and marks the reader failed.
#pragma once

#include "AutocraftNetApi.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ac::net {

/// The bits needed for every value in 0...range.
AUTOCRAFTNET_API int bitsForRange(uint64_t range);

/// `value` clamped to min...max, as an integer in `bits` bits (1...32):
/// 0 is min, all ones is max.
AUTOCRAFTNET_API uint32_t quantizeFloat(float value, float min, float max, int bits);
AUTOCRAFTNET_API float dequantizeFloat(uint32_t quantized, float min, float max, int bits);

/// An angle in 16 bits a full turn; any angle in radians goes in, 0...2π
/// comes out.
AUTOCRAFTNET_API uint16_t quantizeAngle(float radians);
AUTOCRAFTNET_API float dequantizeAngle(uint16_t quantized);

class AUTOCRAFTNET_API BitWriter {
public:
    /// The low `bits` (0...64) of `value`.
    void writeBits(uint64_t value, int bits);
    void writeBool(bool value) { writeBits(value ? 1 : 0, 1); }
    void writeU8(uint8_t value) { writeBits(value, 8); }
    void writeU16(uint16_t value) { writeBits(value, 16); }
    void writeU32(uint32_t value) { writeBits(value, 32); }
    void writeU64(uint64_t value) { writeBits(value, 64); }
    /// Whole bytes; fast when the writer stands on a byte boundary.
    void writeBytes(const uint8_t* data, size_t size);
    /// `value` clamped to min...max, in the bits that range needs.
    void writeInt(int64_t value, int64_t min, int64_t max);
    void writeFloat(float value, float min, float max, int bits) { writeBits(quantizeFloat(value, min, max, bits), bits); }
    void writeAngle(float radians) { writeBits(quantizeAngle(radians), 16); }
    /// Zero bits up to the next byte boundary.
    void align();

    size_t bitCount() const { return bits_; }
    size_t byteCount() const { return (bits_ + 7) / 8; }
    const std::vector<uint8_t>& bytes() const { return bytes_; }
    void clear() {
        bytes_.clear();
        bits_ = 0;
    }

private:
    std::vector<uint8_t> bytes_;
    size_t bits_ = 0;
};

class AUTOCRAFTNET_API BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    uint64_t readBits(int bits);
    bool readBool() { return readBits(1) != 0; }
    uint8_t readU8() { return static_cast<uint8_t>(readBits(8)); }
    uint16_t readU16() { return static_cast<uint16_t>(readBits(16)); }
    uint32_t readU32() { return static_cast<uint32_t>(readBits(32)); }
    uint64_t readU64() { return readBits(64); }
    /// False, and nothing read, when fewer than `size` bytes are left.
    bool readBytes(uint8_t* out, size_t size);
    int64_t readInt(int64_t min, int64_t max);
    float readFloat(float min, float max, int bits) { return dequantizeFloat(static_cast<uint32_t>(readBits(bits)), min, max, bits); }
    float readAngle() { return dequantizeAngle(static_cast<uint16_t>(readBits(16))); }
    void align();

    /// No read has run past the end.
    bool ok() const { return ok_; }
    size_t bitsLeft() const { return size_ * 8 - bit_; }
    /// Where the next whole byte starts (after align()).
    size_t bytePosition() const { return (bit_ + 7) / 8; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t bit_ = 0;
    bool ok_ = true;
};

} // namespace ac::net
