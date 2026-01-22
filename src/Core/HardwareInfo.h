#pragma once

#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <fstream>
#include <sstream>
#endif

/**
 * Cross-platform hardware detection utility
 * Provides CPU, memory, and GPU information for dynamic performance tuning
 */
class HardwareInfo {
public:
    struct CPUInfo {
        unsigned int physicalCores = 0;
        unsigned int logicalCores = 0;
        std::string name;
    };
    
    struct MemoryInfo {
        size_t totalPhysicalMB = 0;
        size_t availablePhysicalMB = 0;
    };
    
    struct GPUInfo {
        std::string renderer;
        std::string vendor;
        std::string version;
        int maxTextureSize = 0;
        int maxComputeWorkGroupCount[3] = {0, 0, 0};
        int maxComputeWorkGroupSize[3] = {0, 0, 0};
        bool supportsComputeShaders = false;
    };

    // Get CPU information
    static CPUInfo getCPUInfo() {
        CPUInfo info;
        
        // Logical cores (threads) - cross-platform via std::thread
        info.logicalCores = std::thread::hardware_concurrency();
        if (info.logicalCores == 0) info.logicalCores = 4; // Fallback
        
#ifdef _WIN32
        // Windows: Get physical cores
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        // Get processor name from registry
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char cpuName[256] = {0};
            DWORD bufSize = sizeof(cpuName);
            RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpuName, &bufSize);
            info.name = cpuName;
            RegCloseKey(hKey);
        }
        
        // Physical cores require GetLogicalProcessorInformation
        DWORD length = 0;
        GetLogicalProcessorInformation(nullptr, &length);
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(buffer.data(), &length)) {
            info.physicalCores = 0;
            for (const auto& proc : buffer) {
                if (proc.Relationship == RelationProcessorCore) {
                    info.physicalCores++;
                }
            }
        }
        if (info.physicalCores == 0) info.physicalCores = info.logicalCores / 2;
        
#elif defined(__linux__)
        // Linux: Parse /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        int cores = 0;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos && info.name.empty()) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.name = line.substr(pos + 2);
                }
            }
            if (line.find("cpu cores") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    cores = std::stoi(line.substr(pos + 2));
                }
            }
        }
        info.physicalCores = cores > 0 ? cores : info.logicalCores / 2;
#else
        // Fallback for other platforms
        info.physicalCores = info.logicalCores / 2;
        info.name = "Unknown CPU";
#endif
        
        if (info.physicalCores == 0) info.physicalCores = 1;
        return info;
    }
    
    // Get memory information
    static MemoryInfo getMemoryInfo() {
        MemoryInfo info;
        
#ifdef _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            info.totalPhysicalMB = static_cast<size_t>(memInfo.ullTotalPhys / (1024 * 1024));
            info.availablePhysicalMB = static_cast<size_t>(memInfo.ullAvailPhys / (1024 * 1024));
        }
#elif defined(__linux__)
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") != std::string::npos) {
                std::istringstream iss(line);
                std::string label;
                size_t kb;
                iss >> label >> kb;
                info.totalPhysicalMB = kb / 1024;
            }
            if (line.find("MemAvailable:") != std::string::npos) {
                std::istringstream iss(line);
                std::string label;
                size_t kb;
                iss >> label >> kb;
                info.availablePhysicalMB = kb / 1024;
            }
        }
#endif
        
        return info;
    }
    
    // Calculate optimal thread pool size based on hardware
    static unsigned int getOptimalThreadCount() {
        CPUInfo cpu = getCPUInfo();
        
        // Use physical cores for compute-heavy work (chunk generation, meshing)
        // Leave 1-2 cores free for main thread and OS
        unsigned int optimal = cpu.physicalCores;
        if (optimal > 2) optimal -= 1; // Reserve one core
        if (optimal < 2) optimal = 2;  // Minimum 2 threads
        if (optimal > 16) optimal = 16; // Cap at 16 for diminishing returns
        
        return optimal;
    }
    
    // Get recommended settings based on hardware
    static int getRecommendedRenderDistance() {
        MemoryInfo mem = getMemoryInfo();
        
        // Scale render distance based on available RAM
        // Each chunk uses ~4KB for blocks, plus mesh data
        if (mem.totalPhysicalMB >= 16000) return 16;  // 16GB+ RAM
        if (mem.totalPhysicalMB >= 8000) return 12;   // 8GB+ RAM
        if (mem.totalPhysicalMB >= 4000) return 8;    // 4GB+ RAM
        return 6; // Low memory
    }
    
    static int getRecommendedMaxChunksPerFrame() {
        CPUInfo cpu = getCPUInfo();
        
        // More cores = can process more chunks per frame
        if (cpu.physicalCores >= 8) return 8;
        if (cpu.physicalCores >= 4) return 4;
        return 2;
    }
    
    // GPU info needs to be set after OpenGL context is created
    // Call setGPUInfo() once from main after context creation
    static GPUInfo& getGPUInfo() {
        static GPUInfo gpuInfo;
        return gpuInfo;
    }
    
    static void setGPUInfo(const std::string& renderer, const std::string& vendor, const std::string& version) {
        GPUInfo& info = getGPUInfo();
        info.renderer = renderer;
        info.vendor = vendor;
        info.version = version;
    }
};
