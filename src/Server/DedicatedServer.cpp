#include "DedicatedServer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace Server {

DedicatedServer::DedicatedServer() = default;

DedicatedServer::~DedicatedServer() {
    stop();
}

bool DedicatedServer::start() {
    if (m_running) {
        log("WARN", "Server is already running");
        return false;
    }
    
    log("INFO", "Starting dedicated server...");
    log("INFO", "Server name: " + m_config.serverName);
    log("INFO", "Port: " + std::to_string(m_config.port));
    log("INFO", "Seed: " + std::to_string(m_config.seed));
    log("INFO", "Max players: " + std::to_string(m_config.maxPlayers));
    
    // Initialize network
    m_server = std::make_unique<Network::GameServer>();
    
    glm::vec3 spawnPos(m_config.spawnX, m_config.spawnY, m_config.spawnZ);
    
    if (!m_server->start(m_config.port, m_config.seed, spawnPos, "Server")) {
        log("ERROR", "Failed to start server on port " + std::to_string(m_config.port));
        m_server.reset();
        return false;
    }
    
    // Setup callbacks
    m_server->setPlayerJoinCallback([this](uint32_t playerId, const std::string& name) {
        try {
            log("DEBUG", "Player join callback started for: " + name);
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_stats.totalConnections++;
            }
            log("INFO", "Player joined: " + name + " (ID: " + std::to_string(playerId) + ")");
            log("DEBUG", "About to broadcast join message...");
            broadcastMessage(name + " joined the game");
            log("DEBUG", "Player join callback completed");
        } catch (const std::exception& e) {
            log("ERROR", "Exception in player join callback: " + std::string(e.what()));
        } catch (...) {
            log("ERROR", "Unknown exception in player join callback");
        }
    });
    
    m_server->setPlayerLeaveCallback([this](uint32_t playerId) {
        log("INFO", "Player left: ID " + std::to_string(playerId));
    });
    
    m_server->setBlockChangeCallback([this](int x, int y, int z, uint8_t blockType, uint32_t playerId) {
        // Silence unused parameter warnings
        (void)x; (void)y; (void)z; (void)blockType; (void)playerId;
        // Could log block changes if verbose logging is enabled
    });
    
    m_server->setChatCallback([this](uint32_t senderId, const std::string& message) {
        std::string playerName = "Player " + std::to_string(senderId);
        auto players = m_server->getPlayers();
        for (const auto& p : players) {
            if (p.id == senderId) {
                playerName = p.name;
                break;
            }
        }
        log("CHAT", "<" + playerName + "> " + message);
    });
    
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    m_server->setTimeOfDay(m_config.timeOfDay);
    m_server->setTimePaused(m_config.timePaused);
    
    log("INFO", "Server started successfully!");
    log("INFO", "Listening on port " + std::to_string(m_config.port));
    
    return true;
}

void DedicatedServer::stop() {
    if (!m_running) return;
    
    log("INFO", "Stopping server...");
    
    if (m_server) {
        broadcastMessage("Server is shutting down...");
        m_server->stop();
        m_server.reset();
    }
    
    m_running = false;
    log("INFO", "Server stopped");
}

void DedicatedServer::update(float deltaTime) {
    if (!m_running || !m_server) return;
    
    // Update network
    m_server->update();
    
    // Update time if not paused
    if (!m_config.timePaused) {
        m_config.timeOfDay += deltaTime * 10.0f;
        if (m_config.timeOfDay >= 2400.0f) {
            m_config.timeOfDay -= 2400.0f;
        }
    }
    
    // Sync time to clients periodically
    m_timeSyncTimer += deltaTime;
    if (m_timeSyncTimer >= 2.0f) {
        syncTime();
        m_timeSyncTimer = 0.0f;
    }
    
    // Update stats
    updateStats(deltaTime);
}

void DedicatedServer::updateStats(float deltaTime) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // Update uptime
    auto now = std::chrono::steady_clock::now();
    m_stats.uptimeSeconds = std::chrono::duration<double>(now - m_startTime).count();
    
    // Update player count
    if (m_server) {
        m_stats.playersOnline = m_server->getPlayerCount();
    }
    
    // Calculate TPS
    m_tickCount++;
    m_tpsTimer += deltaTime;
    if (m_tpsTimer >= 1.0f) {
        m_stats.ticksPerSecond = static_cast<float>(m_tickCount) / m_tpsTimer;
        m_tickCount = 0;
        m_tpsTimer = 0.0f;
    }
}

ServerStats DedicatedServer::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;  // Return a copy
}

void DedicatedServer::syncTime() {
    if (m_server) {
        m_server->broadcastTimeSync(m_config.timeOfDay, m_config.timePaused);
    }
}

void DedicatedServer::executeCommand(const std::string& command) {
    if (command.empty()) return;
    
    // Parse command
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;
    
    // Convert to lowercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (cmd == "help") {
        log("INFO", "Available commands:");
        log("INFO", "  help - Show this help");
        log("INFO", "  stop / quit - Stop the server");
        log("INFO", "  list - List online players");
        log("INFO", "  say <message> - Broadcast a message");
        log("INFO", "  kick <playerId> [reason] - Kick a player");
        log("INFO", "  time set <value> - Set time of day (0-2400)");
        log("INFO", "  time pause/resume - Pause/resume day-night cycle");
        log("INFO", "  status - Show server status");
    }
    else if (cmd == "stop" || cmd == "quit") {
        log("INFO", "Stopping server...");
        stop();
    }
    else if (cmd == "list") {
        auto players = getPlayers();
        log("INFO", "Online players (" + std::to_string(players.size()) + "/" + 
            std::to_string(m_config.maxPlayers) + "):");
        for (const auto& p : players) {
            log("INFO", "  - " + p.name + " (ID: " + std::to_string(p.id) + ")");
        }
    }
    else if (cmd == "say") {
        std::string message;
        std::getline(iss >> std::ws, message);
        if (!message.empty()) {
            broadcastMessage("[Server] " + message);
            log("INFO", "Broadcast: " + message);
        }
    }
    else if (cmd == "kick") {
        uint32_t playerId;
        if (iss >> playerId) {
            std::string reason;
            std::getline(iss >> std::ws, reason);
            kickPlayer(playerId, reason.empty() ? "Kicked by server" : reason);
        } else {
            log("ERROR", "Usage: kick <playerId> [reason]");
        }
    }
    else if (cmd == "time") {
        std::string subcmd;
        iss >> subcmd;
        std::transform(subcmd.begin(), subcmd.end(), subcmd.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        
        if (subcmd == "set") {
            float value;
            if (iss >> value) {
                setTime(value);
                log("INFO", "Time set to " + std::to_string(value));
            } else {
                log("ERROR", "Usage: time set <value>");
            }
        }
        else if (subcmd == "pause") {
            setTimePaused(true);
            log("INFO", "Day-night cycle paused");
        }
        else if (subcmd == "resume") {
            setTimePaused(false);
            log("INFO", "Day-night cycle resumed");
        }
        else {
            log("ERROR", "Usage: time set|pause|resume");
        }
    }
    else if (cmd == "status") {
        log("INFO", "=== Server Status ===");
        log("INFO", "Server: " + m_config.serverName);
        log("INFO", "Players: " + std::to_string(m_stats.playersOnline) + "/" + 
            std::to_string(m_config.maxPlayers));
        
        // Format uptime
        int hours = static_cast<int>(m_stats.uptimeSeconds / 3600);
        int minutes = static_cast<int>((static_cast<int>(m_stats.uptimeSeconds) % 3600) / 60);
        int seconds = static_cast<int>(m_stats.uptimeSeconds) % 60;
        std::ostringstream uptimeStr;
        uptimeStr << hours << "h " << minutes << "m " << seconds << "s";
        log("INFO", "Uptime: " + uptimeStr.str());
        
        log("INFO", "TPS: " + std::to_string(static_cast<int>(m_stats.ticksPerSecond)));
        log("INFO", "Time: " + std::to_string(static_cast<int>(m_config.timeOfDay)) + 
            (m_config.timePaused ? " (paused)" : ""));
    }
    else {
        log("ERROR", "Unknown command: " + cmd + ". Type 'help' for commands.");
    }
}

void DedicatedServer::broadcastMessage(const std::string& message) {
    if (m_server) {
        m_server->broadcastChatMessage(message);
    }
}

void DedicatedServer::kickPlayer(uint32_t playerId, const std::string& reason) {
    // TODO: Implement proper kick functionality in GameServer
    log("INFO", "Kicked player " + std::to_string(playerId) + ": " + reason);
}

void DedicatedServer::setTime(float timeOfDay) {
    m_config.timeOfDay = timeOfDay;
    if (m_config.timeOfDay >= 2400.0f) m_config.timeOfDay -= 2400.0f;
    if (m_config.timeOfDay < 0.0f) m_config.timeOfDay += 2400.0f;
    syncTime();
}

void DedicatedServer::setTimePaused(bool paused) {
    m_config.timePaused = paused;
    syncTime();
}

std::vector<Network::RemotePlayer> DedicatedServer::getPlayers() const {
    if (m_server) {
        return m_server->getPlayers();
    }
    return {};
}

std::vector<LogEntry> DedicatedServer::getLogs() const {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return m_logs;  // Return a copy for thread safety
}

void DedicatedServer::log(const std::string& level, const std::string& message) {
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.message = message;
    
    {
        std::lock_guard<std::mutex> lock(m_logMutex);
        m_logs.push_back(entry);
        if (m_logs.size() > MAX_LOG_ENTRIES) {
            m_logs.erase(m_logs.begin());
        }
    }
    
    // Also log to system logger
    if (level == "ERROR") {
        LOG_ERROR("[Server] " + message);
    } else if (level == "WARN") {
        LOG_WARNING("[Server] " + message);
    } else {
        LOG_INFO("[Server] " + message);
    }
    
    // Call callback
    if (m_onLog) {
        m_onLog(level, message);
    }
}

} // namespace Server
