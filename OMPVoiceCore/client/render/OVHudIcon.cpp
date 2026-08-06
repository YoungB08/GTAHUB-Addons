#include "OVHudIcon.h"

#include <algorithm>

namespace ov::client
{
namespace
{
struct Vertex { float x, y, z, rhw, u, v; };
constexpr DWORD FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;
}
void OVHudIcon::Render(IDirect3DDevice9* device, IDirect3DTexture9* texture, float width, float height, bool talking, bool muted, float pulse)
{
    if (!visible_ || !device || !texture) return;
    const float size = 64.0F * std::clamp(scale_, 0.25F, 3.0F) * (talking ? 1.0F + pulse * 0.08F : 1.0F);
    const float x = (width - size) * 0.5F + offsetX_;
    const float y = (height - 140.0F) + offsetY_;
    Vertex vertices[] = {{x - 0.5F, y - 0.5F, 0, 1, 0, 0}, {x + size - 0.5F, y - 0.5F, 0, 1, 1, 0}, {x + size - 0.5F, y + size - 0.5F, 0, 1, 1, 1}, {x - 0.5F, y + size - 0.5F, 0, 1, 0, 1}};
    IDirect3DStateBlock9* state = nullptr;
    if (SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state))) state->Capture();
    device->SetTexture(0, texture);
    device->SetFVF(FVF);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));
    if (state) { state->Apply(); state->Release(); }
    (void)muted;
}
}
