/**
 * @file Nametag.h
 * @brief H? th?ng render nametag tùy bi?n cho SAMP (declarations).
 *
 * Ki?n trúc render (t? trên xu?ng du?i):
 *
 *   +----------------------------------+
 *   ¦  ADMIN      ¦ ICON ¦     VIP     ¦  ? Hàng 1: Badges
 *   +----------------------------------+
 *          PlayerName (cyan + stroke)      ? Hàng 2: Tên
 *   +--------------+ +--------------+
 *   ¦ ¦¦¦¦¦¦ HP   ¦ ¦ ¦¦¦¦¦¦ Armour¦     ? Hàng 3: Progress bars
 *   +--------------+ +--------------+
 *   +-------------------------------+
 *   ¦     ID: 42   |   12ms        ¦     ? Hàng 4: Capsule info
 *   +-------------------------------+
 *
 * T?a d? v? trí (centerX, topY) du?c tính b?i W2S t? d?u player.
 * G?i Nametag::RenderAll(device) m?i frame t? EndScene hook.
 */
#pragma once
#include <d3d9.h>

namespace Nametag {

/// Kh?i t?o font và sprite. G?i m?t l?n khi D3D device s?n sàng.
void Init(IDirect3DDevice9* dev);

/// Gi?i phóng resource. G?i khi DLL unload ho?c device lost.
void Release();

/// V? toàn b? nametag cho t?t c? player trong frame hi?n t?i.
void RenderAll(IDirect3DDevice9* dev);

} // namespace Nametag
