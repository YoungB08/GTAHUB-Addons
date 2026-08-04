#pragma once

#include <d3d9.h>

namespace Nametag {

void RenderAll(IDirect3DDevice9* device);
void OnLostDevice();
void OnResetDevice(IDirect3DDevice9* device);
void Shutdown();

} // namespace Nametag
