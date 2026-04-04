#pragma once

#include <cstdint>
#include <vector>
#include <utility>

namespace p2p {
namespace varint {

/**
 * Encode unsigned integer to varint (unsigned LEB128)
 *
 * Varint encoding uses 7 bits per byte for data, with the MSB as continuation bit:
 * - MSB = 1: more bytes follow
 * - MSB = 0: last byte
 *
 * Examples:
 * - 0 → 0x00
 * - 127 → 0x7F
 * - 128 → 0x80 0x01
 * - 300 → 0xAC 0x02
 *
 * @param value Value to encode
 * @return Encoded bytes
 */
std::vector<uint8_t> Encode(uint64_t value);

/**
 * Decode varint from buffer
 *
 * @param data Buffer containing varint
 * @param len Buffer length
 * @return Pair of (decoded value, bytes consumed). Returns (0, 0) on error.
 */
std::pair<uint64_t, size_t> Decode(const uint8_t* data, size_t len);

/**
 * Calculate encoded size without actually encoding
 *
 * @param value Value to measure
 * @return Number of bytes required
 */
size_t EncodedSize(uint64_t value);

}  // namespace varint
}  // namespace p2p
