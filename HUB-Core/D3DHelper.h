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

inline std::wstring ConvertTCVN3ToWide(const std::string& text) {
    std::wstring result;
    result.reserve(text.size());

    for (unsigned char c : text) {
        wchar_t wc = 0;
        switch (c) {
            case 0x80: wc = 0x00E0; break; // à
            case 0x81: wc = 0x01EA3; break; // ả
            case 0x82: wc = 0x00E3; break; // ã
            case 0x83: wc = 0x00E1; break; // á
            case 0x84: wc = 0x01EA1; break; // ạ
            case 0x85: wc = 0x0103; break; // ă
            case 0x86: wc = 0x01EB1; break; // ằ
            case 0x87: wc = 0x01EB3; break; // ẳ
            case 0x88: wc = 0x01EB5; break; // ẵ
            case 0x89: wc = 0x01EB9; break; // ắ
            case 0x8A: wc = 0x01EB7; break; // ặ
            case 0x8B: wc = 0x01EC1; break; // ề
            case 0x8C: wc = 0x01EC3; break; // ể
            case 0x8D: wc = 0x01EC5; break; // ễ
            case 0x8E: wc = 0x01EBF; break; // ế
            case 0x8F: wc = 0x01EC7; break; // ệ
            case 0x90: wc = 0x01EC9; break; // ỉ
            case 0x91: wc = 0x01ECB; break; // ị
            case 0x92: wc = 0x01ECD; break; // ọ
            case 0x93: wc = 0x01ECF; break; // ỏ
            case 0x94: wc = 0x01ED7; break; // ỗ
            case 0x95: wc = 0x01ED5; break; // ố
            case 0x96: wc = 0x01ED9; break; // ộ
            case 0x97: wc = 0x01A1;  break; // ơ
            case 0x98: wc = 0x01EDD; break; // ờ
            case 0x99: wc = 0x01EDF; break; // ở
            case 0x9A: wc = 0x01EE1; break; // ỡ
            case 0x9B: wc = 0x01EDB; break; // ớ
            case 0x9C: wc = 0x01EE3; break; // ợ
            case 0x9D: wc = 0x01EE5; break; // ụ
            case 0x9E: wc = 0x01EE7; break; // ủ
            case 0x9F: wc = 0x01EEF; break; // ữ
            case 0xA0: wc = 0x01EEB; break; // ứ
            case 0xA1: wc = L'a';    break;
            case 0xA2: wc = 0x00E2;  break; // â
            case 0xA3: wc = 0x01EA5; break; // ấ
            case 0xA4: wc = 0x01EA7; break; // ầ
            case 0xA5: wc = 0x01EA9; break; // ẩ
            case 0xA6: wc = 0x01EAB; break; // ẫ
            case 0xA7: wc = 0x01EAD; break; // ậ
            case 0xA8: wc = L'e';    break;
            case 0xA9: wc = 0x00EA;  break; // ê
            case 0xAA: wc = L'i';    break;
            case 0xAB: wc = L'o';    break;
            case 0xAC: wc = 0x00F4;  break; // ô
            case 0xAD: wc = 0x01ED5; break; // ố
            case 0xAE: wc = L'u';    break;
            case 0xAF: wc = 0x01B0;  break; // ư
            case 0xB0: wc = 0x01EED; break; // ừ
            case 0xB1: wc = 0x01EEF; break; // ử
            case 0xB2: wc = 0x01EF1; break; // ữ
            case 0xB3: wc = 0x01EF3; break; // ự
            case 0xB4: wc = L'y';    break;
            case 0xB5: wc = 0x01EF3; break; // ỳ
            case 0xB6: wc = 0x01EF7; break; // ỷ
            case 0xB7: wc = 0x01EF9; break; // ỹ
            case 0xB8: wc = 0x01EF5; break; // ỵ
            case 0xB9: wc = 0x0119;  break; // đ
            case 0xBB: wc = 0x0102;  break; // Ă
            case 0xBC: wc = 0x00C2;  break; // Â
            case 0xBD: wc = 0x00CA;  break; // Ê
            case 0xBE: wc = 0x00D4;  break; // Ô
            case 0xC6: wc = 0x01A0;  break; // Ơ
            case 0xDD: wc = 0x01AF;  break; // Ư
            case 0xE1: wc = 0x0118;  break; // Đ
            default:   wc = static_cast<wchar_t>(c); break;
        }
        result.push_back(wc);
    }
    return result;
}

inline std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};

    // 1. Try UTF-8 conversion
    int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0);
    if (length > 0) {
        std::wstring result(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
            result.data(), length) > 0) {
            std::wstring composed(result.size(), L'\0');
            int compLen = FoldStringW(MAP_PRECOMPOSED, result.data(), static_cast<int>(result.size()),
                composed.data(), static_cast<int>(composed.size()));
            if (compLen > 0) {
                composed.resize(compLen);
                return composed;
            }
            return result;
        }
    }

    // 2. Try Windows-1258 (Vietnamese Windows Code Page)
    length = MultiByteToWideChar(1258, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length > 0) {
        std::wstring result(static_cast<size_t>(length), L'\0');
        if (MultiByteToWideChar(1258, 0, text.data(), static_cast<int>(text.size()),
            result.data(), length) > 0) {
            std::wstring composed(result.size(), L'\0');
            int compLen = FoldStringW(MAP_PRECOMPOSED, result.data(), static_cast<int>(result.size()),
                composed.data(), static_cast<int>(composed.size()));
            if (compLen > 0) {
                composed.resize(compLen);
                return composed;
            }
            return result;
        }
    }

    // 3. Check for TCVN3 character byte range (0x80..0xBF)
    bool hasTcvn3 = std::any_of(text.begin(), text.end(), [](char c) {
        unsigned char uc = static_cast<unsigned char>(c);
        return uc >= 0x80 && uc <= 0xBF;
    });
    if (hasTcvn3) {
        return ConvertTCVN3ToWide(text);
    }

    // 4. Fallback: System Code Page (CP_ACP)
    length = MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) return std::wstring(text.begin(), text.end());

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.data(), static_cast<int>(text.size()),
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

inline std::wstring FoldStringW(const std::wstring& input) {
    if (input.empty()) return {};
    std::wstring composed(input.size(), L'\0');
    int compLen = ::FoldStringW(MAP_PRECOMPOSED, input.data(), static_cast<int>(input.size()),
        composed.data(), static_cast<int>(composed.size()));
    if (compLen > 0) {
        composed.resize(compLen);
        return composed;
    }
    return input;
}

inline std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        result.data(), length, nullptr, nullptr);
    return result;
}

inline std::string WideToTCVN3(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (wchar_t wc : text) {
        char c = 0;
        switch (wc) {
            case 0x00E0: c = (char)0x80; break; // à
            case 0x01EA3: c = (char)0x81; break; // ả
            case 0x00E3: c = (char)0x82; break; // ã
            case 0x00E1: c = (char)0x83; break; // á
            case 0x01EA1: c = (char)0x84; break; // ạ
            case 0x0103: c = (char)0x85; break; // ă
            case 0x01EB1: c = (char)0x86; break; // ằ
            case 0x01EB3: c = (char)0x87; break; // ẳ
            case 0x01EB5: c = (char)0x88; break; // ẵ
            case 0x01EB9: c = (char)0x89; break; // ắ
            case 0x01EB7: c = (char)0x8A; break; // ặ
            case 0x01EC1: c = (char)0x8B; break; // ề
            case 0x01EC3: c = (char)0x8C; break; // ể
            case 0x01EC5: c = (char)0x8D; break; // ễ
            case 0x01EBF: c = (char)0x8E; break; // ế
            case 0x01EC7: c = (char)0x8F; break; // ệ
            case 0x01EC9: c = (char)0x90; break; // ỉ
            case 0x01ECB: c = (char)0x91; break; // ị
            case 0x01ECD: c = (char)0x92; break; // ọ
            case 0x01ECF: c = (char)0x93; break; // ỏ
            case 0x01ED7: c = (char)0x94; break; // ỗ
            case 0x01ED5: c = (char)0x95; break; // ố
            case 0x01ED9: c = (char)0x96; break; // ộ
            case 0x01A1:  c = (char)0x97; break; // ơ
            case 0x01EDD: c = (char)0x98; break; // ờ
            case 0x01EDF: c = (char)0x99; break; // ở
            case 0x01EE1: c = (char)0x9A; break; // ỡ
            case 0x01EDB: c = (char)0x9B; break; // ớ
            case 0x01EE3: c = (char)0x9C; break; // ợ
            case 0x01EE5: c = (char)0x9D; break; // ụ
            case 0x01EE7: c = (char)0x9E; break; // ủ
            case 0x00E2:  c = (char)0xA2; break; // â
            case 0x01EA5: c = (char)0xA3; break; // ấ
            case 0x01EA7: c = (char)0xA4; break; // ầ
            case 0x01EA9: c = (char)0xA5; break; // ẩ
            case 0x01EAB: c = (char)0xA6; break; // ẫ
            case 0x01EAD: c = (char)0xA7; break; // ậ
            case 0x00EA:  c = (char)0xA9; break; // ê
            case 0x00F4:  c = (char)0xAC; break; // ô
            case 0x01B0:  c = (char)0xAF; break; // ư
            case 0x01EED: c = (char)0xB0; break; // ừ
            case 0x01EEF: c = (char)0xB1; break; // ử
            case 0x01EF1: c = (char)0xB2; break; // ữ
            case 0x01EF3: c = (char)0xB3; break; // ự
            case 0x01EF7: c = (char)0xB6; break; // ỷ
            case 0x01EF9: c = (char)0xB7; break; // ỹ
            case 0x01EF5: c = (char)0xB8; break; // ỵ
            case 0x0119:  c = (char)0xB9; break; // đ
            case 0x0102:  c = (char)0xBB; break; // Ă
            case 0x00C2:  c = (char)0xBC; break; // Â
            case 0x00CA:  c = (char)0xBD; break; // Ê
            case 0x00D4:  c = (char)0xBE; break; // Ô
            case 0x01A0:  c = (char)0xC6; break; // Ơ
            case 0x01AF:  c = (char)0xDD; break; // Ư
            case 0x0118:  c = (char)0xE1; break; // Đ
            default:
                if (wc <= 255) c = static_cast<char>(wc);
                else c = '?';
                break;
        }
        result.push_back(c);
    }
    return result;
}

} // namespace D3DHelper
