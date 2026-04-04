#include "p2p/core/varint.hpp"

namespace p2p {
namespace varint {

std::vector<uint8_t> Encode(uint64_t value) {
    std::vector<uint8_t> result;

    // Encode using unsigned LEB128
    do {
        uint8_t byte = value & 0x7F;  // Take lower 7 bits
        value >>= 7;                   // Shift right by 7 bits

        if (value != 0) {
            byte |= 0x80;  // Set continuation bit if more bytes follow
        }

        result.push_back(byte);
    } while (value != 0);

    return result;
}

std::pair<uint64_t, size_t> Decode(const uint8_t* data, size_t len) {
    if (data == nullptr || len == 0) {
        return {0, 0};
    }

    uint64_t result = 0;
    size_t shift = 0;
    size_t bytes_read = 0;

    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        bytes_read++;

        // Check for overflow (max 10 bytes for uint64_t)
        if (shift >= 64) {
            return {0, 0};
        }

        // Add lower 7 bits to result
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;

        // If continuation bit is not set, we're done
        if ((byte & 0x80) == 0) {
            return {result, bytes_read};
        }
    }

    // Incomplete varint (continuation bit set but no more bytes)
    return {0, 0};
}

size_t EncodedSize(uint64_t value) {
    if (value == 0) {
        return 1;
    }

    size_t size = 0;
    while (value != 0) {
        value >>= 7;
        size++;
    }

    return size;
}

}  // namespace varint
}  // namespace p2p
