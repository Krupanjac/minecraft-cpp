// Disable miniaudio's built-in Vorbis (we'll decode OGG files manually)
#define MA_NO_VORBIS

// miniaudio implementation
#define MINIAUDIO_IMPLEMENTATION
#include "../../external/miniaudio.h"

// stb_vorbis for OGG decoding (header-only mode, implementation in stb_vorbis_impl.c)
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

#include "AudioManager.h"
#include "../Core/Logger.h"
#include "../Core/Settings.h"
#include "../World/Block.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace Audio {

// Internal structures
struct SoundData {
    std::vector<float> samples;
    uint32_t sampleRate = 44100;
    uint32_t channels = 2;
    float duration = 0.0f;
    
    ~SoundData() = default;
};

struct PlayingSound {
    uint32_t handle = 0;
    std::shared_ptr<SoundData> data;
    SoundType type;
    SoundCategory category;
    
    size_t samplePosition = 0;
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;  // -1 = left, 0 = center, 1 = right
    
    bool is3D = false;
    glm::vec3 position{0.0f};
    
    bool loop = false;
    bool finished = false;
    
    // Fading
    float fadeVolume = 1.0f;
    float fadeTarget = 1.0f;
    float fadeSpeed = 0.0f;
};

// Global miniaudio device
static ma_device g_device;
static ma_device_config g_deviceConfig;
static bool g_deviceInitialized = false;

// Forward declaration for the audio callback to access playing sounds
static std::mutex* g_audioMutex = nullptr;
static std::vector<std::unique_ptr<PlayingSound>>* g_playingSounds = nullptr;
static float g_masterVolume = 1.0f;
static std::map<SoundCategory, float>* g_categoryVolumes = nullptr;
static glm::vec3 g_listenerPos{0.0f};
static glm::vec3 g_listenerForward{0.0f, 0.0f, -1.0f};
static glm::vec3 g_listenerUp{0.0f, 1.0f, 0.0f};

// Helper to get category volume
static float getCategoryVolumeStatic(SoundCategory category) {
    if (!g_categoryVolumes) return 1.0f;
    auto it = g_categoryVolumes->find(category);
    return it != g_categoryVolumes->end() ? it->second : 1.0f;
}

// Helper to calculate 3D attenuation
static float calculateAttenuationStatic(const glm::vec3& soundPos, float maxDistance = 32.0f) {
    float distance = glm::length(soundPos - g_listenerPos);
    if (distance >= maxDistance) return 0.0f;
    if (distance <= 1.0f) return 1.0f;
    return 1.0f - (distance / maxDistance);
}

// Helper to calculate stereo pan based on listener orientation
static void calculateStereoPanStatic(const glm::vec3& soundPos, float& outLeft, float& outRight) {
    glm::vec3 toSound = soundPos - g_listenerPos;
    if (glm::length2(toSound) <= 0.0001f) {
        outLeft = 1.0f;
        outRight = 1.0f;
        return;
    }

    glm::vec3 forward = glm::normalize(g_listenerForward);
    glm::vec3 up = glm::normalize(g_listenerUp);
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 dir = glm::normalize(toSound);

    float pan = std::clamp(glm::dot(dir, right), -1.0f, 1.0f);
    // Equal-power panning
    outLeft = std::sqrt(0.5f * (1.0f - pan));
    outRight = std::sqrt(0.5f * (1.0f + pan));
}

// Audio callback for miniaudio - this runs on audio thread
void audioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pDevice;
    (void)pInput;
    
    float* output = static_cast<float*>(pOutput);
    
    // Clear output buffer
    memset(output, 0, frameCount * 2 * sizeof(float));
    
    if (!g_audioMutex || !g_playingSounds) return;
    
    std::lock_guard<std::mutex> lock(*g_audioMutex);
    
    // Mix all playing sounds
    for (auto& sound : *g_playingSounds) {
        if (!sound || sound->finished || !sound->data) {
            continue;
        }
        
        // Calculate effective volume
        float categoryVol = getCategoryVolumeStatic(sound->category);
        float effectiveVolume = g_masterVolume * categoryVol * sound->volume * sound->fadeVolume;
        
        // Apply 3D attenuation
        float panLeft = 1.0f;
        float panRight = 1.0f;
        if (sound->is3D) {
            effectiveVolume *= calculateAttenuationStatic(sound->position);
            calculateStereoPanStatic(sound->position, panLeft, panRight);
        }
        
        if (effectiveVolume < 0.001f) continue;
        
        // Mix this sound into the output buffer
        auto& samples = sound->data->samples;
        uint32_t channels = sound->data->channels;
        size_t totalSamples = samples.size() / channels;
        
        for (ma_uint32 frame = 0; frame < frameCount; frame++) {
            if (sound->samplePosition >= totalSamples) {
                if (sound->loop) {
                    sound->samplePosition = 0;
                } else {
                    sound->finished = true;
                    break;
                }
            }
            
            // Get sample from sound data
            float leftSample = 0.0f;
            float rightSample = 0.0f;
            
            size_t sampleIndex = sound->samplePosition * channels;
            if (sampleIndex < samples.size()) {
                leftSample = samples[sampleIndex];
                if (channels >= 2 && sampleIndex + 1 < samples.size()) {
                    rightSample = samples[sampleIndex + 1];
                } else {
                    rightSample = leftSample; // Mono to stereo
                }
            }
            
            // Apply volume and mix
            output[frame * 2] += leftSample * effectiveVolume * panLeft;
            output[frame * 2 + 1] += rightSample * effectiveVolume * panRight;
            
            // Advance position (accounting for pitch would require resampling)
            sound->samplePosition++;
        }
    }
    
    // Clamp output to prevent clipping
    for (ma_uint32 i = 0; i < frameCount * 2; i++) {
        output[i] = std::clamp(output[i], -1.0f, 1.0f);
    }
}

AudioManager& AudioManager::instance() {
    static AudioManager instance;
    return instance;
}

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::initialize() {
    if (m_initialized) return true;
    
    LOG_INFO("Initializing audio system...");
    
    // Initialize random number generator
    std::random_device rd;
    m_rng.seed(rd());
    
    // Initialize default volumes
    m_categoryVolumes[SoundCategory::MASTER] = 1.0f;
    m_categoryVolumes[SoundCategory::MUSIC] = 0.5f;
    m_categoryVolumes[SoundCategory::AMBIENT] = 0.7f;
    m_categoryVolumes[SoundCategory::BLOCKS] = 1.0f;
    m_categoryVolumes[SoundCategory::MOBS] = 1.0f;
    m_categoryVolumes[SoundCategory::PLAYER] = 1.0f;
    m_categoryVolumes[SoundCategory::WEATHER] = 0.8f;
    m_categoryVolumes[SoundCategory::UI] = 1.0f;
    
    // Set up global pointers for audio callback
    g_audioMutex = &m_mutex;
    g_playingSounds = &m_playingSounds;
    g_categoryVolumes = &m_categoryVolumes;
    g_masterVolume = m_masterVolume;
    
    // Configure miniaudio device
    g_deviceConfig = ma_device_config_init(ma_device_type_playback);
    g_deviceConfig.playback.format = ma_format_f32;
    g_deviceConfig.playback.channels = 2;
    g_deviceConfig.sampleRate = 44100;
    g_deviceConfig.dataCallback = audioCallback;
    g_deviceConfig.pUserData = this;
    
    if (ma_device_init(NULL, &g_deviceConfig, &g_device) != MA_SUCCESS) {
        LOG_ERROR("Failed to initialize audio device");
        return false;
    }
    
    if (ma_device_start(&g_device) != MA_SUCCESS) {
        LOG_ERROR("Failed to start audio device");
        ma_device_uninit(&g_device);
        return false;
    }
    
    g_deviceInitialized = true;
    m_initialized = true;
    
    LOG_INFO("Audio system initialized successfully");
    return true;
}

void AudioManager::shutdown() {
    if (!m_initialized) return;
    
    LOG_INFO("Shutting down audio system...");
    
    stopAllSounds();
    
    if (g_deviceInitialized) {
        ma_device_uninit(&g_device);
        g_deviceInitialized = false;
    }
    
    m_sounds.clear();
    m_playingSounds.clear();
    m_initialized = false;
}

bool AudioManager::loadSound(SoundType type, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        LOG_WARNING("Sound file not found: " + path);
        return false;
    }
    
    auto soundData = std::make_shared<SoundData>();
    
    // Check if it's an OGG file - use stb_vorbis
    bool isOgg = path.size() >= 4 && 
                 (path.substr(path.size() - 4) == ".ogg" || path.substr(path.size() - 4) == ".OGG");
    
    if (isOgg) {
        // Use stb_vorbis for OGG files
        int channels, sampleRate;
        short* output;
        int samples = stb_vorbis_decode_filename(path.c_str(), &channels, &sampleRate, &output);
        
        if (samples <= 0) {
            LOG_WARNING("Failed to decode OGG file: " + path);
            return false;
        }
        
        soundData->sampleRate = sampleRate;
        soundData->channels = channels;
        soundData->samples.resize(samples * channels);
        soundData->duration = (float)samples / (float)sampleRate;
        
        // Convert from short to float
        for (int i = 0; i < samples * channels; i++) {
            soundData->samples[i] = output[i] / 32768.0f;
        }
        
        free(output);
    } else {
        // Use miniaudio decoder for other formats (MP3, WAV, FLAC)
        ma_decoder decoder;
        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 2, 44100);
        
        if (ma_decoder_init_file(path.c_str(), &decoderConfig, &decoder) != MA_SUCCESS) {
            LOG_WARNING("Failed to decode audio file: " + path);
            return false;
        }
        
        // Get total frame count
        ma_uint64 totalFrames;
        ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
        
        soundData->sampleRate = decoder.outputSampleRate;
        soundData->channels = decoder.outputChannels;
        soundData->samples.resize(totalFrames * decoder.outputChannels);
        soundData->duration = (float)totalFrames / (float)decoder.outputSampleRate;
    
        // Read all samples
        ma_uint64 framesRead;
        ma_decoder_read_pcm_frames(&decoder, soundData->samples.data(), totalFrames, &framesRead);
    
        ma_decoder_uninit(&decoder);
    }
    
    // Store the sound
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sounds[type].push_back(soundData);
    
    LOG_DEBUG("Loaded sound: " + path + " (" + std::to_string(soundData->duration) + "s)");
    return true;
}

bool AudioManager::loadSoundVariants(SoundType type, const std::string& basePath, int count) {
    bool anyLoaded = false;
    for (int i = 1; i <= count; i++) {
        std::string path = basePath + std::to_string(i) + ".ogg";
        if (loadSound(type, path)) {
            anyLoaded = true;
        }
    }
    return anyLoaded;
}

void AudioManager::loadAllSounds() {
    LOG_INFO("Loading all sound assets...");
    
    const std::string base = "assets/sounds/";
    
    // Ambient - Cave
    loadSoundVariants(SoundType::CAVE_AMBIENT, base + "ambient/cave/cave", 20);
    
    // Ambient - Underwater
    loadSoundVariants(SoundType::UNDERWATER_ENTER, base + "ambient/underwater/enter", 3);
    loadSoundVariants(SoundType::UNDERWATER_EXIT, base + "ambient/underwater/exit", 3);
    
    // Weather
    loadSoundVariants(SoundType::RAIN, base + "ambient/weather/rain", 4);
    loadSoundVariants(SoundType::THUNDER, base + "ambient/weather/thunder", 3);
    
    // Footsteps
    loadSoundVariants(SoundType::FOOTSTEP_GRASS, base + "step/grass", 6);
    loadSoundVariants(SoundType::FOOTSTEP_STONE, base + "step/stone", 6);
    loadSoundVariants(SoundType::FOOTSTEP_WOOD, base + "step/wood", 6);
    loadSoundVariants(SoundType::FOOTSTEP_SAND, base + "step/sand", 5);
    loadSoundVariants(SoundType::FOOTSTEP_GRAVEL, base + "step/gravel", 4);
    loadSoundVariants(SoundType::FOOTSTEP_SNOW, base + "step/snow", 4);
    
    // Player
    loadSoundVariants(SoundType::PLAYER_HURT, base + "damage/hit", 3);
    loadSoundVariants(SoundType::PLAYER_FALL_BIG, base + "damage/fallbig", 2);
    loadSound(SoundType::PLAYER_FALL_SMALL, base + "damage/fallsmall.ogg");
    loadSoundVariants(SoundType::PLAYER_EAT, base + "random/eat", 3);
    loadSound(SoundType::PLAYER_DRINK, base + "random/drink.ogg");
    loadSound(SoundType::PLAYER_BURP, base + "random/burp.ogg");
    
    // Block dig/break
    loadSoundVariants(SoundType::DIG_GRASS, base + "dig/grass", 4);
    loadSoundVariants(SoundType::DIG_STONE, base + "dig/stone", 4);
    loadSoundVariants(SoundType::DIG_WOOD, base + "dig/wood", 4);
    loadSoundVariants(SoundType::DIG_SAND, base + "dig/sand", 4);
    loadSoundVariants(SoundType::DIG_GRAVEL, base + "dig/gravel", 4);
    loadSoundVariants(SoundType::DIG_GLASS, base + "dig/glass", 4);
    loadSoundVariants(SoundType::DIG_SNOW, base + "dig/snow", 4);
    loadSoundVariants(SoundType::DIG_CLOTH, base + "dig/cloth", 4);
    
    // UI
    loadSound(SoundType::UI_CLICK, base + "random/click.ogg");
    loadSound(SoundType::UI_LEVELUP, base + "random/levelup.ogg");
    
    // Mobs - Pig
    loadSoundVariants(SoundType::MOB_PIG_SAY, base + "mob/pig/say", 3);
    loadSound(SoundType::MOB_PIG_DEATH, base + "mob/pig/death.ogg");
    loadSoundVariants(SoundType::MOB_PIG_STEP, base + "mob/pig/step", 5);
    
    // Mobs - Chicken
    loadSoundVariants(SoundType::MOB_CHICKEN_SAY, base + "mob/chicken/say", 3);
    loadSoundVariants(SoundType::MOB_CHICKEN_HURT, base + "mob/chicken/hurt", 2);
    loadSound(SoundType::MOB_CHICKEN_PLOP, base + "mob/chicken/plop.ogg");
    
    // Mobs - Sheep
    loadSoundVariants(SoundType::MOB_SHEEP_SAY, base + "mob/sheep/say", 3);
    loadSoundVariants(SoundType::MOB_SHEEP_STEP, base + "mob/sheep/step", 5);
    
    // Mobs - Cow
    loadSoundVariants(SoundType::MOB_COW_SAY, base + "mob/cow/say", 4);
    loadSoundVariants(SoundType::MOB_COW_HURT, base + "mob/cow/hurt", 3);
    
    // Mobs - Skeleton
    loadSoundVariants(SoundType::MOB_SKELETON_SHOOT, base + "mob/skeleton/shoot", 2);
    
    // Mobs - Wolf
    loadSoundVariants(SoundType::MOB_WOLF_BARK, base + "mob/wolf/bark", 3);
    loadSoundVariants(SoundType::MOB_WOLF_GROWL, base + "mob/wolf/growl", 3);
    loadSoundVariants(SoundType::MOB_WOLF_HOWL, base + "mob/wolf/howl", 2);
    loadSoundVariants(SoundType::MOB_WOLF_HURT, base + "mob/wolf/hurt", 3);
    loadSound(SoundType::MOB_WOLF_DEATH, base + "mob/wolf/death.ogg");
    
    // Effects
    loadSoundVariants(SoundType::EXPLOSION, base + "random/explode", 4);
    loadSound(SoundType::FIRE, base + "fire/fire.ogg");
    loadSound(SoundType::FIRE_IGNITE, base + "fire/ignite.ogg");
    loadSound(SoundType::WATER_SPLASH, base + "liquid/splash.ogg");
    loadSoundVariants(SoundType::WATER_SWIM, base + "liquid/swim", 4);
    loadSoundVariants(SoundType::LAVA, base + "liquid/lava", 3);
    loadSoundVariants(SoundType::LAVA_POP, base + "liquid/lavapop", 4);
    loadSoundVariants(SoundType::PORTAL, base + "portal/portal", 3);
    loadSound(SoundType::BOW_SHOOT, base + "random/bow.ogg");
    
    // Doors/Chests
    loadSound(SoundType::DOOR_OPEN, base + "block/wooden_door/open.ogg");
    loadSound(SoundType::DOOR_CLOSE, base + "block/wooden_door/close.ogg");
    loadSound(SoundType::CHEST_OPEN, base + "block/chest/open.ogg");
    loadSound(SoundType::CHEST_CLOSE, base + "block/chest/close.ogg");
    
    // Music - check for mp3
    if (std::filesystem::exists(base + "menu/soundtrack1.mp3")) {
        loadSound(SoundType::MUSIC_MENU, base + "menu/soundtrack1.mp3");
    }
    
    LOG_INFO("Sound loading complete");
}

uint32_t AudioManager::playSound(SoundType type, float volume, float pitch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_sounds.find(type);
    if (it == m_sounds.end() || it->second.empty()) {
        return 0;
    }
    
    // Pick random variant
    std::uniform_int_distribution<size_t> dist(0, it->second.size() - 1);
    auto& soundData = it->second[dist(m_rng)];
    
    auto sound = std::make_unique<PlayingSound>();
    sound->handle = m_nextHandle++;
    sound->data = soundData;
    sound->type = type;
    sound->category = getCategoryForType(type);
    sound->volume = volume;
    sound->pitch = pitch;
    sound->is3D = false;
    
    uint32_t handle = sound->handle;
    m_playingSounds.push_back(std::move(sound));
    
    return handle;
}

uint32_t AudioManager::playSoundAt(SoundType type, const glm::vec3& position, float volume, float pitch) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_sounds.find(type);
    if (it == m_sounds.end() || it->second.empty()) {
        return 0;
    }
    
    // Check distance - don't play if too far
    float distance = glm::length(position - m_listenerPos);
    if (distance > 32.0f) {
        return 0;
    }
    
    // Pick random variant
    std::uniform_int_distribution<size_t> dist(0, it->second.size() - 1);
    auto& soundData = it->second[dist(m_rng)];
    
    auto sound = std::make_unique<PlayingSound>();
    sound->handle = m_nextHandle++;
    sound->data = soundData;
    sound->type = type;
    sound->category = getCategoryForType(type);
    sound->volume = volume;
    sound->pitch = pitch;
    sound->is3D = true;
    sound->position = position;
    
    uint32_t handle = sound->handle;
    m_playingSounds.push_back(std::move(sound));
    
    return handle;
}

uint32_t AudioManager::playMusic(SoundType type, bool loop, float fadeIn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Stop existing music
    if (m_currentMusic != 0) {
        for (auto& sound : m_playingSounds) {
            if (sound->handle == m_currentMusic) {
                sound->fadeTarget = 0.0f;
                sound->fadeSpeed = 1.0f / fadeIn;
            }
        }
    }
    
    auto it = m_sounds.find(type);
    if (it == m_sounds.end() || it->second.empty()) {
        return 0;
    }
    
    auto& soundData = it->second[0];  // Music usually has one variant
    
    auto sound = std::make_unique<PlayingSound>();
    sound->handle = m_nextHandle++;
    sound->data = soundData;
    sound->type = type;
    sound->category = SoundCategory::MUSIC;
    sound->volume = 1.0f;
    sound->pitch = 1.0f;
    sound->loop = loop;
    sound->fadeVolume = 0.0f;  // Start silent
    sound->fadeTarget = 1.0f;
    sound->fadeSpeed = 1.0f / fadeIn;
    
    m_currentMusic = sound->handle;
    m_playingSounds.push_back(std::move(sound));
    
    return m_currentMusic;
}

void AudioManager::stopSound(uint32_t handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& sound : m_playingSounds) {
        if (sound->handle == handle) {
            sound->finished = true;
            break;
        }
    }
}

void AudioManager::stopAllSounds() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playingSounds.clear();
    m_currentMusic = 0;
    m_rainSound = 0;
}

void AudioManager::stopMusic(float fadeOut) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& sound : m_playingSounds) {
        if (sound->handle == m_currentMusic) {
            sound->fadeTarget = 0.0f;
            sound->fadeSpeed = 1.0f / fadeOut;
            m_musicFadingOut = true;
        }
    }
}

void AudioManager::pauseAll() {
    m_paused = true;
}

void AudioManager::resumeAll() {
    m_paused = false;
}

void AudioManager::setMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
    g_masterVolume = m_masterVolume; // Update global for audio callback
}

void AudioManager::setCategoryVolume(SoundCategory category, float volume) {
    m_categoryVolumes[category] = std::clamp(volume, 0.0f, 1.0f);
}

float AudioManager::getCategoryVolume(SoundCategory category) const {
    auto it = m_categoryVolumes.find(category);
    return it != m_categoryVolumes.end() ? it->second : 1.0f;
}

void AudioManager::setListenerPosition(const glm::vec3& pos, const glm::vec3& forward, const glm::vec3& up) {
    m_listenerPos = pos;
    m_listenerForward = forward;
    m_listenerUp = up;
    g_listenerPos = pos; // Update global for audio callback
    g_listenerForward = forward;
    g_listenerUp = up;
}

void AudioManager::setUnderwater(bool underwater) {
    if (underwater && !m_wasUnderwater) {
        playSound(SoundType::UNDERWATER_ENTER);
    } else if (!underwater && m_wasUnderwater) {
        playSound(SoundType::UNDERWATER_EXIT);
    }
    m_wasUnderwater = m_underwater;
    m_underwater = underwater;
}

void AudioManager::setRaining(bool raining) {
    if (raining && !m_raining && m_rainSound == 0) {
        m_rainSound = playSound(SoundType::RAIN, 0.5f);
        // Mark rain sound as looping
        for (auto& sound : m_playingSounds) {
            if (sound->handle == m_rainSound) {
                sound->loop = true;
                break;
            }
        }
    } else if (!raining && m_rainSound != 0) {
        stopSound(m_rainSound);
        m_rainSound = 0;
    }
    m_raining = raining;
}

void AudioManager::setThundering(bool thundering) {
    m_thundering = thundering;
}

void AudioManager::update(float deltaTime) {
    if (!m_initialized || m_paused) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update ambient sounds
    updateAmbient(deltaTime);
    
    // Update music
    updateMusic(deltaTime);
    
    // Process each playing sound - handle fading
    for (auto& sound : m_playingSounds) {
        if (sound->finished) continue;
        
        // Update fade
        if (sound->fadeVolume != sound->fadeTarget) {
            if (sound->fadeVolume < sound->fadeTarget) {
                sound->fadeVolume = std::min(sound->fadeTarget, sound->fadeVolume + sound->fadeSpeed * deltaTime);
            } else {
                sound->fadeVolume = std::max(sound->fadeTarget, sound->fadeVolume - sound->fadeSpeed * deltaTime);
            }
            
            // Check if faded out
            if (sound->fadeVolume <= 0.001f && sound->fadeTarget <= 0.0f) {
                sound->finished = true;
                if (sound->handle == m_currentMusic) {
                    m_currentMusic = 0;
                    m_musicFadingOut = false;
                }
            }
        }
    }
    
    // Remove finished sounds
    m_playingSounds.erase(
        std::remove_if(m_playingSounds.begin(), m_playingSounds.end(),
            [](const auto& s) { return s->finished; }),
        m_playingSounds.end());
}

void AudioManager::updateAmbient(float deltaTime) {
    // Random cave ambient sounds
    if (m_inCave) {
        m_ambientTimer -= deltaTime;
        if (m_ambientTimer <= 0.0f) {
            playSound(SoundType::CAVE_AMBIENT, 0.3f);
            
            // Next ambient in 30-120 seconds
            std::uniform_real_distribution<float> dist(30.0f, 120.0f);
            m_nextAmbientTime = dist(m_rng);
            m_ambientTimer = m_nextAmbientTime;
        }
    }
    
    // Random thunder during storms
    if (m_thundering) {
        static float thunderTimer = 0.0f;
        thunderTimer -= deltaTime;
        if (thunderTimer <= 0.0f) {
            playSound(SoundType::THUNDER, 0.8f);
            
            std::uniform_real_distribution<float> dist(5.0f, 30.0f);
            thunderTimer = dist(m_rng);
        }
    }
}

void AudioManager::updateMusic(float deltaTime) {
    (void)deltaTime;
    // Music loop checking is handled in main update
}

float AudioManager::calculateAttenuation(const glm::vec3& soundPos, float maxDistance) {
    float distance = glm::length(soundPos - m_listenerPos);
    if (distance >= maxDistance) return 0.0f;
    if (distance <= 1.0f) return 1.0f;
    
    // Linear falloff
    return 1.0f - (distance / maxDistance);
}

float AudioManager::getEffectiveVolume(SoundCategory category, float baseVolume) {
    float categoryVol = getCategoryVolume(category);
    return m_masterVolume * categoryVol * baseVolume;
}

SoundCategory AudioManager::getCategoryForType(SoundType type) {
    switch (type) {
        case SoundType::MUSIC_MENU:
        case SoundType::MUSIC_GAME:
            return SoundCategory::MUSIC;
            
        case SoundType::CAVE_AMBIENT:
        case SoundType::UNDERWATER_ENTER:
        case SoundType::UNDERWATER_EXIT:
            return SoundCategory::AMBIENT;
            
        case SoundType::RAIN:
        case SoundType::THUNDER:
            return SoundCategory::WEATHER;
            
        case SoundType::FOOTSTEP_GRASS:
        case SoundType::FOOTSTEP_STONE:
        case SoundType::FOOTSTEP_WOOD:
        case SoundType::FOOTSTEP_SAND:
        case SoundType::FOOTSTEP_GRAVEL:
        case SoundType::FOOTSTEP_SNOW:
        case SoundType::FOOTSTEP_WATER:
        case SoundType::PLAYER_HURT:
        case SoundType::PLAYER_FALL_BIG:
        case SoundType::PLAYER_FALL_SMALL:
        case SoundType::PLAYER_EAT:
        case SoundType::PLAYER_DRINK:
        case SoundType::PLAYER_BURP:
            return SoundCategory::PLAYER;
            
        case SoundType::DIG_GRASS:
        case SoundType::DIG_STONE:
        case SoundType::DIG_WOOD:
        case SoundType::DIG_SAND:
        case SoundType::DIG_GRAVEL:
        case SoundType::DIG_GLASS:
        case SoundType::DIG_SNOW:
        case SoundType::DIG_CLOTH:
        case SoundType::PLACE_GRASS:
        case SoundType::PLACE_STONE:
        case SoundType::PLACE_WOOD:
        case SoundType::PLACE_SAND:
        case SoundType::PLACE_GRAVEL:
        case SoundType::PLACE_GLASS:
        case SoundType::PLACE_SNOW:
        case SoundType::DOOR_OPEN:
        case SoundType::DOOR_CLOSE:
        case SoundType::CHEST_OPEN:
        case SoundType::CHEST_CLOSE:
            return SoundCategory::BLOCKS;
            
        case SoundType::MOB_PIG_SAY:
        case SoundType::MOB_PIG_DEATH:
        case SoundType::MOB_PIG_STEP:
        case SoundType::MOB_CHICKEN_SAY:
        case SoundType::MOB_CHICKEN_HURT:
        case SoundType::MOB_CHICKEN_PLOP:
        case SoundType::MOB_SHEEP_SAY:
        case SoundType::MOB_SHEEP_STEP:
        case SoundType::MOB_COW_SAY:
        case SoundType::MOB_COW_HURT:
        case SoundType::MOB_SKELETON_SHOOT:
        case SoundType::MOB_WOLF_BARK:
        case SoundType::MOB_WOLF_GROWL:
        case SoundType::MOB_WOLF_HOWL:
        case SoundType::MOB_WOLF_HURT:
        case SoundType::MOB_WOLF_DEATH:
            return SoundCategory::MOBS;
            
        case SoundType::UI_CLICK:
        case SoundType::UI_LEVELUP:
            return SoundCategory::UI;
            
        default:
            return SoundCategory::MASTER;
    }
}

// Helper functions for block sounds
SoundType getStepSoundForBlock(uint8_t blockType) {
    BlockType type = static_cast<BlockType>(blockType);
    switch (type) {
        case BlockType::GRASS:
        case BlockType::LEAVES:
        case BlockType::TALL_GRASS:
        case BlockType::ROSE:
            return SoundType::FOOTSTEP_GRASS;
            
        case BlockType::STONE:
        case BlockType::BEDROCK:
        case BlockType::ICE:
            return SoundType::FOOTSTEP_STONE;
            
        case BlockType::WOOD:
        case BlockType::LOG:
            return SoundType::FOOTSTEP_WOOD;
            
        case BlockType::SAND:
        case BlockType::SANDSTONE:
            return SoundType::FOOTSTEP_SAND;
            
        case BlockType::GRAVEL:
            return SoundType::FOOTSTEP_GRAVEL;
            
        case BlockType::SNOW:
            return SoundType::FOOTSTEP_SNOW;
            
        case BlockType::DIRT:
            return SoundType::FOOTSTEP_GRASS;
            
        default:
            return SoundType::FOOTSTEP_STONE;
    }
}

SoundType getDigSoundForBlock(uint8_t blockType) {
    BlockType type = static_cast<BlockType>(blockType);
    switch (type) {
        case BlockType::GRASS:
        case BlockType::DIRT:
        case BlockType::LEAVES:
        case BlockType::TALL_GRASS:
        case BlockType::ROSE:
            return SoundType::DIG_GRASS;
            
        case BlockType::STONE:
        case BlockType::BEDROCK:
        case BlockType::ICE:
            return SoundType::DIG_STONE;
            
        case BlockType::WOOD:
        case BlockType::LOG:
            return SoundType::DIG_WOOD;
            
        case BlockType::SAND:
        case BlockType::SANDSTONE:
            return SoundType::DIG_SAND;
            
        case BlockType::GRAVEL:
            return SoundType::DIG_GRAVEL;
            
        case BlockType::SNOW:
            return SoundType::DIG_SNOW;
            
        default:
            return SoundType::DIG_STONE;
    }
}

SoundType getPlaceSoundForBlock(uint8_t blockType) {
    // Place sounds are same as dig sounds
    return getDigSoundForBlock(blockType);
}

} // namespace Audio
