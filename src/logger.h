#pragma once

#include <string_view>

namespace Logger
{
    void initialize();

    void info(
        std::string_view source,
        std::string_view message
    );

    void warn(
        std::string_view source,
        std::string_view message
    );

    void error(
        std::string_view source,
        std::string_view message
    );
}

#define LOG_INFO(message) \
    Logger::info(__FILE__, message)

#define LOG_WARN(message) \
    Logger::info(__FILE__, message)

#define LOG_ERROR(message) \
    Logger::error(__FILE__, message)