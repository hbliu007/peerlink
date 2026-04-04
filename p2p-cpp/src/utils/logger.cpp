#include "p2p/utils/logger.hpp"
#include <spdlog/async.h>
#include <iostream>

namespace p2p {
namespace utils {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::Init(
    const std::string& log_file,
    spdlog::level::level_enum level,
    size_t max_file_size,
    size_t max_files
) {
    try {
        // Create sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(level);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, max_file_size, max_files
        );
        file_sink->set_level(level);
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] %v");

        // Create logger with both sinks
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        logger_ = std::make_shared<spdlog::logger>("peerlink", sinks.begin(), sinks.end());
        logger_->set_level(level);
        logger_->flush_on(spdlog::level::warn);

        // Register as default logger
        spdlog::set_default_logger(logger_);

        LOG_INFO("Logger initialized: file={}, level={}", log_file, spdlog::level::to_string_view(level));
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Logger initialization failed: " << ex.what() << std::endl;
        throw;
    }
}

std::shared_ptr<spdlog::logger> Logger::Get() {
    if (!logger_) {
        // Initialize with default settings if not already initialized
        Init();
    }
    return logger_;
}

} // namespace utils
} // namespace p2p
