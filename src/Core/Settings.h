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
    
    // Shadow method: 0 = Shadow Map (fast), 1 = Ray Traced (accurate)
    int shadowMethod = 0;
    static constexpr int SHADOW_MAP = 0;
    static constexpr int SHADOW_RAY_TRACED = 1;
    static constexpr const char* SHADOW_METHOD_NAMES[] = { "Shadow Map", "Ray Traced" };
    
    // Anti-aliasing method: 0 = None, 1 = FXAA, 2 = TAA
    int aaMethod = 1; // Default to FXAA
    static constexpr int AA_NONE = 0;
    static constexpr int AA_FXAA = 1;
    static constexpr int AA_TAA = 2;
    static constexpr const char* AA_METHOD_NAMES[] = { "None", "FXAA", "TAA" };
    
    // Graphics presets
    int graphicsPreset = 2; // Default to Medium
    static constexpr int PRESET_LOW = 0;
    static constexpr int PRESET_MEDIUM = 1;
    static constexpr int PRESET_HIGH = 2;
    static constexpr int PRESET_RT_LOW = 3;
    static constexpr int PRESET_RT_MEDIUM = 4;
    static constexpr int PRESET_RT_HIGH = 5;
    static constexpr int PRESET_CUSTOM = 6;
    static constexpr const char* PRESET_NAMES[] = { 
        "Low", "Medium", "High", 
        "RT Low", "RT Medium", "RT High",
        "Custom"
    };
    static constexpr int NUM_PRESETS = 7;
    
    void applyPreset(int preset) {
        graphicsPreset = preset;
        switch (preset) {
            case PRESET_LOW:
                renderDistance = 6;
                enableShadows = true;
                shadowDistance = 80.0f;
                shadowMethod = SHADOW_MAP;
                enableSSAO = false;
                enableVolumetrics = false;
                aaMethod = AA_NONE;
                enableRayTracing = false;
                break;
            case PRESET_MEDIUM:
                renderDistance = 10;
                enableShadows = true;
                shadowDistance = 120.0f;
                shadowMethod = SHADOW_MAP;
                enableSSAO = true;
                enableVolumetrics = false;
                aaMethod = AA_FXAA;
                enableRayTracing = false;
                break;
            case PRESET_HIGH:
                renderDistance = 16;
                enableShadows = true;
                shadowDistance = 160.0f;
                shadowMethod = SHADOW_MAP;
                enableSSAO = true;
                enableVolumetrics = true;
                aaMethod = AA_TAA;
                enableRayTracing = false;
                break;
            case PRESET_RT_LOW:
                renderDistance = 8;
                enableShadows = true;
                shadowDistance = 100.0f;
                shadowMethod = SHADOW_RAY_TRACED;
                enableSSAO = false;
                enableVolumetrics = false;
                aaMethod = AA_FXAA;
                enableRayTracing = true;
                rayTracingQuality = RT_QUALITY_LOW;
                rtShadows = true;
                rtReflections = false;
                break;
            case PRESET_RT_MEDIUM:
                renderDistance = 12;
                enableShadows = true;
                shadowDistance = 140.0f;
                shadowMethod = SHADOW_RAY_TRACED;
                enableSSAO = true;
                enableVolumetrics = true;
                aaMethod = AA_TAA;
                enableRayTracing = true;
                rayTracingQuality = RT_QUALITY_MEDIUM;
                rtShadows = true;
                rtReflections = true;
                break;
            case PRESET_RT_HIGH:
                renderDistance = 16;
                enableShadows = true;
                shadowDistance = 180.0f;
                shadowMethod = SHADOW_RAY_TRACED;
                enableSSAO = true;
                enableVolumetrics = true;
                aaMethod = AA_TAA;
                enableRayTracing = true;
                rayTracingQuality = RT_QUALITY_HIGH;
                rtShadows = true;
                rtReflections = true;
                break;
            case PRESET_CUSTOM:
                // Custom - don't change anything
                break;
        }
    }
    
    // Ray Tracing settings (OpenGL compute shader based)
    bool enableRayTracing = false; // Disabled by default (experimental)
    int rayTracingQuality = 1;     // 0 = Low, 1 = Medium, 2 = High
    static constexpr int RT_QUALITY_LOW = 0;
    static constexpr int RT_QUALITY_MEDIUM = 1;
    
    // Resource Pack settings (PBRANDPOM)
    bool usePBRResourcePack = false; // Use PBRANDPOM resource pack textures
    bool pbrSettingsChanged = false; // Flag to trigger shadow/lighting recalculation when PBR mode changes
    bool enableParallaxMapping = false; // Enable 3D parallax occlusion mapping for PBR textures
    static constexpr int RT_QUALITY_HIGH = 2;
    static constexpr const char* RT_QUALITY_NAMES[] = { "Low", "Medium", "High" };
    bool rtShadows = true;         // Ray traced shadows
    bool rtReflections = true;    // Ray traced water reflections
    
    int fullscreen = 0; // 0: Windowed, 1: Fullscreen, 2: Borderless
    // Debug visualization options
    bool debugShowTAA = false; // Show TAA motion/weight overlay
    bool debugNoTexture = false; // Render geometry without textures (flat color)
    bool debugWireframe = false; // Render in wireframe
    bool debugShowNormals = false; // Visualize normals as colors
    
    // UI settings
    bool enableTooltips = true; // Show tooltips on hover
    
    // Player customization
    std::string playerNickname = "Steve";
    int playerModelIndex = 0; // 0 = Half-Life, 1-4 = Quaternius characters
    
    // Available player models
    static constexpr int NUM_PLAYER_MODELS = 5;
    static constexpr const char* PLAYER_MODEL_NAMES[] = {
        "Half-Life (Default)",
        "Male Character 1",
        "Male Character 2", 
        "Female Character 1",
        "Female Character 2"
    };
    static constexpr const char* PLAYER_MODEL_PATHS[] = {
        "assets/models/Player/scene.gltf",
        "assets/models/Characters/Character_Male_1.gltf",
        "assets/models/Characters/Character_Male_2.gltf",
        "assets/models/Characters/Character_Female_1.gltf",
        "assets/models/Characters/Character_Female_2.gltf"
    };
    
    // Multiplayer settings
    std::string lastPlayerName = "Player";
    std::string lastServerAddress = "localhost";
    int lastServerPort = 25565;
    bool chatShowTimestamps = false;
    float chatFadeDelay = 10.0f; // Seconds before messages start fading
    int maxChatMessages = 100; // Maximum messages to keep in history
    
    // Audio settings
    float masterVolume = 1.0f;
    float musicVolume = 0.5f;
    float soundVolume = 1.0f; // Sound effects (blocks, mobs, player)
    float ambientVolume = 0.7f;
    
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
                    else if (key == "aaMethod") aaMethod = std::stoi(value);
                    else if (key == "graphicsPreset") graphicsPreset = std::stoi(value);
                    else if (key == "enableRayTracing") enableRayTracing = (value == "1");
                    else if (key == "rayTracingQuality") rayTracingQuality = std::stoi(value);
                    else if (key == "shadowMethod") shadowMethod = std::stoi(value);
                    else if (key == "rtShadows") rtShadows = (value == "1");
                    else if (key == "rtReflections") rtReflections = (value == "1");
                    else if (key == "usePBRResourcePack") usePBRResourcePack = (value == "1");
                    else if (key == "debugShowTAA") debugShowTAA = (value == "1");
                    else if (key == "debugNoTexture") debugNoTexture = (value == "1");
                    else if (key == "debugWireframe") debugWireframe = (value == "1");
                    else if (key == "debugShowNormals") debugShowNormals = (value == "1");
                    else if (key == "shadowDistance") shadowDistance = std::stof(value);
                    else if (key == "fullscreen") fullscreen = std::stoi(value);
                    // UI
                    else if (key == "enableTooltips") enableTooltips = (value == "1");
                    // Player customization
                    else if (key == "playerNickname") playerNickname = value;
                    else if (key == "playerModelIndex") playerModelIndex = std::stoi(value);
                    // Multiplayer
                    else if (key == "lastPlayerName") lastPlayerName = value;
                    else if (key == "lastServerAddress") lastServerAddress = value;
                    else if (key == "lastServerPort") lastServerPort = std::stoi(value);
                    else if (key == "chatShowTimestamps") chatShowTimestamps = (value == "1");
                    else if (key == "chatFadeDelay") chatFadeDelay = std::stof(value);
                    else if (key == "maxChatMessages") maxChatMessages = std::stoi(value);
                    // Audio
                    else if (key == "masterVolume") masterVolume = std::stof(value);
                    else if (key == "musicVolume") musicVolume = std::stof(value);
                    else if (key == "soundVolume") soundVolume = std::stof(value);
                    else if (key == "ambientVolume") ambientVolume = std::stof(value);
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
        file << "aaMethod=" << aaMethod << "\n";
        file << "graphicsPreset=" << graphicsPreset << "\n";
        file << "enableRayTracing=" << (enableRayTracing ? "1" : "0") << "\n";
        file << "rayTracingQuality=" << rayTracingQuality << "\n";
        file << "shadowMethod=" << shadowMethod << "\n";
        file << "rtShadows=" << (rtShadows ? "1" : "0") << "\n";
        file << "rtReflections=" << (rtReflections ? "1" : "0") << "\n";
        file << "usePBRResourcePack=" << (usePBRResourcePack ? "1" : "0") << "\n";
        file << "debugShowTAA=" << (debugShowTAA ? "1" : "0") << "\n";
        file << "debugNoTexture=" << (debugNoTexture ? "1" : "0") << "\n";
        file << "debugWireframe=" << (debugWireframe ? "1" : "0") << "\n";
        file << "debugShowNormals=" << (debugShowNormals ? "1" : "0") << "\n";
        file << "fullscreen=" << fullscreen << "\n";
        // UI
        file << "enableTooltips=" << (enableTooltips ? "1" : "0") << "\n";
        // Player customization
        file << "playerNickname=" << playerNickname << "\n";
        file << "playerModelIndex=" << playerModelIndex << "\n";
        // Multiplayer
        file << "lastPlayerName=" << lastPlayerName << "\n";
        file << "lastServerAddress=" << lastServerAddress << "\n";
        file << "lastServerPort=" << lastServerPort << "\n";
        file << "chatShowTimestamps=" << (chatShowTimestamps ? "1" : "0") << "\n";
        file << "chatFadeDelay=" << chatFadeDelay << "\n";
        file << "maxChatMessages=" << maxChatMessages << "\n";
        // Audio
        file << "masterVolume=" << masterVolume << "\n";
        file << "musicVolume=" << musicVolume << "\n";
        file << "soundVolume=" << soundVolume << "\n";
        file << "ambientVolume=" << ambientVolume << "\n";
        
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
