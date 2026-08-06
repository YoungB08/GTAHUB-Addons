#include "OVSpeakerRenderer.h"

#include <algorithm>

namespace ov::client
{
void OVSpeakerRenderer::Submit(SpeakerState speaker)
{
    if (speaker.playerId == localPlayerId_ || !speaker.talking || speaker.distance > 120.0F || speaker.distance < 0.0F) return;
    speakers_.push_back(speaker);
}
void OVSpeakerRenderer::Clear() { speakers_.clear(); }
void OVSpeakerRenderer::Render(IDirect3DDevice9* device, IDirect3DTexture9* texture)
{
    if (!enabled_ || !device || !texture) return;
    for (const auto& speaker : speakers_)
    {
        const float size = 28.0F * std::clamp(scale_, 0.25F, 3.0F);
        D3DSURFACE_DESC description{};
        texture->GetLevelDesc(0, &description);
        const float aspect = description.Height > 0 ? static_cast<float>(description.Width) / description.Height : 1.0F;
        const float halfWidth = size * aspect;
        const float x = speaker.x + offsetX_;
        const float y = speaker.y + offsetY_;
        if (viewportWidth_ > 0.0F && (x + halfWidth < 0.0F || y + size < 0.0F || x - halfWidth > viewportWidth_ || y - size > viewportHeight_)) continue;
        struct Vertex { float x, y, z, rhw, u, v; } vertices[] = {{x - halfWidth, y - size, 0, 1, 0, 0}, {x + halfWidth, y - size, 0, 1, 1, 0}, {x + halfWidth, y + size, 0, 1, 1, 1}, {x - halfWidth, y + size, 0, 1, 0, 1}};
        device->SetTexture(0, texture);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));
    }
    speakers_.clear();
}
}
