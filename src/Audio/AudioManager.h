#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include <random>
#include <mutex>

namespace Audio {

// Sound categories for volume control
enum class SoundCategory {
    MASTER,
    MUSIC,
    AMBIENT,
    BLOCKS,
    MOBS,
    PLAYER,
    WEATHER,
    UI
};

// Sound types for organized loading
enum class SoundType {
    NONE = 0,  // No sound (for default/unimplemented)
    
    // Ambient
    CAVE_AMBIENT,
    UNDERWATER_ENTER,
    UNDERWATER_EXIT,
    WATER_AMBIENT,
    
    // Weather
    RAIN,
    THUNDER,
    
    // Player
    FOOTSTEP_GRASS,
    FOOTSTEP_STONE,
    FOOTSTEP_WOOD,
    FOOTSTEP_SAND,
    FOOTSTEP_GRAVEL,
    FOOTSTEP_SNOW,
    FOOTSTEP_WATER,
    PLAYER_HURT,
    PLAYER_FALL_BIG,
    PLAYER_FALL_SMALL,
    PLAYER_EAT,
    PLAYER_DRINK,
    PLAYER_BURP,
    
    // Blocks - Dig/Break
    DIG_GRASS,
    DIG_STONE,
    DIG_WOOD,
    DIG_SAND,
    DIG_GRAVEL,
    DIG_GLASS,
    DIG_SNOW,
    DIG_CLOTH,
    
    // Block place
    PLACE_GRASS,
    PLACE_STONE,
    PLACE_WOOD,
    PLACE_SAND,
    PLACE_GRAVEL,
    PLACE_GLASS,
    PLACE_SNOW,
    
    // UI
    UI_CLICK,
    UI_LEVELUP,
    
    // Mobs
    MOB_PIG_SAY,
    MOB_PIG_DEATH,
    MOB_PIG_STEP,
    MOB_CHICKEN_SAY,
    MOB_CHICKEN_HURT,
    MOB_CHICKEN_PLOP,
    MOB_SHEEP_SAY,
    MOB_SHEEP_STEP,
    MOB_COW_SAY,
    MOB_COW_HURT,
    MOB_SKELETON_SHOOT,
    MOB_WOLF_BARK,
    MOB_WOLF_GROWL,
    MOB_WOLF_HOWL,
    MOB_WOLF_HURT,
    MOB_WOLF_DEATH,
    
    // Other
    EXPLOSION,
    FIRE,
    FIRE_IGNITE,
    WATER_SPLASH,
    WATER_SWIM,
    LAVA,
    LAVA_POP,
    PORTAL,
    BOW_SHOOT,
    DOOR_OPEN,
    DOOR_CLOSE,
    CHEST_OPEN,
    CHEST_CLOSE,
    
    // Music
    MUSIC_MENU,
    MUSIC_GAME
};

// Forward declarations
struct SoundData;
struct PlayingSound;

class AudioManager {
public:
    static AudioManager& instance();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    void update(float deltaTime);
    
    // Sound loading
    bool loadSound(SoundType type, const std::string& path);
    bool loadSoundVariants(SoundType type, const std::string& basePath, int count);
    void loadAllSounds();
    
    // Sound playback - returns handle for the playing sound
    uint32_t playSound(SoundType type, float volume = 1.0f, float pitch = 1.0f);
    uint32_t playSoundAt(SoundType type, const glm::vec3& position, float volume = 1.0f, float pitch = 1.0f);
    uint32_t playSoundAtWithRange(SoundType type, const glm::vec3& position, float volume, float maxDistance, float pitch = 1.0f);
    uint32_t playMusic(SoundType type, bool loop = true, float fadeIn = 1.0f);
    
    // Sound control
    void stopSound(uint32_t handle);
    void stopAllSounds();
    void stopMusic(float fadeOut = 1.0f);
    void pauseAll();
    void resumeAll();
    
    // Volume control (0.0 - 1.0)
    void setMasterVolume(float volume);
    void setCategoryVolume(SoundCategory category, float volume);
    float getMasterVolume() const { return m_masterVolume; }
    float getCategoryVolume(SoundCategory category) const;
    
    // 3D audio
    void setListenerPosition(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up);
    
    // Ambient system
    void setInCave(bool inCave) { m_inCave = inCave; }
    void setUnderwater(bool underwater);
    void setRaining(bool raining);
    void setThundering(bool thundering);
    
    // Status
    bool isInitialized() const { return m_initialized; }
    bool isMusicPlaying() const { return m_currentMusic != 0; }
    
private:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    
    // Internal helpers
    float calculateAttenuation(const glm::vec3& soundPos, float maxDistance = 32.0f);
    float getEffectiveVolume(SoundCategory category, float baseVolume);
    SoundCategory getCategoryForType(SoundType type);
    
    // Ambient sound logic
    void updateAmbient(float deltaTime);
    void updateMusic(float deltaTime);
    
    bool m_initialized = false;
    bool m_paused = false;
    
    // Volume levels
    float m_masterVolume = 1.0f;
    std::map<SoundCategory, float> m_categoryVolumes;
    
    // 3D listener
    glm::vec3 m_listenerPos{0.0f};
    glm::vec3 m_listenerForward{0.0f, 0.0f, -1.0f};
    glm::vec3 m_listenerUp{0.0f, 1.0f, 0.0f};
    
    // Sound data storage
    std::map<SoundType, std::vector<std::shared_ptr<SoundData>>> m_sounds;
    std::vector<std::unique_ptr<PlayingSound>> m_playingSounds;
    uint32_t m_nextHandle = 1;
    
    // Music state
    uint32_t m_currentMusic = 0;
    float m_musicFadeTimer = 0.0f;
    float m_musicFadeDuration = 0.0f;
    bool m_musicFadingOut = false;
    
    // Ambient state
    bool m_inCave = false;
    bool m_underwater = false;
    bool m_wasUnderwater = false;
    bool m_raining = false;
    bool m_thundering = false;
    float m_ambientTimer = 0.0f;
    float m_nextAmbientTime = 30.0f;
    float m_waterAmbientTimer = 0.0f;
    float m_nextWaterAmbientTime = 12.0f;
    uint32_t m_rainSound = 0;
    
    // Random
    std::mt19937 m_rng;
    
    // Thread safety
    std::mutex m_mutex;
};

// Helper function to get material sound type from block type
SoundType getStepSoundForBlock(uint8_t blockType);
SoundType getDigSoundForBlock(uint8_t blockType);
SoundType getPlaceSoundForBlock(uint8_t blockType);

} // namespace Audio
