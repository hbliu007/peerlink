#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace p2p {
namespace utils {

class Logger {
public:
    static void Init(
        const std::string& log_file = "peerlink.log",
        spdlog::level::level_enum level = spdlog::level::info,
        size_t max_file_size = 10 * 1024 * 1024,  // 10MB
        size_t max_files = 3
    );

    static std::shared_ptr<spdlog::logger> Get();

private:
    static std::shared_ptr<spdlog::logger> logger_;
};

// Convenience macros
#define LOG_TRACE(...)    ::p2p::utils::Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    ::p2p::utils::Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...)     ::p2p::utils::Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...)     ::p2p::utils::Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    ::p2p::utils::Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::p2p::utils::Logger::Get()->critical(__VA_ARGS__)

} // namespace utils
} // namespace p2p
