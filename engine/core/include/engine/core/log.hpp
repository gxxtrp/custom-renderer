#pragma once

#include <cstdlib>
#include <format>
#include <string_view>

#include <engine/core/types.hpp>

namespace engine::core {

enum class LogLevel : u8 { Trace, Debug, Info, Warn, Error, Critical, Fatal, Off };

class Log {
public:
    static void init(LogLevel level = LogLevel::Info);
    static void shutdown();
    static void setLevel(LogLevel level);

    static void log(LogLevel level, std::string_view message);

    template<typename... Args>
    static void trace(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Trace, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Debug, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    static void critical(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Critical, std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    [[noreturn]] static void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log(LogLevel::Fatal, std::format(fmt, std::forward<Args>(args)...));
        std::abort();
    }
};

}  // namespace engine::core

#define ENGINE_LOG_TRACE(...) ::engine::core::Log::trace(__VA_ARGS__)
#define ENGINE_LOG_DEBUG(...) ::engine::core::Log::debug(__VA_ARGS__)
#define ENGINE_LOG_INFO(...) ::engine::core::Log::info(__VA_ARGS__)
#define ENGINE_LOG_WARN(...) ::engine::core::Log::warn(__VA_ARGS__)
#define ENGINE_LOG_ERROR(...) ::engine::core::Log::error(__VA_ARGS__)
#define ENGINE_LOG_CRITICAL(...) ::engine::core::Log::critical(__VA_ARGS__)
#define ENGINE_LOG_FATAL(...) ::engine::core::Log::fatal(__VA_ARGS__)
