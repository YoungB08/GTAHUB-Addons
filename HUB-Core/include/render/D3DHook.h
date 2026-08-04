#pragma once

#include <d3d9.h>

namespace D3DHook {

void Install(IDirect3DDevice9* device);
void Uninstall();
bool IsInstalled();

} // namespace D3DHook
