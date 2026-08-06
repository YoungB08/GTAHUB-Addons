#pragma once

#include "OVDebugOverlay.h"
#include "OVHudIcon.h"
#include "OVResourceManager.h"
#include "OVSpeakerRenderer.h"

#include <d3d9.h>

#include <filesystem>
#include <memory>

namespace ov::client
{
class OVClientConfig;
class OVAudioEngine;
class OVBassApi;
class OVDiagnostic;
class OVSettingsPanel;
class OVFakeRemote;

class OVDx9Renderer final
{
public:
    OVDx9Renderer(OVClientConfig& config, OVAudioEngine& audio, OVBassApi& bass, OVDiagnostic& diagnostic);
    ~OVDx9Renderer();
    bool Initialize(IDirect3DDevice9* device);
    void Shutdown();
    void OnEndScene(IDirect3DDevice9* device);
    void OnResetBefore();
    void OnResetAfter(IDirect3DDevice9* device);
    void ToggleSettings() noexcept;
    void SetOverlayVisible(bool visible) noexcept;
    void SetFakeRemote(bool enabled) noexcept { fakeRemote_ = enabled; }
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

private:
    OVClientConfig& config_;
    OVAudioEngine& audio_;
    OVBassApi& bass_;
    OVDiagnostic& diagnostic_;
    OVResourceManager resources_;
    OVHudIcon hud_;
    OVSpeakerRenderer speakers_;
    OVDebugOverlay overlay_;
    std::unique_ptr<OVSettingsPanel> settings_;
    IDirect3DDevice9* device_{};
    bool initialized_{};
    bool settingsOpen_{};
    bool fakeRemote_{};
};
}
