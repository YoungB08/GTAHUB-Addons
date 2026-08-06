#include "OVDx9Renderer.h"

#include "client/audio/OVAudioEngine.h"
#include "client/config/OVClientConfig.h"
#include "client/debug/OVDiagnostic.h"
#include "client/ui/OVSettingsPanel.h"

#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

#include <Windows.h>

#include <algorithm>
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
    D3DDEVICE_CREATION_PARAMETERS creation{};
    device_->GetCreationParameters(&creation);
    ImGui_ImplWin32_Init(creation.hFocusWindow ? creation.hFocusWindow : GetForegroundWindow());
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
void OVDx9Renderer::SelectSettingsTab(SettingsTab tab) noexcept { settings_->SelectTab(tab); }
SettingsTab OVDx9Renderer::LastRenderedSettingsTab() const noexcept { return settings_->LastRenderedTab(); }
void OVDx9Renderer::OnEndScene(IDirect3DDevice9* device)
{
    if (!Initialize(device)) return;
    D3DVIEWPORT9 viewport{}; device_->GetViewport(&viewport);
    const bool transmitting = audio_.IsTransmitting();
    speakers_.SetLocalPlayerId(0);
    speakers_.SetViewport(static_cast<float>(viewport.Width), static_cast<float>(viewport.Height));
    auto& microphoneIcon = config_.Values().microphoneIcon;
    const char* microphoneTexture = config_.Values().microphone.muted ? "micro_muted.png" : (transmitting ? "micro_active.png" : "micro_passive.png");
    IDirect3DTexture9* microphoneTextureHandle = resources_.Get(microphoneTexture);
    if (settingsOpen_)
    {
        POINT cursor{};
        HWND window = GetForegroundWindow();
        if (window && GetCursorPos(&cursor) && ScreenToClient(window, &cursor))
        {
            const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            float aspect = 1.0F;
            D3DSURFACE_DESC description{};
            if (microphoneTextureHandle && SUCCEEDED(microphoneTextureHandle->GetLevelDesc(0, &description)) && description.Height > 0)
                aspect = static_cast<float>(description.Width) / description.Height;
            const float iconHeight = 64.0F * std::clamp(microphoneIcon.scale, 0.25F, 3.0F);
            const float iconWidth = std::min(static_cast<float>(viewport.Width) * 0.8F, iconHeight * aspect);
            const float x = (static_cast<float>(viewport.Width) - iconWidth) * 0.5F + microphoneIcon.offsetX;
            const float y = (static_cast<float>(viewport.Height) - 140.0F) + microphoneIcon.offsetY;
            if (pressed && !draggingHud_ && cursor.x >= x && cursor.x <= x + iconWidth && cursor.y >= y && cursor.y <= y + iconHeight)
            {
                draggingHud_ = true;
                dragStartX_ = cursor.x;
                dragStartY_ = cursor.y;
                dragOffsetX_ = microphoneIcon.offsetX;
                dragOffsetY_ = microphoneIcon.offsetY;
            }
            if (pressed && draggingHud_)
            {
                microphoneIcon.offsetX = dragOffsetX_ + static_cast<float>(cursor.x - dragStartX_);
                microphoneIcon.offsetY = dragOffsetY_ + static_cast<float>(cursor.y - dragStartY_);
                config_.MarkDirty();
            }
            if (!pressed) draggingHud_ = false;
        }
    }
    else draggingHud_ = false;
    hud_.SetVisible(microphoneIcon.enabled);
    hud_.SetScale(microphoneIcon.scale);
    hud_.SetOffset(microphoneIcon.offsetX, microphoneIcon.offsetY);
    speakers_.SetEnabled(config_.Values().speakerIcon.enabled);
    speakers_.SetScale(config_.Values().speakerIcon.scale);
    speakers_.SetOffset(config_.Values().speakerIcon.offsetX, config_.Values().speakerIcon.offsetY);
    if (fakeRemote_) speakers_.Submit(SpeakerState{999, static_cast<float>(viewport.Width) * 0.5F, static_cast<float>(viewport.Height) * 0.42F, true, 5.0F});
    const float pulse = transmitting ? (std::sin(static_cast<float>(GetTickCount64() % 800U) * 0.00785398F) * 0.5F + 0.5F) : 0.0F;
    hud_.Render(device_, microphoneTextureHandle, static_cast<float>(viewport.Width), static_cast<float>(viewport.Height), transmitting, config_.Values().microphone.muted, pulse);
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
