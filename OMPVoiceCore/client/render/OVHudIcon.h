#pragma once

#include <d3d9.h>

#include <string>

namespace ov::client
{
class OVHudIcon final
{
public:
    void SetVisible(bool visible) noexcept { visible_ = visible; }
    void SetScale(float scale) noexcept { scale_ = scale; }
    void SetOffset(float x, float y) noexcept { offsetX_ = x; offsetY_ = y; }
    void Render(IDirect3DDevice9* device, IDirect3DTexture9* texture, float width, float height, bool talking, bool muted, float pulse);

private:
    bool visible_{true};
    float scale_{1.0F};
    float offsetX_{};
    float offsetY_{};
};
}
