#pragma once

#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace va {

    enum class LogLevel {
        Trace,
        Info,
        Debug,
        Warning,
        Error
    };

    class Log {
    public:
        // Delete copy constructor and assignment operator
        Log(const Log&) = delete;
        Log& operator=(const Log&) = delete;

        static Log& Get() {
            static Log instance;
            return instance;
        }

        void Trace(const std::string& message);
        void Info(const std::string& message);
        void Debug(const std::string& message);
        void Warning(const std::string& message);
        void Error(const std::string& message);

        void SetLevel(LogLevel level);

    private:
        Log() = default;
        ~Log() = default;

        void log(LogLevel level, const std::string& message);

        std::mutex _mutex;
        LogLevel _logLevel = LogLevel::Trace;
    };

} // namespace va
