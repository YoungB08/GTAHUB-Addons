#pragma once

#include <d3d9.h>
#include <string>

namespace HUB::Chat::TextRenderer {

bool Initialize(IDirect3DDevice9* device, int fontHeight);
float MeasureWidth(const std::wstring& text);
bool Draw(const std::wstring& text, float x, float y, float right, float height,
    D3DCOLOR color, DWORD format);
void OnLostDevice();
void Shutdown();

} // namespace HUB::Chat::TextRenderer
