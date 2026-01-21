// Minecraft CPP - Dedicated Server Entry Point
// This is a separate executable for running a dedicated server with GUI

#include "ServerGUI.h"
#include "../Core/Logger.h"

#include <iostream>

int main(int argc, char* argv[]) {
    LOG_INFO("=== Minecraft CPP Dedicated Server ===");
    
    // Check for command line mode
    bool headless = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "-h") {
            headless = true;
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
    while (gui.update()) {
        // GUI handles everything
    }
    
    gui.shutdown();
    
    LOG_INFO("Server shutdown complete");
    return 0;
}
