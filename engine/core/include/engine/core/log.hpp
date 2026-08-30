#pragma once

#include <engine/core/types.hpp>

#include <spdlog/spdlog.h>

namespace engine::core {

enum class LogLevel : u8 { Trace, Debug, Info, Warn, Error, Critical, Off };

class Log {
public:
    static void init(LogLevel level = LogLevel::Info);
    static void shutdown();
    static void setLevel(LogLevel level);
    static spdlog::logger* getLogger();
};

}  // namespace engine::core

#define ENGINE_LOG_TRACE(...) ::engine::core::Log::getLogger()->trace(__VA_ARGS__)
#define ENGINE_LOG_DEBUG(...) ::engine::core::Log::getLogger()->debug(__VA_ARGS__)
#define ENGINE_LOG_INFO(...) ::engine::core::Log::getLogger()->info(__VA_ARGS__)
#define ENGINE_LOG_WARN(...) ::engine::core::Log::getLogger()->warn(__VA_ARGS__)
#define ENGINE_LOG_ERROR(...) ::engine::core::Log::getLogger()->error(__VA_ARGS__)
#define ENGINE_LOG_CRITICAL(...) ::engine::core::Log::getLogger()->critical(__VA_ARGS__)
