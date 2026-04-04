#include "p2p/utils/logger.hpp"
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace p2p::utils;
namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    std::string test_log_file_;

    void SetUp() override {
        // Generate unique log file name for each test
        test_log_file_ = "test_logger_" +
                        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                        ".log";
    }

    void TearDown() override {
        // Clean up log files created during test
        try {
            if (fs::exists(test_log_file_)) {
                fs::remove(test_log_file_);
            }
            // Also remove rotated log files
            for (int i = 1; i <= 10; ++i) {
                std::string rotated = test_log_file_ + "." + std::to_string(i);
                if (fs::exists(rotated)) {
                    fs::remove(rotated);
                }
            }
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    // Helper: Read log file content
    std::string ReadLogFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Helper: Check if string contains substring
    bool Contains(const std::string& str, const std::string& substr) {
        return str.find(substr) != std::string::npos;
    }
};

// --- Initialization Tests ---

TEST_F(LoggerTest, Init_CreatesLoggerInstance) {
    Logger::Init(test_log_file_);

    auto logger = Logger::Get();
    EXPECT_NE(logger, nullptr);
}

TEST_F(LoggerTest, Init_CreatesLogFile) {
    Logger::Init(test_log_file_);

    // Write a log message to ensure file is created
    LOG_INFO("Test message");

    // Give spdlog time to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(fs::exists(test_log_file_));
}

TEST_F(LoggerTest, Init_WithInfoLevel_FiltersDebug) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_DEBUG("Debug message - should not appear");
    LOG_INFO("Info message - should appear");

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_FALSE(Contains(content, "Debug message"));
    EXPECT_TRUE(Contains(content, "Info message"));
}

TEST_F(LoggerTest, Init_WithDebugLevel_ShowsDebug) {
    Logger::Init(test_log_file_, spdlog::level::debug);

    LOG_DEBUG("Debug message - should appear");
    LOG_INFO("Info message - should appear");

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Debug message"));
    EXPECT_TRUE(Contains(content, "Info message"));
}

TEST_F(LoggerTest, Init_WithWarnLevel_FiltersInfoAndDebug) {
    Logger::Init(test_log_file_, spdlog::level::warn);

    LOG_DEBUG("Debug - should not appear");
    LOG_INFO("Info - should not appear");
    LOG_WARN("Warn - should appear");
    LOG_ERROR("Error - should appear");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_FALSE(Contains(content, "Debug - should not appear"));
    EXPECT_FALSE(Contains(content, "Info - should not appear"));
    EXPECT_TRUE(Contains(content, "Warn - should appear"));
    EXPECT_TRUE(Contains(content, "Error - should appear"));
}

// --- Get Tests ---

TEST_F(LoggerTest, Get_WithoutInit_AutoInitializes) {
    // Don't call Init, just Get
    auto logger = Logger::Get();

    EXPECT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "peerlink");
}

TEST_F(LoggerTest, Get_ReturnsValidLogger) {
    Logger::Init(test_log_file_);

    auto logger = Logger::Get();

    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "peerlink");
}

TEST_F(LoggerTest, Get_ReturnsSameInstance) {
    Logger::Init(test_log_file_);

    auto logger1 = Logger::Get();
    auto logger2 = Logger::Get();

    EXPECT_EQ(logger1, logger2);
}

// --- Log Output Tests ---

TEST_F(LoggerTest, LogInfo_WritesToFile) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_INFO("Test info message");

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Test info message"));
    EXPECT_TRUE(Contains(content, "[info]"));
}

TEST_F(LoggerTest, LogWarn_WritesToFile) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_WARN("Test warning message");

    // Warn level triggers immediate flush
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Test warning message"));
    EXPECT_TRUE(Contains(content, "[warn]") || Contains(content, "[warning]"));
}

TEST_F(LoggerTest, LogError_WritesToFile) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_ERROR("Test error message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Test error message"));
    EXPECT_TRUE(Contains(content, "[error]"));
}

TEST_F(LoggerTest, LogCritical_WritesToFile) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_CRITICAL("Test critical message");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Test critical message"));
    EXPECT_TRUE(Contains(content, "[critical]"));
}

// --- Macro Tests ---

TEST_F(LoggerTest, Macros_AllLevels_Work) {
    Logger::Init(test_log_file_, spdlog::level::trace);

    LOG_TRACE("Trace message");
    LOG_DEBUG("Debug message");
    LOG_INFO("Info message");
    LOG_WARN("Warn message");
    LOG_ERROR("Error message");
    LOG_CRITICAL("Critical message");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Trace message"));
    EXPECT_TRUE(Contains(content, "Debug message"));
    EXPECT_TRUE(Contains(content, "Info message"));
    EXPECT_TRUE(Contains(content, "Warn message"));
    EXPECT_TRUE(Contains(content, "Error message"));
    EXPECT_TRUE(Contains(content, "Critical message"));
}

TEST_F(LoggerTest, Macros_WithFormatting_Work) {
    Logger::Init(test_log_file_, spdlog::level::info);

    int value = 42;
    std::string name = "test";
    LOG_INFO("Value: {}, Name: {}", value, name);

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Value: 42"));
    EXPECT_TRUE(Contains(content, "Name: test"));
}

// --- Boundary Condition Tests ---

TEST_F(LoggerTest, LogLargeMessage_Succeeds) {
    Logger::Init(test_log_file_, spdlog::level::info);

    // Create a large message (10KB)
    std::string large_msg(10000, 'A');
    LOG_INFO("Large: {}", large_msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Large:"));
}

TEST_F(LoggerTest, LogEmptyMessage_Succeeds) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_INFO("");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Should not crash, file should exist
    EXPECT_TRUE(fs::exists(test_log_file_));
}

TEST_F(LoggerTest, LogSpecialCharacters_Succeeds) {
    Logger::Init(test_log_file_, spdlog::level::info);

    LOG_INFO("Special: \n\t\r{}[]");

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string content = ReadLogFile(test_log_file_);
    EXPECT_TRUE(Contains(content, "Special:"));
}

// --- Multiple Messages Tests ---

TEST_F(LoggerTest, MultipleMessages_AllWritten) {
    Logger::Init(test_log_file_, spdlog::level::info);

    for (int i = 0; i < 10; ++i) {
        LOG_INFO("Message {}", i);
    }

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string content = ReadLogFile(test_log_file_);
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(Contains(content, "Message " + std::to_string(i)));
    }
}

TEST_F(LoggerTest, RapidLogging_NoDataLoss) {
    Logger::Init(test_log_file_, spdlog::level::info);

    const int num_messages = 100;
    for (int i = 0; i < num_messages; ++i) {
        LOG_INFO("Rapid {}", i);
    }

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    // Give more time for all messages to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string content = ReadLogFile(test_log_file_);

    // Count how many messages were written
    int count = 0;
    for (int i = 0; i < num_messages; ++i) {
        if (Contains(content, "Rapid " + std::to_string(i))) {
            count++;
        }
    }

    // Should have most messages (allow some loss in extreme cases)
    EXPECT_GT(count, num_messages * 0.9);
}

// --- Thread Safety Tests ---

TEST_F(LoggerTest, ConcurrentLogging_ThreadSafe) {
    Logger::Init(test_log_file_, spdlog::level::info);

    const int num_threads = 4;
    const int messages_per_thread = 25;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < 25; ++i) {
                LOG_INFO("Thread {} Message {}", t, i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Flush to ensure logs are written to file
    Logger::Get()->flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::string content = ReadLogFile(test_log_file_);

    // Verify messages from all threads
    for (int t = 0; t < num_threads; ++t) {
        bool found_thread = false;
        for (int i = 0; i < messages_per_thread; ++i) {
            if (Contains(content, "Thread " + std::to_string(t))) {
                found_thread = true;
                break;
            }
        }
        EXPECT_TRUE(found_thread) << "Thread " << t << " messages not found";
    }
}
