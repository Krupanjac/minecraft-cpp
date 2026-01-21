#pragma once

#include "DedicatedServer.h"
#include <string>
#include <vector>

// Forward declarations
struct GLFWwindow;

namespace Server {

// Server GUI States
enum class ServerGUIState {
    SETUP,      // Initial setup screen
    RUNNING,    // Server running, showing console
    STOPPED     // Server stopped
};

class ServerGUI {
public:
    ServerGUI();
    ~ServerGUI();
    
    // Initialize window and ImGui
    bool init();
    
    // Main loop - returns false when should exit
    bool update();
    
    // Cleanup
    void shutdown();
    
private:
    void renderSetupScreen();
    void renderConsoleScreen();
    void renderStatsPanel();
    void renderPlayersPanel();
    void renderLogPanel();
    void renderCommandInput();
    
    void startServer();
    void stopServer();
    void processCommand(const std::string& command);
    
    GLFWwindow* m_window = nullptr;
    ServerGUIState m_state = ServerGUIState::SETUP;
    
    // Server
    std::unique_ptr<DedicatedServer> m_server;
    ServerConfig m_setupConfig;
    
    // UI State
    char m_commandBuffer[256] = {0};
    bool m_scrollToBottom = false;
    bool m_autoScroll = true;
    std::vector<std::string> m_commandHistory;
    int m_historyIndex = -1;
    
    // Timing
    float m_lastFrameTime = 0.0f;
};

} // namespace Server
