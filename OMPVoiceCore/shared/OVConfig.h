#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

namespace ov
{
struct SoundConfig
{
    bool enabled{true};
    int masterVolume{32};
    bool smoothing{true};
    bool highPassFilter{true};
    bool noiseSuppression{true};
    bool automaticGainControl{true};
    bool voiceActivation{};
    float voiceThreshold{0.15F};
};

struct MicrophoneConfig { int device{-1}; float gain{1.0F}; bool muted{}; bool enabled{true}; };
struct IconConfig { bool enabled{true}; float scale{1.0F}; float offsetX{}; float offsetY{}; };
struct DebugConfig { bool loopback{}; bool mirrorMode{}; bool fakeRemote{}; bool showOverlay{true}; float packetLoss{}; int jitterMs{}; int latencyMs{}; };

struct Config
{
    SoundConfig sound;
    MicrophoneConfig microphone;
    IconConfig speakerIcon;
    IconConfig microphoneIcon;
    DebugConfig debug;
    float phoneLeakRadius{8.0F};
    float defaultVoiceDistance{25.0F};
    float globalDistanceValue{-1.0F};
    int maxVoices{8};
    int talkKey{0x5A};
    std::unordered_set<int> blacklist;
};

class ConfigStore final
{
public:
    explicit ConfigStore(std::filesystem::path path);
    bool Load(Config& config, std::string& error) const;
    bool Save(const Config& config, std::string& error) const;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

private:
    static void Validate(Config& config);
    std::filesystem::path path_;
};
}
