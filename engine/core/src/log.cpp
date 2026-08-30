#include <vector>

#include <engine/core/log.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace engine::core {

namespace {

spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Critical:
        return spdlog::level::critical;
    case LogLevel::Off:
        return spdlog::level::off;
    }
    return spdlog::level::info;
}

}  // namespace

std::shared_ptr<spdlog::logger> Log::sLogger = nullptr;

void Log::init(LogLevel level) {
    if (sLogger) {
        return;
    }

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] [%n] [%l]: %v%$");

    sLogger = std::make_shared<spdlog::logger>("ENGINE", consoleSink);
    sLogger->set_level(toSpdlogLevel(level));
    sLogger->flush_on(spdlog::level::trace);

    spdlog::register_logger(sLogger);
}

void Log::shutdown() {
    if (sLogger) {
        sLogger->flush();
        spdlog::drop("ENGINE");
        sLogger.reset();
    }
}

void Log::setLevel(LogLevel level) {
    if (sLogger) {
        sLogger->set_level(toSpdlogLevel(level));
    }
}

std::shared_ptr<spdlog::logger>& Log::getLogger() {
    if (!sLogger) {
        init();
    }
    return sLogger;
}

}  // namespace engine::core
