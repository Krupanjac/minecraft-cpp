#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include "../Util/Config.h"

class Settings {
public:
    static Settings& instance() {
        static Settings instance;
        return instance;
    }

    // Settings variables
    int renderDistance = RENDER_DISTANCE;
    float fov = FOV;
    float mouseSensitivity = MOUSE_SENSITIVITY;
    float aoStrength = 1.0f; // Multiplier for AO
    float gamma = 2.2f;
    float exposure = 1.0f;
    // Sizes for celestial bodies (in world units)
    float sunSize = 5.0f;
    float moonSize = 4.0f;
    bool vsync = true;
    bool enableSSAO = true;
    bool enableVolumetrics = true;
    bool enableTAA = false; // Disabled by default due to potential jitter/shaking artifacts
    bool enableShadows = true;
    float shadowDistance = 160.0f;
    int fullscreen = 0; // 0: Windowed, 1: Fullscreen, 2: Borderless
    // Debug visualization options
    bool debugShowTAA = false; // Show TAA motion/weight overlay
    bool debugNoTexture = false; // Render geometry without textures (flat color)
    bool debugWireframe = false; // Render in wireframe
    bool debugShowNormals = false; // Visualize normals as colors
    
    struct KeyBindings {
        int forward = 87;  // W
        int backward = 83; // S
        int left = 65;     // A
        int right = 68;    // D
        int jump = 32;     // Space
        int sprint = 340;  // Left Shift (GLFW)
        int sneak = 341;   // Left Ctrl (GLFW)
        int inventory = 69;// E
    } keys;

    void load() {
        std::ifstream file("settings.ini");
        if (!file.is_open()) return;

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream is_line(line);
            std::string key;
            if (std::getline(is_line, key, '=')) {
                std::string value;
                if (std::getline(is_line, value)) {
                    if (key == "renderDistance") renderDistance = std::stoi(value);
                    else if (key == "fov") fov = std::stof(value);
                    else if (key == "mouseSensitivity") mouseSensitivity = std::stof(value);
                    else if (key == "aoStrength") aoStrength = std::stof(value);
                    else if (key == "gamma") gamma = std::stof(value);
                    else if (key == "exposure") exposure = std::stof(value);
                    else if (key == "sunSize") sunSize = std::stof(value);
                    else if (key == "moonSize") moonSize = std::stof(value);
                    else if (key == "vsync") vsync = (value == "1");
                    else if (key == "enableSSAO") enableSSAO = (value == "1");
                    else if (key == "enableVolumetrics") enableVolumetrics = (value == "1");
                    else if (key == "enableTAA") enableTAA = (value == "1");
                    else if (key == "enableShadows") enableShadows = (value == "1");
                    else if (key == "shadowDistance") shadowDistance = std::stof(value);
                    else if (key == "debugShowTAA") debugShowTAA = (value == "1");
                    else if (key == "debugNoTexture") debugNoTexture = (value == "1");
                    else if (key == "debugWireframe") debugWireframe = (value == "1");
                    else if (key == "debugShowNormals") debugShowNormals = (value == "1");
                    else if (key == "shadowDistance") shadowDistance = std::stof(value);
                    else if (key == "fullscreen") fullscreen = std::stoi(value);
                    // Keys
                    else if (key == "key_forward") keys.forward = std::stoi(value);
                    else if (key == "key_backward") keys.backward = std::stoi(value);
                    else if (key == "key_left") keys.left = std::stoi(value);
                    else if (key == "key_right") keys.right = std::stoi(value);
                    else if (key == "key_jump") keys.jump = std::stoi(value);
                    else if (key == "key_sprint") keys.sprint = std::stoi(value);
                    else if (key == "key_sneak") keys.sneak = std::stoi(value);
                    else if (key == "key_inventory") keys.inventory = std::stoi(value);
                }
            }
        }
    }

    void save() {
        std::ofstream file("settings.ini");
        if (!file.is_open()) return;
        
        file << "renderDistance=" << renderDistance << "\n";
        file << "fov=" << fov << "\n";
        file << "mouseSensitivity=" << mouseSensitivity << "\n";
        file << "aoStrength=" << aoStrength << "\n";
        file << "gamma=" << gamma << "\n";
        file << "exposure=" << exposure << "\n";
        file << "sunSize=" << sunSize << "\n";
        file << "moonSize=" << moonSize << "\n";
        file << "vsync=" << (vsync ? "1" : "0") << "\n";
        file << "enableSSAO=" << (enableSSAO ? "1" : "0") << "\n";
        file << "enableVolumetrics=" << (enableVolumetrics ? "1" : "0") << "\n";
        file << "enableTAA=" << (enableTAA ? "1" : "0") << "\n";
        file << "enableShadows=" << (enableShadows ? "1" : "0") << "\n";
        file << "shadowDistance=" << shadowDistance << "\n";
        file << "debugShowTAA=" << (debugShowTAA ? "1" : "0") << "\n";
        file << "debugNoTexture=" << (debugNoTexture ? "1" : "0") << "\n";
        file << "debugWireframe=" << (debugWireframe ? "1" : "0") << "\n";
        file << "debugShowNormals=" << (debugShowNormals ? "1" : "0") << "\n";
        file << "fullscreen=" << fullscreen << "\n";
        
        file << "key_forward=" << keys.forward << "\n";
        file << "key_backward=" << keys.backward << "\n";
        file << "key_left=" << keys.left << "\n";
        file << "key_right=" << keys.right << "\n";
        file << "key_jump=" << keys.jump << "\n";
        file << "key_sprint=" << keys.sprint << "\n";
        file << "key_sneak=" << keys.sneak << "\n";
        file << "key_inventory=" << keys.inventory << "\n";
    }

private:
    Settings() { load(); }
    ~Settings() { save(); }
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};
