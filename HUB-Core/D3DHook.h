/**
 * @file D3DHook.h
 * @brief Hook IDirect3DDevice9::EndScene bằng VMT patching.
 *
 * Cơ chế:
 *  1. Lấy device từ RefPlayerTags()->m_pDevice sau khi SAMP khởi động.
 *  2. Đọc VMT[42] (EndScene) → lưu lại original pointer.
 *  3. Ghi VMT[42] = &hkEndScene bằng VirtualProtect.
 *  4. Trong hkEndScene: lazy-init fonts, render nametags, gọi original.
 *
 * Tại sao VMT index 42?
 *  D3D9 COM interface layout — EndScene là method thứ 42 (0-indexed).
 *  Cố định theo D3D9 spec, không phụ thuộc driver hay version.
 *
 * Save/restore render state:
 *  Dùng IDirect3DStateBlock9 để capture TOÀN BỘ state trước khi vẽ
 *  và restore sau. Đảm bảo không ảnh hưởng đến rendering của SAMP/GTA.
 */
#pragma once
#include <d3d9.h>

namespace D3DHook {

/// Cài hook EndScene vào device. Chỉ gọi một lần.
void Install(IDirect3DDevice9* dev);

/// Gỡ hook, restore VMT về original. Gọi khi DLL unload.
void Uninstall();

/// Trả true nếu hook đã được cài.
bool IsInstalled();

} // namespace D3DHook
