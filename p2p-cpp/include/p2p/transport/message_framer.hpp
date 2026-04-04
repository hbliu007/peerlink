#pragma once

#include <cstdint>
#include <vector>
#include <iostream>

namespace p2p {
namespace transport {

/**
 * @brief Reassembles complete P2P messages from a TCP byte stream.
 *
 * TCP is a stream protocol with no message boundaries. A single read may
 * deliver half a message, exactly one message, or several messages glued
 * together ("sticky packets" / "split packets").
 *
 * This class accumulates raw bytes from successive reads and extracts
 * complete frames based on the protocol's length prefix:
 *
 *   [total_length (4B big-endian)]  — json_bytes + payload bytes
 *   [json_length  (4B big-endian)]
 *   [json bytes ...]
 *   [payload bytes ...]
 *
 * Frame size on the wire = 8 + total_length.
 */
class MessageFramer {
public:
    static constexpr size_t HEADER_SIZE = 8;           // total_length + json_length
    static constexpr size_t MAX_FRAME_SIZE = 64 * 1024; // 64 KiB safety cap

    /**
     * @brief Feed raw bytes from a TCP read into the framer.
     * @param data  Pointer to received bytes
     * @param len   Number of bytes received
     * @return Zero or more complete message frames ready for decode
     */
    std::vector<std::vector<uint8_t>> feed(const uint8_t* data, size_t len) {
        buffer_.insert(buffer_.end(), data, data + len);

        std::vector<std::vector<uint8_t>> messages;

        while (buffer_.size() >= HEADER_SIZE) {
            // Read total_length (big-endian uint32)
            uint32_t total_length =
                (static_cast<uint32_t>(buffer_[0]) << 24) |
                (static_cast<uint32_t>(buffer_[1]) << 16) |
                (static_cast<uint32_t>(buffer_[2]) << 8)  |
                 static_cast<uint32_t>(buffer_[3]);

            size_t frame_size = HEADER_SIZE + total_length;

            // Sanity check: reject absurdly large frames
            if (frame_size > MAX_FRAME_SIZE) {
                std::cerr << "[MessageFramer] Frame too large ("
                          << frame_size << " bytes), discarding buffer" << std::endl;
                buffer_.clear();
                break;
            }

            // Not enough bytes for a complete frame yet — wait for more data
            if (buffer_.size() < frame_size) {
                break;
            }

            // Extract one complete frame
            messages.emplace_back(buffer_.begin(), buffer_.begin() + frame_size);
            buffer_.erase(buffer_.begin(), buffer_.begin() + frame_size);
        }

        return messages;
    }

    /** @brief Discard all buffered data (e.g. on disconnect). */
    void clear() { buffer_.clear(); }

    /** @brief Number of bytes currently buffered (incomplete frame). */
    size_t buffered() const { return buffer_.size(); }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace transport
} // namespace p2p
