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
    bool vsync = true;

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
                    else if (key == "vsync") vsync = (value == "1");
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
        file << "vsync=" << (vsync ? 1 : 0) << "\n";
    }

private:
    Settings() { load(); }
    ~Settings() { save(); }
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;
};
