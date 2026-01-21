#include "ServerGUI.h"
#include "../Core/Logger.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace Server {

ServerGUI::ServerGUI() {
    m_server = std::make_unique<DedicatedServer>();
}

ServerGUI::~ServerGUI() {
    shutdown();
}

bool ServerGUI::init() {
    // Initialize GLFW
    if (!glfwInit()) {
        return false;
    }
    
    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // Create window
    m_window = glfwCreateWindow(1000, 700, "Minecraft CPP - Dedicated Server", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // Setup style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.4f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.5f, 0.4f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.4f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.5f, 0.4f, 1.0f);
    
    // Default config
    m_setupConfig.port = 25565;
    m_setupConfig.seed = static_cast<int64_t>(time(nullptr));
    m_setupConfig.serverName = "Minecraft CPP Server";
    m_setupConfig.maxPlayers = 20;
    m_setupConfig.spawnY = 100.0f;
    m_setupConfig.timeOfDay = 600.0f;
    
    return true;
}

bool ServerGUI::update() {
    if (glfwWindowShouldClose(m_window)) {
        return false;
    }
    
    try {
        glfwPollEvents();
        
        // Calculate delta time
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - m_lastFrameTime;
        m_lastFrameTime = currentTime;
        
        // Update server if running
        if (m_state == ServerGUIState::RUNNING && m_server && m_server->isRunning()) {
            m_server->update(deltaTime);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in ServerGUI::update (server): " + std::string(e.what()));
    } catch (...) {
        LOG_ERROR("Unknown exception in ServerGUI::update (server)");
    }
    
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Render appropriate screen
    switch (m_state) {
        case ServerGUIState::SETUP:
            renderSetupScreen();
            break;
        case ServerGUIState::RUNNING:
        case ServerGUIState::STOPPED:
            renderConsoleScreen();
            break;
    }
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
    
    return true;
}

void ServerGUI::shutdown() {
    if (m_server && m_server->isRunning()) {
        m_server->stop();
    }
    
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

void ServerGUI::renderSetupScreen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowSize(500, 450);
    ImVec2 windowPos((io.DisplaySize.x - windowSize.x) * 0.5f, 
                     (io.DisplaySize.y - windowSize.y) * 0.5f);
    
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
    
    ImGui::Begin("Server Setup", nullptr, 
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Minecraft CPP - Dedicated Server");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Server name
    static char serverName[64];
    strncpy(serverName, m_setupConfig.serverName.c_str(), sizeof(serverName) - 1);
    if (ImGui::InputText("Server Name", serverName, sizeof(serverName))) {
        m_setupConfig.serverName = serverName;
    }
    
    // Port
    int port = m_setupConfig.port;
    if (ImGui::InputInt("Port", &port)) {
        if (port > 0 && port < 65536) {
            m_setupConfig.port = static_cast<uint16_t>(port);
        }
    }
    
    // Max players
    int maxPlayers = m_setupConfig.maxPlayers;
    if (ImGui::SliderInt("Max Players", &maxPlayers, 1, 100)) {
        m_setupConfig.maxPlayers = maxPlayers;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("World Settings");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Seed
    static char seedStr[32];
    snprintf(seedStr, sizeof(seedStr), "%lld", static_cast<long long>(m_setupConfig.seed));
    if (ImGui::InputText("World Seed", seedStr, sizeof(seedStr))) {
        try {
            m_setupConfig.seed = std::stoll(seedStr);
        } catch (...) {}
    }
    ImGui::SameLine();
    if (ImGui::Button("Random")) {
        m_setupConfig.seed = static_cast<int64_t>(time(nullptr));
    }
    
    // Spawn position
    float spawn[3] = { m_setupConfig.spawnX, m_setupConfig.spawnY, m_setupConfig.spawnZ };
    if (ImGui::InputFloat3("Spawn Position", spawn)) {
        m_setupConfig.spawnX = spawn[0];
        m_setupConfig.spawnY = spawn[1];
        m_setupConfig.spawnZ = spawn[2];
    }
    
    // Time of day
    float timeOfDay = m_setupConfig.timeOfDay;
    if (ImGui::SliderFloat("Time of Day", &timeOfDay, 0.0f, 2400.0f, "%.0f")) {
        m_setupConfig.timeOfDay = timeOfDay;
    }
    
    // Time presets
    ImGui::SameLine();
    if (ImGui::Button("Dawn")) m_setupConfig.timeOfDay = 0.0f;
    ImGui::SameLine();
    if (ImGui::Button("Noon")) m_setupConfig.timeOfDay = 600.0f;
    ImGui::SameLine();
    if (ImGui::Button("Dusk")) m_setupConfig.timeOfDay = 1200.0f;
    ImGui::SameLine();
    if (ImGui::Button("Night")) m_setupConfig.timeOfDay = 1800.0f;
    
    ImGui::Checkbox("Pause Day/Night Cycle", &m_setupConfig.timePaused);
    
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Start button
    ImVec2 buttonSize(200, 40);
    ImGui::SetCursorPosX((windowSize.x - buttonSize.x) * 0.5f);
    
    if (ImGui::Button("Start Server", buttonSize)) {
        startServer();
    }
    
    ImGui::End();
}

void ServerGUI::renderConsoleScreen() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Full window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    
    ImGui::Begin("Server Console", nullptr, 
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_MenuBar);
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Server")) {
            if (m_state == ServerGUIState::RUNNING) {
                if (ImGui::MenuItem("Stop Server")) {
                    stopServer();
                }
            } else {
                if (ImGui::MenuItem("Start Server")) {
                    startServer();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                stopServer();
                glfwSetWindowShouldClose(m_window, true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Auto-scroll", nullptr, &m_autoScroll);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Split into panels
    float panelWidth = io.DisplaySize.x * 0.25f;
    
    // Left panel - Stats and Players
    ImGui::BeginChild("LeftPanel", ImVec2(panelWidth, 0), true);
    renderStatsPanel();
    ImGui::Separator();
    renderPlayersPanel();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    // Right panel - Log and Command
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    renderLogPanel();
    ImGui::Separator();
    renderCommandInput();
    ImGui::EndChild();
    
    ImGui::End();
}

void ServerGUI::renderStatsPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Server Status");
    ImGui::Separator();
    
    if (!m_server || !m_server->isRunning()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "OFFLINE");
        return;
    }
    
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "ONLINE");
    ImGui::Spacing();
    
    // Get thread-safe copies
    auto stats = m_server->getStats();
    const auto& config = m_server->getConfig();
    
    ImGui::Text("Server: %s", config.serverName.c_str());
    ImGui::Text("Port: %d", config.port);
    ImGui::Text("Seed: %lld", static_cast<long long>(config.seed));
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Players
    ImGui::Text("Players: %zu / %d", stats.playersOnline, config.maxPlayers);
    
    // Progress bar for players
    float playerRatio = static_cast<float>(stats.playersOnline) / config.maxPlayers;
    ImGui::ProgressBar(playerRatio, ImVec2(-1, 0));
    
    ImGui::Spacing();
    
    // TPS
    ImVec4 tpsColor = (stats.ticksPerSecond >= 18.0f) ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) :
                      (stats.ticksPerSecond >= 15.0f) ? ImVec4(1.0f, 1.0f, 0.3f, 1.0f) :
                                                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    ImGui::TextColored(tpsColor, "TPS: %.1f", stats.ticksPerSecond);
    
    // Uptime
    int hours = static_cast<int>(stats.uptimeSeconds / 3600);
    int minutes = static_cast<int>((static_cast<int>(stats.uptimeSeconds) % 3600) / 60);
    int seconds = static_cast<int>(stats.uptimeSeconds) % 60;
    ImGui::Text("Uptime: %02d:%02d:%02d", hours, minutes, seconds);
    
    // Time of day
    ImGui::Text("Time: %.0f %s", config.timeOfDay, config.timePaused ? "(paused)" : "");
    
    // Time slider for quick adjustment
    float timeOfDay = config.timeOfDay;
    if (ImGui::SliderFloat("##TimeSlider", &timeOfDay, 0.0f, 2400.0f, "")) {
        m_server->getConfig().timeOfDay = timeOfDay;
    }
}

void ServerGUI::renderPlayersPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Players Online");
    ImGui::Separator();
    
    if (!m_server || !m_server->isRunning()) {
        ImGui::TextDisabled("Server offline");
        return;
    }
    
    auto players = m_server->getPlayers();
    
    if (players.empty()) {
        ImGui::TextDisabled("No players connected");
        return;
    }
    
    for (const auto& player : players) {
        // Use player.id + 1 to ensure non-zero ID for ImGui (or use string-based ID)
        ImGui::PushID(static_cast<int>(player.id) + 1);
        
        // Player name with color based on state
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImGui::TextColored(color, "%s", player.name.c_str());
        
        // Tooltip with details
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("ID: %u", player.id);
            ImGui::Text("Position: %.1f, %.1f, %.1f", 
                       player.position.x, player.position.y, player.position.z);
            ImGui::EndTooltip();
        }
        
        // Context menu - use explicit string ID
        if (ImGui::BeginPopupContextItem(("player_ctx_" + std::to_string(player.id)).c_str())) {
            if (ImGui::MenuItem("Kick")) {
                m_server->executeCommand("kick " + std::to_string(player.id));
            }
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }
}

void ServerGUI::renderLogPanel() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Server Log");
    ImGui::Separator();
    
    // Log area
    float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("LogRegion", ImVec2(0, -footerHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (m_server) {
        const auto& logs = m_server->getLogs();
        
        for (const auto& entry : logs) {
            // Format timestamp
            auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm tm_buf;
#ifdef _WIN32
            localtime_s(&tm_buf, &time);
#else
            localtime_r(&time, &tm_buf);
#endif
            char timeStr[16];
            std::strftime(timeStr, sizeof(timeStr), "[%H:%M:%S]", &tm_buf);
            
            // Color based on level
            ImVec4 color;
            if (entry.level == "ERROR") {
                color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            } else if (entry.level == "WARN") {
                color = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
            } else if (entry.level == "CHAT") {
                color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
            } else {
                color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            }
            
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", timeStr);
            ImGui::SameLine();
            ImGui::TextColored(color, "[%s]", entry.level.c_str());
            ImGui::SameLine();
            ImGui::TextColored(color, "%s", entry.message.c_str());
        }
        
        if (m_scrollToBottom || (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }
        m_scrollToBottom = false;
    }
    
    ImGui::EndChild();
}

void ServerGUI::renderCommandInput() {
    ImGui::PushItemWidth(-1);
    
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
                                       ImGuiInputTextFlags_CallbackHistory;
    
    if (ImGui::InputText("##Command", m_commandBuffer, sizeof(m_commandBuffer), input_flags,
        [](ImGuiInputTextCallbackData* data) -> int {
            ServerGUI* gui = static_cast<ServerGUI*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                if (data->EventKey == ImGuiKey_UpArrow) {
                    if (gui->m_historyIndex < static_cast<int>(gui->m_commandHistory.size()) - 1) {
                        gui->m_historyIndex++;
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, gui->m_commandHistory[gui->m_commandHistory.size() - 1 - gui->m_historyIndex].c_str());
                    }
                } else if (data->EventKey == ImGuiKey_DownArrow) {
                    if (gui->m_historyIndex > 0) {
                        gui->m_historyIndex--;
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, gui->m_commandHistory[gui->m_commandHistory.size() - 1 - gui->m_historyIndex].c_str());
                    } else if (gui->m_historyIndex == 0) {
                        gui->m_historyIndex = -1;
                        data->DeleteChars(0, data->BufTextLen);
                    }
                }
            }
            return 0;
        }, this)) {
        
        std::string cmd = m_commandBuffer;
        if (!cmd.empty()) {
            processCommand(cmd);
            m_commandHistory.push_back(cmd);
            m_historyIndex = -1;
        }
        m_commandBuffer[0] = '\0';
        reclaim_focus = true;
        m_scrollToBottom = true;
    }
    
    ImGui::PopItemWidth();
    
    // Auto-focus on command input
    if (reclaim_focus) {
        ImGui::SetKeyboardFocusHere(-1);
    }
}

void ServerGUI::startServer() {
    m_server->setConfig(m_setupConfig);
    if (m_server->start()) {
        m_state = ServerGUIState::RUNNING;
    }
}

void ServerGUI::stopServer() {
    if (m_server) {
        m_server->stop();
    }
    m_state = ServerGUIState::STOPPED;
}

void ServerGUI::processCommand(const std::string& command) {
    if (m_server) {
        m_server->executeCommand(command);
    }
}

} // namespace Server
