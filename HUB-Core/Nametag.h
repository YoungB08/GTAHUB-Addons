/**
 * @file Nametag.h
 * @brief Hệ thống render nametag tùy biến cho SAMP (declarations).
 *
 * Kiến trúc render (từ trên xuống dưới):
 *
 *   ┌─────────────┬──────┬─────────────┐
 *   │  ADMIN      │ ICON │     VIP     │  ← Hàng 1: Badges
 *   └─────────────┴──────┴─────────────┘
 *          PlayerName (cyan + stroke)      ← Hàng 2: Tên
 *   ┌──────────────┐ ┌──────────────┐
 *   │ ████░░ HP   │ │ ████░░ Armour│     ← Hàng 3: Progress bars
 *   └──────────────┘ └──────────────┘
 *   ┌───────────────────────────────┐
 *   │     ID: 42   |   12ms        │     ← Hàng 4: Capsule info
 *   └───────────────────────────────┘
 *
 * Tọa độ vị trí (centerX, topY) được tính bởi W2S từ đầu player.
 * Gọi Nametag::RenderAll(device) mỗi frame từ EndScene hook.
 */
#pragma once
#include <d3d9.h>
#include <d3dx9.h>

namespace Nametag {

/// Khởi tạo font và sprite. Gọi một lần khi D3D device sẵn sàng.
void Init(IDirect3DDevice9* dev);

/// Giải phóng resource. Gọi khi DLL unload hoặc device lost.
void Release();

/// Vẽ toàn bộ nametag cho tất cả player trong frame hiện tại.
void RenderAll(IDirect3DDevice9* dev);

} // namespace Nametag
