// Minecraft CPP - Dedicated Server Entry Point
// This is a separate executable for running a dedicated server with GUI

#include "ServerGUI.h"
#include "../Core/Logger.h"

#include <iostream>
#include <csignal>

// Global flag for clean shutdown
static volatile bool g_running = true;

void signalHandler(int signum) {
    LOG_INFO("Received signal " + std::to_string(signum) + ", shutting down...");
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers for clean shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
#ifdef _DEBUG
    // Enable file logging in Debug mode
    Logger::instance().setLevel(LogLevel::DEBUG);
    Logger::instance().enableFileLogging("server_debug.log");
    LOG_DEBUG("Debug logging enabled - writing to server_debug.log");
#endif
    
    LOG_INFO("=== Minecraft CPP Dedicated Server ===");
    
    // Check for command line mode
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "-h") {
            headless = true;
        }
        if (arg == "--log" || arg == "-l") {
            // Enable file logging even in Release mode
            Logger::instance().enableFileLogging("server.log");
            LOG_INFO("File logging enabled - writing to server.log");
        }
    }
    
    if (headless) {
        // TODO: Implement headless console mode
        LOG_INFO("Headless mode not yet implemented. Use GUI mode.");
        return 1;
    }
    
    // Run GUI server
    Server::ServerGUI gui;
    
    if (!gui.init()) {
        LOG_ERROR("Failed to initialize server GUI");
        return 1;
    }
    
    LOG_INFO("Server GUI initialized");
    
    // Main loop
    while (g_running && gui.update()) {
        // GUI handles everything
    }
    
    gui.shutdown();
    
    LOG_INFO("Server shutdown complete");
    
#ifdef _DEBUG
    Logger::instance().disableFileLogging();
#endif
    
    return 0;
}
