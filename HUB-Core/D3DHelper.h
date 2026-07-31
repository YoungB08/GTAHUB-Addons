/**
 * @file D3DHelper.h
 * @brief Primitive 2D drawing utilities cho D3D9 (header-only).
 *
 * Tất cả hàm nhận IDirect3DDevice9* và vẽ trực tiếp bằng
 * DrawPrimitiveUP với FVF XYZRHW|DIFFUSE (pre-transformed 2D).
 *
 * Caller phải set đúng render state (alpha blend, z-disable, v.v.)
 * trước khi dùng — xem D3DHook.cpp::SetupRenderState().
 */
#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <cstring>

namespace D3DHelper {

// ---------------------------------------------------------------------------
// Vertex format
// ---------------------------------------------------------------------------

struct Vertex2D {
    float    x, y, z, rhw; ///< Tọa độ màn hình (rhw = 1.0)
    D3DCOLOR color;
};
constexpr DWORD kFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

/**
 * @brief Vẽ hình chữ nhật tô màu (solid fill).
 * @param x,y   Góc trên-trái (pixel)
 * @param w,h   Kích thước (pixel)
 * @param color D3DCOLOR_ARGB(a,r,g,b)
 */
inline void DrawFilledRect(IDirect3DDevice9* dev,
    float x, float y, float w, float h, D3DCOLOR color)
{
    Vertex2D v[4] = {
        { x,     y + h, 0.f, 1.f, color },
        { x,     y,     0.f, 1.f, color },
        { x + w, y + h, 0.f, 1.f, color },
        { x + w, y,     0.f, 1.f, color },
    };
    dev->SetFVF(kFVF);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex2D));
}

/**
 * @brief Vẽ khung viền (border only, 4 cạnh).
 * @param t Độ dày viền (pixel)
 */
inline void DrawBorderRect(IDirect3DDevice9* dev,
    float x, float y, float w, float h, float t, D3DCOLOR color)
{
    DrawFilledRect(dev, x,         y,         w, t, color); // top
    DrawFilledRect(dev, x,         y + h - t, w, t, color); // bottom
    DrawFilledRect(dev, x,         y,         t, h, color); // left
    DrawFilledRect(dev, x + w - t, y,         t, h, color); // right
}

/**
 * @brief Vẽ thanh progress bar với nền và fill màu riêng.
 * @param percent Phần trăm [0.0 – 100.0]
 * @param bgColor Màu nền
 * @param fillColor Màu fill
 */
inline void DrawProgressBar(IDirect3DDevice9* dev,
    float x, float y, float w, float h,
    float percent, D3DCOLOR bgColor, D3DCOLOR fillColor)
{
    const float fill = (w - 2.f) * (max(0.f, min(100.f, percent)) / 100.f);
    DrawFilledRect(dev, x,     y,     w,    h,     bgColor);
    DrawFilledRect(dev, x + 1, y + 1, fill, h - 2, fillColor);
    DrawBorderRect(dev, x,     y,     w,    h, 1.f, D3DCOLOR_ARGB(255, 0, 0, 0));
}

/**
 * @brief Vẽ text với stroke đen 8 hướng (kiểu SA:MP).
 * @param fmt      DT_* flags (DT_LEFT | DT_NOCLIP, ...)
 * @param textColor  Màu chữ
 * @param strokeColor Màu viền (thường là đen)
 */
inline void DrawTextStroke(ID3DXFont* font, const char* text,
    RECT rect, DWORD fmt, D3DCOLOR textColor, D3DCOLOR strokeColor)
{
    constexpr int kOffsets[8][2] = {
        {-1,-1}, {0,-1}, {1,-1},
        {-1, 0},          {1, 0},
        {-1, 1}, {0, 1}, {1, 1},
    };
    for (auto& ofs : kOffsets) {
        RECT r = { rect.left + ofs[0], rect.top + ofs[1],
                   rect.right + ofs[0], rect.bottom + ofs[1] };
        font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    }
    font->DrawTextA(NULL, text, -1, &rect, fmt, textColor);
}

/**
 * @brief Đo kích thước text (không vẽ).
 * @return SIZE {cx=width, cy=height} tính bằng pixel.
 */
inline SIZE MeasureText(ID3DXFont* font, const char* text) {
    RECT tmp = { 0, 0, 1000, 100 };
    font->DrawTextA(NULL, text, -1, &tmp, DT_CALCRECT | DT_LEFT, 0);
    return SIZE{ tmp.right - tmp.left, tmp.bottom - tmp.top };
}

} // namespace D3DHelper
