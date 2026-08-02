/**
 * @file D3DHelper.h
 * @brief Primitive 2D drawing utilities cho D3D9 (header-only).
 *
 * Hỗ trợ vẽ hình chữ nhật, viền, progress bar và các góc bo tròn (Rounded Corners).
 */
#pragma once
#include <d3d9.h>
#include <d3dx9.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <string>

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
    dev->SetTexture(0, NULL);
    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    dev->SetFVF(kFVF);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(Vertex2D));

    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

/**
 * @brief Vẽ hình chữ nhật bo tròn góc (solid fill).
 */
inline void DrawRoundedFilledRect(IDirect3DDevice9* dev,
    float x, float y, float w, float h, float r, D3DCOLOR color)
{
    if (r <= 0.5f) {
        DrawFilledRect(dev, x, y, w, h, color);
        return;
    }
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;

    // Center fill rects
    DrawFilledRect(dev, x + r, y, w - 2.f * r, h, color);
    DrawFilledRect(dev, x, y + r, r, h - 2.f * r, color);
    DrawFilledRect(dev, x + w - r, y + r, r, h - 2.f * r, color);

    // 4 Corner Triangle Fans
    const int segs = 6;
    const float pi = 3.14159265f;

    float cx[4] = { x + r,     x + w - r, x + w - r, x + r     };
    float cy[4] = { y + r,     y + r,     y + h - r, y + h - r };
    float startAngles[4] = { pi, 1.5f * pi, 0.f, 0.5f * pi };

    Vertex2D v[32];
    for (int i = 0; i < 4; i++) {
        int vCount = 0;
        v[vCount++] = { cx[i], cy[i], 0.f, 1.f, color };
        for (int j = 0; j <= segs; j++) {
            float a = startAngles[i] + (0.5f * pi * j / segs);
            float vx = cx[i] + cosf(a) * r;
            float vy = cy[i] + sinf(a) * r;
            v[vCount++] = { vx, vy, 0.f, 1.f, color };
        }

        dev->SetTexture(0, NULL);
        dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        dev->SetFVF(kFVF);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, vCount - 2, v, sizeof(Vertex2D));
    }

    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

/**
 * @brief Vẽ viền (border only, 4 cạnh).
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
 * @brief Vẽ viền bo tròn góc (rounded border).
 */
inline void DrawRoundedBorderRect(IDirect3DDevice9* dev,
    float x, float y, float w, float h, float r, float t, D3DCOLOR color)
{
    if (r <= 0.5f) {
        DrawBorderRect(dev, x, y, w, h, t, color);
        return;
    }
    if (r > w * 0.5f) r = w * 0.5f;
    if (r > h * 0.5f) r = h * 0.5f;

    DrawFilledRect(dev, x + r, y, w - 2.f * r, t, color);
    DrawFilledRect(dev, x + r, y + h - t, w - 2.f * r, t, color);
    DrawFilledRect(dev, x, y + r, t, h - 2.f * r, color);
    DrawFilledRect(dev, x + w - t, y + r, t, h - 2.f * r, color);

    const int segs = 6;
    const float pi = 3.14159265f;
    float cx[4] = { x + r,     x + w - r, x + w - r, x + r     };
    float cy[4] = { y + r,     y + r,     y + h - r, y + h - r };
    float startAngles[4] = { pi, 1.5f * pi, 0.f, 0.5f * pi };

    Vertex2D v[32];
    for (int i = 0; i < 4; i++) {
        int vCount = 0;
        for (int j = 0; j <= segs; j++) {
            float a = startAngles[i] + (0.5f * pi * j / segs);
            float innerX = cx[i] + cosf(a) * (r - t);
            float innerY = cy[i] + sinf(a) * (r - t);
            float outerX = cx[i] + cosf(a) * r;
            float outerY = cy[i] + sinf(a) * r;
            v[vCount++] = { outerX, outerY, 0.f, 1.f, color };
            v[vCount++] = { innerX, innerY, 0.f, 1.f, color };
        }

        dev->SetTexture(0, NULL);
        dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        dev->SetFVF(kFVF);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, vCount - 2, v, sizeof(Vertex2D));
    }

    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

/**
 * @brief Vẽ thanh progress bar bo tròn góc.
 */
inline void DrawProgressBar(IDirect3DDevice9* dev,
    float x, float y, float w, float h, float r,
    float percent, D3DCOLOR bgColor, D3DCOLOR fillColor)
{
    float p = percent;
    if (p < 0.f) p = 0.f;
    if (p > 100.f) p = 100.f;
    const float fill = (w - 2.f) * (p / 100.f);
    DrawRoundedFilledRect(dev, x, y, w, h, r, bgColor);
    if (fill > 2.f) {
        float innerR = r - 1.f;
        if (innerR < 1.f) innerR = 1.f;
        DrawRoundedFilledRect(dev, x + 1.f, y + 1.f, fill, h - 2.f, innerR, fillColor);
    }
    DrawRoundedBorderRect(dev, x, y, w, h, r, 1.f, D3DCOLOR_ARGB(255, 0, 0, 0));
}

/**
 * @brief Vẽ text với stroke đen 8 hướng (kiểu SA:MP).
 */
inline void DrawTextStroke(ID3DXFont* font, const char* text,
    RECT rect, DWORD fmt, D3DCOLOR textColor, D3DCOLOR strokeColor)
{
    RECT r = rect;

    // 8 hướng stroke
    r.left = rect.left - 1; r.right = rect.right - 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.left = rect.left + 1; r.right = rect.right + 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.top  = rect.top  - 1; r.bottom = rect.bottom - 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.top  = rect.top  + 1; r.bottom = rect.bottom + 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);

    r.left = rect.left - 1; r.right = rect.right - 1; r.top = rect.top - 1; r.bottom = rect.bottom - 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.left = rect.left + 1; r.right = rect.right + 1; r.top = rect.top - 1; r.bottom = rect.bottom - 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.left = rect.left - 1; r.right = rect.right - 1; r.top = rect.top + 1; r.bottom = rect.bottom + 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    r.left = rect.left + 1; r.right = rect.right + 1; r.top = rect.top + 1; r.bottom = rect.bottom + 1; font->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);

    // Text chính
    font->DrawTextA(NULL, text, -1, &rect, fmt, textColor);
}

inline std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};

    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (length <= 0) {
        codePage = CP_ACP;
        flags = 0;
        length = MultiByteToWideChar(codePage, flags, text.data(),
            static_cast<int>(text.size()), nullptr, 0);
    }
    if (length <= 0) return {};

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(codePage, flags, text.data(), static_cast<int>(text.size()),
        result.data(), length);
    return result;
}

inline void DrawTextStrokeW(ID3DXFont* font, const wchar_t* text,
    RECT rect, DWORD format, D3DCOLOR textColor, D3DCOLOR strokeColor)
{
    if (!font || !text) return;
    static constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    };

    for (const auto& offset : offsets) {
        RECT shifted = rect;
        OffsetRect(&shifted, offset[0], offset[1]);
        font->DrawTextW(nullptr, text, -1, &shifted, format, strokeColor);
    }
    font->DrawTextW(nullptr, text, -1, &rect, format, textColor);
}

/**
 * @brief Đo chiều rộng và cao của text khi dùng D3DXFont.
 */
inline SIZE MeasureText(ID3DXFont* font, const char* text) {
    SIZE sz = { 0, 0 };
    if (!font || !text) return sz;
    RECT r = { 0, 0, 0, 0 };
    font->DrawTextA(NULL, text, -1, &r, DT_CALCRECT, 0);
    sz.cx = r.right - r.left;
    sz.cy = r.bottom - r.top;
    return sz;
}

inline SIZE MeasureTextW(ID3DXFont* font, const wchar_t* text) {
    SIZE size = {0, 0};
    if (!font || !text) return size;
    RECT rect = {0, 0, 0, 0};
    font->DrawTextW(nullptr, text, -1, &rect, DT_CALCRECT, 0);
    size.cx = rect.right - rect.left;
    size.cy = rect.bottom - rect.top;
    return size;
}

} // namespace D3DHelper
