#include "OVDebugOverlay.h"

#include <imgui.h>

namespace ov::client
{
void OVDebugOverlay::AddRms(float rms)
{
    rmsHistory_.push_back(rms);
    if (rmsHistory_.size() > 128) rmsHistory_.erase(rmsHistory_.begin());
}
void OVDebugOverlay::SetWaveform(std::vector<float> waveform, float peak)
{
    waveform_ = std::move(waveform);
    micPeak_ = peak;
}
void OVDebugOverlay::Render()
{
    if (!visible_) return;
    ImGui::SetNextWindowBgAlpha(0.78F);
    if (!ImGui::Begin("OMPVoice Debug", &visible_, ImGuiWindowFlags_AlwaysAutoResize)) { ImGui::End(); return; }
    ImGui::Text("Voice State      : %s", stats_.transmitting ? "TRANSMITTING" : "IDLE");
    ImGui::Text("Mic RMS          : %.3f", stats_.micRms);
    ImGui::Text("Mic Gain         : %.2f", stats_.micGain);
    ImGui::Text("Encoded Size     : %zu bytes", stats_.encodedSize);
    ImGui::Text("Bitrate          : %.1f kbps", stats_.bitrateKbps);
    ImGui::Text("Jitter Buffer    : %zu packets", stats_.jitterPackets);
    ImGui::Text("Packet Loss      : %.1f%%", stats_.packetLoss);
    ImGui::Text("Remote Streams   : %zu", stats_.remoteStreams);
    ImGui::Text("Loopback         : %s", stats_.loopback ? "ON" : "OFF");
    ImGui::Text("Fake Remote      : %s", stats_.fakeRemote ? "ON" : "OFF");
    ImGui::Text("Mirror Mode      : %s", stats_.mirrorMode ? "ON" : "OFF");
    ImGui::Text("RTT              : %u ms", stats_.rttMs);
    if (!rmsHistory_.empty()) ImGui::PlotLines("Mic RMS", rmsHistory_.data(), static_cast<int>(rmsHistory_.size()), 0, nullptr, 0.0F, 1.0F, ImVec2(260, 70));
    if (!waveform_.empty()) ImGui::PlotLines("Waveform", waveform_.data(), static_cast<int>(waveform_.size()), 0, nullptr, 0.0F, 1.0F, ImVec2(260, 70));
    ImGui::ProgressBar(micPeak_, ImVec2(260, 0), "Peak");
    ImGui::End();
}
}
