#pragma once

#include "OVResourceManager.h"

#include <d3d9.h>

#include <cstdint>
#include <vector>

namespace ov::client
{
struct SpeakerState { std::uint16_t playerId{}; float x{}; float y{}; bool talking{}; float distance{}; };
class OVSpeakerRenderer final
{
public:
    void Submit(SpeakerState speaker);
    void Clear();
    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void SetScale(float scale) noexcept { scale_ = scale; }
    void SetOffset(float x, float y) noexcept { offsetX_ = x; offsetY_ = y; }
    void SetLocalPlayerId(std::uint16_t playerId) noexcept { localPlayerId_ = playerId; }
    void SetViewport(float width, float height) noexcept { viewportWidth_ = width; viewportHeight_ = height; }
    void Render(IDirect3DDevice9* device, IDirect3DTexture9* texture);

private:
    bool enabled_{true};
    float scale_{1.0F};
    float offsetX_{};
    float offsetY_{};
    std::uint16_t localPlayerId_{};
    float viewportWidth_{};
    float viewportHeight_{};
    std::vector<SpeakerState> speakers_;
};
}
