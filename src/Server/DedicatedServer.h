#pragma once

#include "../Network/GameServer.h"
#include "../Core/Logger.h"
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

namespace Server {

// Server configuration
struct ServerConfig {
    uint16_t port = 25565;
    int64_t seed = 12345;
    std::string serverName = "Minecraft CPP Server";
    std::string motd = "Welcome to Minecraft CPP!";
    int maxPlayers = 20;
    float spawnX = 0.0f;
    float spawnY = 100.0f;
    float spawnZ = 0.0f;
    float timeOfDay = 600.0f;  // Start at noon
    bool timePaused = false;
};

// Server statistics
struct ServerStats {
    size_t playersOnline = 0;
    size_t totalConnections = 0;
    size_t packetsReceived = 0;
    size_t packetsSent = 0;
    float ticksPerSecond = 0.0f;
    double uptimeSeconds = 0.0;
    size_t memoryUsageMB = 0;
};

// Log entry for the server console
struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string level;
    std::string message;
};

class DedicatedServer {
public:
    DedicatedServer();
    ~DedicatedServer();
    
    // Configuration
    void setConfig(const ServerConfig& config) { m_config = config; }
    ServerConfig& getConfig() { return m_config; }
    const ServerConfig& getConfig() const { return m_config; }
    
    // Lifecycle
    bool start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Server tick (call from main loop)
    void update(float deltaTime);
    
    // Commands
    void executeCommand(const std::string& command);
    void broadcastMessage(const std::string& message);
    void kickPlayer(uint32_t playerId, const std::string& reason = "Kicked by server");
    void setTime(float timeOfDay);
    void setTimePaused(bool paused);
    
    // Getters
    ServerStats getStats() const;  // Returns a copy for thread safety
    std::vector<LogEntry> getLogs() const;  // Returns a copy for thread safety
    std::vector<Network::RemotePlayer> getPlayers() const;
    
    // Callbacks
    using LogCallback = std::function<void(const std::string& level, const std::string& message)>;
    void setLogCallback(LogCallback cb) { m_onLog = std::move(cb); }
    
private:
    void log(const std::string& level, const std::string& message);
    void updateStats(float deltaTime);
    void syncTime();
    
    ServerConfig m_config;
    ServerStats m_stats;
    std::vector<LogEntry> m_logs;
    static constexpr size_t MAX_LOG_ENTRIES = 1000;
    
    std::unique_ptr<Network::GameServer> m_server;
    std::atomic<bool> m_running{false};
    
    // Timing
    std::chrono::steady_clock::time_point m_startTime;
    float m_tickTimer = 0.0f;
    int m_tickCount = 0;
    float m_tpsTimer = 0.0f;
    float m_timeSyncTimer = 0.0f;
    
    LogCallback m_onLog;
    mutable std::mutex m_logMutex;
    mutable std::mutex m_statsMutex;
};

} // namespace Server
