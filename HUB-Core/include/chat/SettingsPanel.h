#pragma once

#include <d3d9.h>
#include <windows.h>
#include <string>

namespace HUB::Chat::SettingsPanel {

void Initialize();
void Open();
void Close();
void Toggle();
bool IsOpen();

void Render(IDirect3DDevice9* device);
bool HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

} // namespace HUB::Chat::SettingsPanel
