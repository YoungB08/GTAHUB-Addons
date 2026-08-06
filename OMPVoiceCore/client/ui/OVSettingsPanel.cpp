#include "OVSettingsPanel.h"

#include "client/audio/OVAudioEngine.h"
#include "client/audio/OVBassApi.h"
#include "client/debug/OVDiagnostic.h"

#include <imgui.h>

#include <algorithm>
#include <string>

namespace ov::client
{
void OVSettingsPanel::Render(OVClientConfig& config, OVAudioEngine& audio, OVBassApi& bass, OVDiagnostic& diagnostic, bool dx9Hooked)
{
    if (!open_) return;
    if (!ImGui::Begin("OMPVoice Settings", &open_, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    auto& values = config.Values();
    if (ImGui::BeginTabBar("voice_tabs"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            ImGui::Checkbox("Turn on sound", &values.sound.enabled);
            int volume = values.sound.masterVolume;
            if (ImGui::SliderInt("Sound volume", &volume, 0, 100)) { values.sound.masterVolume = volume; audio.SetMasterVolume(volume / 100.0F); }
            ImGui::Checkbox("Volume smoothing", &values.sound.smoothing);
            ImGui::Checkbox("High pass filter", &values.sound.highPassFilter); audio.EnableHighPass(values.sound.highPassFilter);
            ImGui::Checkbox("Noise suppression", &values.sound.noiseSuppression); audio.EnableNoiseSuppression(values.sound.noiseSuppression);
            ImGui::Checkbox("Automatic gain control", &values.sound.automaticGainControl); audio.EnableAGC(values.sound.automaticGainControl);
            ImGui::Checkbox("Voice activation", &values.sound.voiceActivation);
            ImGui::SliderFloat("Activation threshold", &values.sound.voiceThreshold, 0.01F, 1.0F);
            ImGui::InputInt("Talk key (VK)", &values.talkKey);
            values.talkKey = std::clamp(values.talkKey, 1, 255);
            ImGui::Checkbox("Speaker icons", &values.speakerIcon.enabled);
            ImGui::SliderFloat("Speaker icon scale", &values.speakerIcon.scale, 0.25F, 3.0F);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Microphone"))
        {
            ImGui::Checkbox("Enable microphone", &values.microphone.enabled);
            const auto devices = bass.RecordDevices();
            const char* currentDevice = values.microphone.device >= 0 && static_cast<std::size_t>(values.microphone.device) < devices.size() ? devices[values.microphone.device].c_str() : "Default device";
            if (ImGui::BeginCombo("Input device", currentDevice))
            {
                if (ImGui::Selectable("Default device", values.microphone.device == -1)) { values.microphone.device = -1; audio.SetInputDevice(-1); }
                for (std::size_t index = 0; index < devices.size(); ++index)
                {
                    if (ImGui::Selectable(devices[index].c_str(), values.microphone.device == static_cast<int>(index))) { values.microphone.device = static_cast<int>(index); audio.SetInputDevice(static_cast<int>(index)); }
                }
                ImGui::EndCombo();
            }
            ImGui::SliderFloat("Microphone gain", &values.microphone.gain, 0.0F, 2.0F);
            ImGui::Checkbox("Mute microphone", &values.microphone.muted);
            ImGui::Checkbox("Test microphone", &microphoneTest_);
            if (microphoneTest_) audio.SetTransmitting(true);
            ImGui::ProgressBar(audio.MicRms(), ImVec2(260, 0), "Live level");
            ImGui::ProgressBar(audio.MicPeak(), ImVec2(260, 0), "Peak");
            ImGui::SliderFloat("HUD scale", &values.microphoneIcon.scale, 0.25F, 3.0F);
            ImGui::SliderFloat("HUD X offset", &values.microphoneIcon.offsetX, -500.0F, 500.0F);
            ImGui::SliderFloat("HUD Y offset", &values.microphoneIcon.offsetY, -300.0F, 300.0F);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Black list"))
        {
            ImGui::InputText("Search player", blacklistSearch_, sizeof(blacklistSearch_));
            for (int playerId = 0; playerId < 1000; ++playerId)
            {
                const bool muted = values.blacklist.count(playerId) != 0;
                if (ImGui::Button((std::to_string(playerId) + (muted ? "  Unmute" : "  Mute")).c_str()))
                {
                    if (muted) values.blacklist.erase(playerId); else values.blacklist.insert(playerId);
                }
                if (playerId > 32) break;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug"))
        {
            ImGui::Checkbox("Self loopback", &values.debug.loopback);
            ImGui::Checkbox("Voice mirror mode", &values.debug.mirrorMode);
            ImGui::Checkbox("Fake remote player", &values.debug.fakeRemote);
            ImGui::Checkbox("Show debug overlay", &values.debug.showOverlay);
            ImGui::SliderFloat("Packet loss", &values.debug.packetLoss, 0.0F, 50.0F);
            ImGui::SliderInt("Jitter (ms)", &values.debug.jitterMs, 0, 200);
            ImGui::SliderInt("Latency (ms)", &values.debug.latencyMs, 0, 300);
            if (ImGui::Button("Run diagnostic")) { std::string output; diagnostic.Run(bass, audio, dx9Hooked, output); }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (ImGui::Button("Save JSON")) config.Save();
    ImGui::SameLine();
    if (ImGui::Button("Reload JSON")) config.Load();
    ImGui::End();
}
}
