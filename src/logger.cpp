#include "Logger.h"

#include <windows.h>

#include <iostream>
#include <string>

namespace
{
    std::string getFileName(std::string_view path)
    {
        const std::size_t position = path.find_last_of("\\/");

        if (position == std::string_view::npos)
        {
            return std::string(path);
        }

        return std::string(path.substr(position + 1));
    }

    void writeLog(
        std::string_view level,
        std::string_view source,
        std::string_view message
    )
    {
        std::cout
            << "["
            << level
            << "] ["
            << getFileName(source)
            << "] "
            << message
            << '\n'
            << std::flush;
    }
}

namespace Logger
{
    void initialize()
    {
        // WindowsコンソールをUTF-8に設定
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    void info(
        std::string_view source,
        std::string_view message
    )
    {
        writeLog("INFO", source, message);
    }

        void warn(
        std::string_view source,
        std::string_view message
    )
    {
        writeLog("WARN", source, message);
    }

    void error(
        std::string_view source,
        std::string_view message
    )
    {
        writeLog("ERROR", source, message);
    }
}