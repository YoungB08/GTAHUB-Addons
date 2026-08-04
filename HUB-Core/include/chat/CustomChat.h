#pragma once

#include <d3d9.h>

namespace HUB::Chat::CustomChat {

void Render(IDirect3DDevice9* device);
void OnLostDevice();
void OnResetDevice(IDirect3DDevice9* device);
void Shutdown();

bool IsReady();
void OpenInput();
void CloseInput();
void SubmitInput();
void RequestCloseInput();
void Scroll(short wheelDelta);

} // namespace HUB::Chat::CustomChat
