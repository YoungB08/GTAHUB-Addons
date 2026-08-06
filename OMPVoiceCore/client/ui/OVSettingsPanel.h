#pragma once

#include "client/config/OVClientConfig.h"

namespace ov::client
{
enum class SettingsTab { None = -1, General, Microphone, Blacklist, Debug };
class OVAudioEngine;
class OVBassApi;
class OVDiagnostic;
class OVSettingsPanel final
{
public:
    void SetOpen(bool open) noexcept { open_ = open; }
    [[nodiscard]] bool IsOpen() const noexcept { return open_; }
    void SelectTab(SettingsTab tab) noexcept { selectedTab_ = tab; }
    [[nodiscard]] SettingsTab LastRenderedTab() const noexcept { return lastRenderedTab_; }
    void Render(OVClientConfig& config, OVAudioEngine& audio, OVBassApi& bass, OVDiagnostic& diagnostic, bool dx9Hooked);

private:
    bool open_{};
    char blacklistSearch_[64]{};
    bool microphoneTest_{};
    SettingsTab selectedTab_{SettingsTab::None};
    SettingsTab lastRenderedTab_{SettingsTab::None};
};
}
