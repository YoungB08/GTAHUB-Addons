#include "OVDx9Renderer.h"

#include "client/audio/OVAudioEngine.h"
#include "client/config/OVClientConfig.h"
#include "client/debug/OVDiagnostic.h"
#include "client/ui/OVSettingsPanel.h"

#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

#include <Windows.h>

#include <cmath>

namespace ov::client
{
OVDx9Renderer::OVDx9Renderer(OVClientConfig& config, OVAudioEngine& audio, OVBassApi& bass, OVDiagnostic& diagnostic)
    : config_(config), audio_(audio), bass_(bass), diagnostic_(diagnostic), settings_(std::make_unique<OVSettingsPanel>())
{
}
OVDx9Renderer::~OVDx9Renderer() { Shutdown(); }
bool OVDx9Renderer::Initialize(IDirect3DDevice9* device)
{
    if (initialized_ && device_ == device) return true;
    device_ = device;
    if (!device_) return false;
    if (!ImGui::GetCurrentContext()) ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(GetForegroundWindow());
    ImGui_ImplDX9_Init(device_);
    resources_.Initialize(device_, std::filesystem::path("ompvoice") / "resources");
    resources_.ReloadTextures();
    hud_.SetScale(config_.Values().microphoneIcon.scale);
    hud_.SetOffset(config_.Values().microphoneIcon.offsetX, config_.Values().microphoneIcon.offsetY);
    initialized_ = true;
    return true;
}
void OVDx9Renderer::Shutdown()
{
    if (!initialized_) return;
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    resources_.ReleaseAllTextures();
    initialized_ = false;
    device_ = nullptr;
}
void OVDx9Renderer::OnResetBefore()
{
    if (!initialized_) return;
    resources_.ReleaseAllTextures();
    ImGui_ImplDX9_InvalidateDeviceObjects();
}
void OVDx9Renderer::OnResetAfter(IDirect3DDevice9* device)
{
    device_ = device;
    if (!initialized_) return;
    ImGui_ImplDX9_CreateDeviceObjects();
    resources_.ReloadTextures();
}
void OVDx9Renderer::ToggleSettings() noexcept { settingsOpen_ = !settingsOpen_; settings_->SetOpen(settingsOpen_); }
void OVDx9Renderer::SetOverlayVisible(bool visible) noexcept { overlay_.SetVisible(visible); }
void OVDx9Renderer::OnEndScene(IDirect3DDevice9* device)
{
    if (!Initialize(device)) return;
    D3DVIEWPORT9 viewport{}; device_->GetViewport(&viewport);
    const bool transmitting = audio_.IsTransmitting();
    speakers_.SetEnabled(config_.Values().speakerIcon.enabled);
    speakers_.SetScale(config_.Values().speakerIcon.scale);
    speakers_.SetOffset(config_.Values().speakerIcon.offsetX, config_.Values().speakerIcon.offsetY);
    if (fakeRemote_) speakers_.Submit(SpeakerState{999, static_cast<float>(viewport.Width) * 0.5F, static_cast<float>(viewport.Height) * 0.42F, true, 5.0F});
    const float pulse = transmitting ? (std::sin(static_cast<float>(GetTickCount64() % 800U) * 0.00785398F) * 0.5F + 0.5F) : 0.0F;
    const char* microphoneTexture = config_.Values().microphone.muted ? "micro_muted.png" : (transmitting ? "micro_active.png" : "micro_passive.png");
    hud_.Render(device_, resources_.Get(microphoneTexture), static_cast<float>(viewport.Width), static_cast<float>(viewport.Height), transmitting, config_.Values().microphone.muted, pulse);
    speakers_.Render(device_, resources_.Get("speaker.png"));
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    settings_->Render(config_, audio_, bass_, diagnostic_, true);
    overlay_.SetStats(DebugStats{transmitting, audio_.MicRms(), config_.Values().microphone.gain, 0, 0.0F, 0, 0, audio_.RemoteStreamCount(), false, fakeRemote_, config_.Values().debug.mirrorMode, 0});
    overlay_.AddRms(audio_.MicRms());
    overlay_.SetWaveform(audio_.MicWaveform(), audio_.MicPeak());
    overlay_.Render();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
}
