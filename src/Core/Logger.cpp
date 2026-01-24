#include "Logger.h"

// Only include Console in non-server builds
#ifndef SERVER_BUILD
#include "../UI/Console.h"
#endif

void Logger::log(LogLevel level, const std::string& message) {
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
    
#ifndef SERVER_BUILD
    // Forward to in-game console if enabled (game client only)
    if (m_consoleLoggingEnabled) {
        glm::vec4 color;
        switch (level) {
            case LogLevel::DEBUG:
                color = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f); // Gray
                break;
            case LogLevel::INFO:
                color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // White
                break;
            case LogLevel::WARNING:
                color = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow
                break;
            case LogLevel::ERROR:
                color = glm::vec4(1.0f, 0.3f, 0.3f, 1.0f); // Red
                break;
            default:
                color = glm::vec4(1.0f);
                break;
        }
        Console::instance().addMessage(ss.str(), color);
    }
#endif
}
