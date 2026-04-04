#include "p2p/transport/message_framer.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

using namespace p2p::transport;

// Helper: create a frame with given total_length and json_length
std::vector<uint8_t> make_frame(uint32_t total_length, uint32_t json_length) {
    std::vector<uint8_t> frame;
    frame.reserve(8 + total_length);

    // total_length (big-endian)
    frame.push_back((total_length >> 24) & 0xFF);
    frame.push_back((total_length >> 16) & 0xFF);
    frame.push_back((total_length >> 8) & 0xFF);
    frame.push_back(total_length & 0xFF);

    // json_length (big-endian)
    frame.push_back((json_length >> 24) & 0xFF);
    frame.push_back((json_length >> 16) & 0xFF);
    frame.push_back((json_length >> 8) & 0xFF);
    frame.push_back(json_length & 0xFF);

    // json + payload (fill with dummy data)
    for (uint32_t i = 0; i < total_length; ++i) {
        frame.push_back(static_cast<uint8_t>('A' + (i % 26)));
    }

    return frame;
}

TEST(MessageFramerTest, SingleCompleteMessage) {
    MessageFramer framer;

    auto frame = make_frame(100, 50);  // 100 bytes total, 50 json, 50 payload
    auto messages = framer.feed(frame.data(), frame.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].size(), 108);  // 8 header + 100 data
    EXPECT_EQ(messages[0], frame);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, TwoMessagesInOneRead_StickyPacket) {
    MessageFramer framer;

    auto frame1 = make_frame(50, 25);
    auto frame2 = make_frame(80, 40);

    // Concatenate two frames (TCP sticky packet scenario)
    std::vector<uint8_t> combined;
    combined.insert(combined.end(), frame1.begin(), frame1.end());
    combined.insert(combined.end(), frame2.begin(), frame2.end());

    auto messages = framer.feed(combined.data(), combined.size());

    ASSERT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0], frame1);
    EXPECT_EQ(messages[1], frame2);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, MessageSplitAcrossReads_SplitPacket) {
    MessageFramer framer;

    auto frame = make_frame(200, 100);

    // Split frame into 3 reads
    size_t split1 = 50;
    size_t split2 = 100;

    // First read: partial header + some data
    auto msgs1 = framer.feed(frame.data(), split1);
    EXPECT_EQ(msgs1.size(), 0);  // Not enough for complete message
    EXPECT_GT(framer.buffered(), 0);

    // Second read: more data but still incomplete
    auto msgs2 = framer.feed(frame.data() + split1, split2 - split1);
    EXPECT_EQ(msgs2.size(), 0);
    EXPECT_GT(framer.buffered(), 0);

    // Third read: rest of the frame
    auto msgs3 = framer.feed(frame.data() + split2, frame.size() - split2);
    ASSERT_EQ(msgs3.size(), 1);
    EXPECT_EQ(msgs3[0], frame);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, HeaderSplitAcrossReads) {
    MessageFramer framer;

    auto frame = make_frame(100, 50);

    // Read only 5 bytes (incomplete header)
    auto msgs1 = framer.feed(frame.data(), 5);
    EXPECT_EQ(msgs1.size(), 0);
    EXPECT_EQ(framer.buffered(), 5);

    // Read rest
    auto msgs2 = framer.feed(frame.data() + 5, frame.size() - 5);
    ASSERT_EQ(msgs2.size(), 1);
    EXPECT_EQ(msgs2[0], frame);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, MultipleMessagesWithSplits) {
    MessageFramer framer;

    auto frame1 = make_frame(50, 25);
    auto frame2 = make_frame(60, 30);
    auto frame3 = make_frame(40, 20);

    std::vector<uint8_t> all_frames;
    all_frames.insert(all_frames.end(), frame1.begin(), frame1.end());
    all_frames.insert(all_frames.end(), frame2.begin(), frame2.end());
    all_frames.insert(all_frames.end(), frame3.begin(), frame3.end());

    // Feed in chunks that don't align with message boundaries
    size_t chunk_size = 70;
    size_t offset = 0;
    std::vector<std::vector<uint8_t>> all_messages;

    while (offset < all_frames.size()) {
        size_t len = std::min(chunk_size, all_frames.size() - offset);
        auto msgs = framer.feed(all_frames.data() + offset, len);
        all_messages.insert(all_messages.end(), msgs.begin(), msgs.end());
        offset += len;
    }

    ASSERT_EQ(all_messages.size(), 3);
    EXPECT_EQ(all_messages[0], frame1);
    EXPECT_EQ(all_messages[1], frame2);
    EXPECT_EQ(all_messages[2], frame3);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, OversizedFrameRejected) {
    MessageFramer framer;

    // Create a frame header claiming 100KB (exceeds MAX_FRAME_SIZE of 64KB)
    uint32_t huge_length = 100 * 1024;
    std::vector<uint8_t> bad_header = {
        static_cast<uint8_t>((huge_length >> 24) & 0xFF),
        static_cast<uint8_t>((huge_length >> 16) & 0xFF),
        static_cast<uint8_t>((huge_length >> 8) & 0xFF),
        static_cast<uint8_t>(huge_length & 0xFF),
        0, 0, 0, 50  // json_length = 50
    };

    auto msgs = framer.feed(bad_header.data(), bad_header.size());

    // Framer should discard buffer and return no messages
    EXPECT_EQ(msgs.size(), 0);
    EXPECT_EQ(framer.buffered(), 0);  // Buffer cleared
}

TEST(MessageFramerTest, ZeroLengthMessage) {
    MessageFramer framer;

    auto frame = make_frame(0, 0);  // Empty message (just header)
    auto messages = framer.feed(frame.data(), frame.size());

    ASSERT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].size(), 8);  // Just the header
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, ClearMethod) {
    MessageFramer framer;

    auto frame = make_frame(100, 50);

    // Feed partial data
    framer.feed(frame.data(), 20);
    EXPECT_GT(framer.buffered(), 0);

    // Clear should reset buffer
    framer.clear();
    EXPECT_EQ(framer.buffered(), 0);

    // Should be able to process new messages after clear
    auto msgs = framer.feed(frame.data(), frame.size());
    ASSERT_EQ(msgs.size(), 1);
    EXPECT_EQ(msgs[0], frame);
}

TEST(MessageFramerTest, EmptyFeed) {
    MessageFramer framer;

    auto msgs = framer.feed(nullptr, 0);
    EXPECT_EQ(msgs.size(), 0);
    EXPECT_EQ(framer.buffered(), 0);
}

TEST(MessageFramerTest, MaxSizeFrameAccepted) {
    MessageFramer framer;

    // Create a frame at exactly MAX_FRAME_SIZE (64KB)
    uint32_t max_data = 64 * 1024 - 8;  // 64KB - header
    auto frame = make_frame(max_data, max_data / 2);

    auto msgs = framer.feed(frame.data(), frame.size());

    ASSERT_EQ(msgs.size(), 1);
    EXPECT_EQ(msgs[0].size(), 64 * 1024);
    EXPECT_EQ(framer.buffered(), 0);
}
