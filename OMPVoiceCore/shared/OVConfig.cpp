#include "OVConfig.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace ov
{
using nlohmann::json;

ConfigStore::ConfigStore(std::filesystem::path path) : path_(std::move(path)) {}

void ConfigStore::Validate(Config& config)
{
    config.sound.masterVolume = std::clamp(config.sound.masterVolume, 0, 100);
    config.sound.voiceThreshold = std::clamp(config.sound.voiceThreshold, 0.01F, 1.0F);
    config.microphone.gain = std::clamp(config.microphone.gain, 0.0F, 2.0F);
    config.speakerIcon.scale = std::clamp(config.speakerIcon.scale, 0.25F, 3.0F);
    config.microphoneIcon.scale = std::clamp(config.microphoneIcon.scale, 0.25F, 3.0F);
    config.phoneLeakRadius = std::clamp(config.phoneLeakRadius, 5.0F, 10.0F);
    config.defaultVoiceDistance = std::clamp(config.defaultVoiceDistance, 1.0F, 200.0F);
    config.globalDistanceValue = -1.0F;
    config.maxVoices = std::clamp(config.maxVoices, 1, 8);
    config.debug.packetLoss = std::clamp(config.debug.packetLoss, 0.0F, 50.0F);
    config.debug.jitterMs = std::clamp(config.debug.jitterMs, 0, 200);
    config.debug.latencyMs = std::clamp(config.debug.latencyMs, 0, 300);
}

bool ConfigStore::Load(Config& config, std::string& error) const
{
    if (!std::filesystem::exists(path_)) return Save(config, error);
    try
    {
        std::ifstream input(path_);
        if (!input) { error = "cannot open config for reading"; return false; }
        const json root = json::parse(input);
        const auto& sound = root.value("sound", json::object());
        config.sound.enabled = sound.value("enabled", config.sound.enabled);
        config.sound.masterVolume = sound.value("masterVolume", config.sound.masterVolume);
        config.sound.smoothing = sound.value("smoothing", config.sound.smoothing);
        config.sound.highPassFilter = sound.value("highPassFilter", config.sound.highPassFilter);
        config.sound.noiseSuppression = sound.value("noiseSuppression", config.sound.noiseSuppression);
        config.sound.automaticGainControl = sound.value("automaticGainControl", config.sound.automaticGainControl);
        config.sound.voiceActivation = sound.value("voiceActivation", config.sound.voiceActivation);
        config.sound.voiceThreshold = sound.value("voiceThreshold", config.sound.voiceThreshold);
        const auto& mic = root.value("microphone", json::object());
        config.microphone.device = mic.value("device", config.microphone.device);
        config.microphone.gain = mic.value("gain", config.microphone.gain);
        config.microphone.muted = mic.value("muted", config.microphone.muted);
        config.microphone.enabled = mic.value("enabled", config.microphone.enabled);
        const auto& speaker = root.value("speakerIcon", json::object());
        config.speakerIcon.enabled = speaker.value("enabled", config.speakerIcon.enabled);
        config.speakerIcon.scale = speaker.value("scale", config.speakerIcon.scale);
        config.speakerIcon.offsetX = speaker.value("offsetX", config.speakerIcon.offsetX);
        config.speakerIcon.offsetY = speaker.value("offsetY", config.speakerIcon.offsetY);
        const auto& hud = root.value("microphoneHud", json::object());
        config.microphoneIcon.enabled = hud.value("enabled", config.microphoneIcon.enabled);
        config.microphoneIcon.scale = hud.value("scale", config.microphoneIcon.scale);
        config.microphoneIcon.offsetX = hud.value("offsetX", config.microphoneIcon.offsetX);
        config.microphoneIcon.offsetY = hud.value("offsetY", config.microphoneIcon.offsetY);
        const auto& debug = root.value("debug", json::object());
        config.debug.loopback = debug.value("loopback", config.debug.loopback);
        config.debug.mirrorMode = debug.value("mirrorMode", config.debug.mirrorMode);
        config.debug.fakeRemote = debug.value("fakeRemote", config.debug.fakeRemote);
        config.debug.showOverlay = debug.value("showOverlay", config.debug.showOverlay);
        config.debug.packetLoss = debug.value("packetLoss", config.debug.packetLoss);
        config.debug.jitterMs = debug.value("jitterMs", config.debug.jitterMs);
        config.debug.latencyMs = debug.value("latencyMs", config.debug.latencyMs);
        config.phoneLeakRadius = root.value("phone", json::object()).value("leakRadius", config.phoneLeakRadius);
        config.defaultVoiceDistance = root.value("defaultVoiceDistance", config.defaultVoiceDistance);
        config.maxVoices = root.value("maxVoices", config.maxVoices);
        config.talkKey = root.value("talkKey", config.talkKey);
        config.blacklist.clear();
        for (const int id : root.value("blacklist", std::vector<int>{})) config.blacklist.insert(id);
        Validate(config);
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        Config defaults;
        config = defaults;
        return false;
    }
}

bool ConfigStore::Save(const Config& source, std::string& error) const
{
    Config config = source;
    Validate(config);
    try
    {
        std::filesystem::create_directories(path_.parent_path());
        json root = {
            {"sound", {{"enabled", config.sound.enabled}, {"masterVolume", config.sound.masterVolume}, {"smoothing", config.sound.smoothing}, {"highPassFilter", config.sound.highPassFilter}, {"noiseSuppression", config.sound.noiseSuppression}, {"automaticGainControl", config.sound.automaticGainControl}, {"voiceActivation", config.sound.voiceActivation}, {"voiceThreshold", config.sound.voiceThreshold}}},
            {"microphone", {{"device", config.microphone.device}, {"gain", config.microphone.gain}, {"muted", config.microphone.muted}, {"enabled", config.microphone.enabled}}},
            {"speakerIcon", {{"enabled", config.speakerIcon.enabled}, {"scale", config.speakerIcon.scale}, {"offsetX", config.speakerIcon.offsetX}, {"offsetY", config.speakerIcon.offsetY}}},
            {"microphoneHud", {{"enabled", config.microphoneIcon.enabled}, {"scale", config.microphoneIcon.scale}, {"offsetX", config.microphoneIcon.offsetX}, {"offsetY", config.microphoneIcon.offsetY}}},
            {"phone", {{"leakRadius", config.phoneLeakRadius}}},
            {"debug", {{"loopback", config.debug.loopback}, {"mirrorMode", config.debug.mirrorMode}, {"fakeRemote", config.debug.fakeRemote}, {"showOverlay", config.debug.showOverlay}, {"packetLoss", config.debug.packetLoss}, {"jitterMs", config.debug.jitterMs}, {"latencyMs", config.debug.latencyMs}}},
            {"defaultVoiceDistance", config.defaultVoiceDistance}, {"globalDistanceValue", -1.0F}, {"maxVoices", config.maxVoices}, {"talkKey", config.talkKey},
            {"blacklist", std::vector<int>(config.blacklist.begin(), config.blacklist.end())}};
        const auto temporary = path_.string() + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) { error = "cannot open temporary config"; return false; }
        output << root.dump(2) << '\n';
        output.close();
        std::error_code filesystemError;
        if (std::filesystem::exists(path_))
        {
            const auto backup = path_.string() + ".bak";
            std::filesystem::copy_file(path_, backup, std::filesystem::copy_options::overwrite_existing, filesystemError);
        }
        filesystemError.clear();
        std::filesystem::rename(temporary, path_, filesystemError);
        if (filesystemError)
        {
            std::filesystem::remove(path_, filesystemError);
            filesystemError.clear();
            std::filesystem::rename(temporary, path_, filesystemError);
        }
        if (filesystemError) { error = filesystemError.message(); return false; }
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}
}
