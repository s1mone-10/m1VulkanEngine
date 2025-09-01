#include "log/Log.hpp"

namespace m1 {

    void Log::Trace(const std::string& message) {
        log(LogLevel::Trace, message);
    }

    void Log::Info(const std::string& message) {
        log(LogLevel::Info, message);
    }

    void Log::Debug(const std::string& message) {
        log(LogLevel::Debug, message);
    }

    void Log::Warning(const std::string& message) {
        log(LogLevel::Warning, message);
    }

    void Log::Error(const std::string& message) {
        log(LogLevel::Error, message);
    }

    void Log::SetLevel(LogLevel level) {
        _logLevel = level;
    }

    void Log::log(LogLevel level, const std::string& message) {
        if (level < _logLevel) {
            return;
        }

        std::lock_guard<std::mutex> lock(_mutex);

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);

        std::stringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " ";

        switch (level) {
            case LogLevel::Trace:
                ss << "[TRACE]   ";
                break;
            case LogLevel::Info:
                ss << "[INFO]    ";
                break;
            case LogLevel::Debug:
                ss << "[DEBUG]   ";
                break;
            case LogLevel::Warning:
                ss << "[WARNING] ";
                break;
            case LogLevel::Error:
                ss << "[ERROR]   ";
                break;
        }

        ss << message;

        if (level == LogLevel::Error || level == LogLevel::Warning) {
            std::cerr << ss.str() << std::endl;
        } else {
            std::cout << ss.str() << std::endl;
        }
    }

} // namespace va
