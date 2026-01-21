#pragma once

// Windows.h defines ERROR as a macro, so we need to handle it
#ifdef ERROR
#undef ERROR
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
    
    ~Logger() {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
    }

    void setLevel(LogLevel level) {
        minLevel = level;
    }
    
    void enableFileLogging(const std::string& filename = "minecraft.log") {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_fileStream.open(filename, std::ios::out | std::ios::trunc);
        m_fileLoggingEnabled = m_fileStream.is_open();
        if (m_fileLoggingEnabled) {
            m_fileStream << "=== Log started ===" << std::endl;
            m_fileStream.flush();
        }
    }
    
    void disableFileLogging() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_fileStream.is_open()) {
            m_fileStream << "=== Log ended ===" << std::endl;
            m_fileStream.close();
        }
        m_fileLoggingEnabled = false;
    }

    void log(LogLevel level, const std::string& message) {
        if (level < minLevel) return;

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        ss << "[" << levelToString(level) << "] ";
        ss << message;

        std::lock_guard<std::mutex> lock(m_mutex);
        std::cout << ss.str() << std::endl;
        
        // Write to file if enabled
        if (m_fileLoggingEnabled && m_fileStream.is_open()) {
            m_fileStream << ss.str() << std::endl;
            m_fileStream.flush();  // Flush immediately to catch crashes
        }
    }

    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warning(const std::string& message) { log(LogLevel::WARNING, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }

private:
    Logger() : minLevel(LogLevel::INFO), m_fileLoggingEnabled(false) {}
    LogLevel minLevel;
    std::mutex m_mutex;
    std::ofstream m_fileStream;
    bool m_fileLoggingEnabled;

    const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

// Convenience macros
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg) Logger::instance().info(msg)
#define LOG_WARNING(msg) Logger::instance().warning(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
