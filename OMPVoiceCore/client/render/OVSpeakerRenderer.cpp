#include "OVSpeakerRenderer.h"

#include <algorithm>

namespace ov::client
{
void OVSpeakerRenderer::Submit(SpeakerState speaker) { if (speaker.talking && speaker.distance <= 120.0F) speakers_.push_back(speaker); }
void OVSpeakerRenderer::Clear() { speakers_.clear(); }
void OVSpeakerRenderer::Render(IDirect3DDevice9* device, IDirect3DTexture9* texture)
{
    if (!enabled_ || !device || !texture) return;
    for (const auto& speaker : speakers_)
    {
        const float size = 28.0F * std::clamp(scale_, 0.25F, 3.0F);
        struct Vertex { float x, y, z, rhw, u, v; } vertices[] = {{speaker.x - size, speaker.y - size, 0, 1, 0, 0}, {speaker.x + size, speaker.y - size, 0, 1, 1, 0}, {speaker.x + size, speaker.y + size, 0, 1, 1, 1}, {speaker.x - size, speaker.y + size, 0, 1, 0, 1}};
        device->SetTexture(0, texture);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));
    }
    speakers_.clear();
}
}
