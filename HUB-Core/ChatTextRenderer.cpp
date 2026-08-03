#include "pch.h"
#include "ChatTextRenderer.h"

#include "Logger.h"

#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unordered_map>

using Microsoft::WRL::ComPtr;

namespace {

struct TextTexture {
    ComPtr<IDirect3DTexture9> texture;
    int width = 0;
    int height = 0;
    float maximumU = 1.0f;
    float maximumV = 1.0f;
};

struct TexturedVertex {
    float x;
    float y;
    float z;
    float rhw;
    D3DCOLOR color;
    float u;
    float v;
};

constexpr DWORD kTexturedFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr size_t kMaximumCachedStrings = 512;

IDirect3DDevice9* g_Device = nullptr;
HDC g_DeviceContext = nullptr;
HFONT g_Font = nullptr;
HGDIOBJ g_PreviousFont = nullptr;
int g_FontHeight = 0;
int g_LineHeight = 0;
std::unordered_map<std::wstring, TextTexture> g_TextureCache;

int NextPowerOfTwo(int value) {
    int result = 1;
    while (result < value && result < 4096) result <<= 1;
    return result;
}

bool CreateGdiResources(int fontHeight) {
    g_DeviceContext = CreateCompatibleDC(nullptr);
    if (!g_DeviceContext) return false;
    g_Font = CreateFontW(-fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    if (!g_Font) return false;
    g_PreviousFont = SelectObject(g_DeviceContext, g_Font);
    SetBkMode(g_DeviceContext, TRANSPARENT);
    SetTextColor(g_DeviceContext, RGB(255, 255, 255));
    TEXTMETRICW metrics{};
    if (!GetTextMetricsW(g_DeviceContext, &metrics)) return false;
    g_LineHeight = (std::max)(fontHeight, static_cast<int>(metrics.tmHeight));
    return true;
}

SIZE Measure(const std::wstring& text) {
    SIZE size{};
    if (!g_DeviceContext || text.empty()) return size;
    GetTextExtentPoint32W(g_DeviceContext, text.c_str(), static_cast<int>(text.size()), &size);
    size.cy = g_LineHeight;
    return size;
}

TextTexture* CreateTexture(const std::wstring& text) {
    if (!g_Device || !g_DeviceContext || text.empty()) return nullptr;
    if (g_TextureCache.size() >= kMaximumCachedStrings) g_TextureCache.clear();

    const SIZE measured = Measure(text);
    const int bitmapWidth = std::clamp(static_cast<int>(measured.cx), 1, 2048);
    const int bitmapHeight = (std::max)(1, static_cast<int>(measured.cy));
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = bitmapWidth;
    bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapPixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(g_DeviceContext, &bitmapInfo, DIB_RGB_COLORS,
        &bitmapPixels, nullptr, 0);
    if (!bitmap || !bitmapPixels) {
        if (bitmap) DeleteObject(bitmap);
        return nullptr;
    }

    HGDIOBJ previousBitmap = SelectObject(g_DeviceContext, bitmap);
    std::memset(bitmapPixels, 0, static_cast<size_t>(bitmapWidth) * bitmapHeight * sizeof(uint32_t));
    TextOutW(g_DeviceContext, 0, 0, text.c_str(), static_cast<int>(text.size()));
    GdiFlush();

    const int textureWidth = NextPowerOfTwo(bitmapWidth);
    const int textureHeight = NextPowerOfTwo(bitmapHeight);
    TextTexture value;
    const HRESULT createResult = g_Device->CreateTexture(textureWidth, textureHeight, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, value.texture.GetAddressOf(), nullptr);
    if (SUCCEEDED(createResult)) {
        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(value.texture->LockRect(0, &locked, nullptr, 0))) {
            for (int row = 0; row < textureHeight; ++row) {
                auto* destination = reinterpret_cast<uint32_t*>(
                    static_cast<unsigned char*>(locked.pBits) + row * locked.Pitch);
                std::memset(destination, 0, static_cast<size_t>(textureWidth) * sizeof(uint32_t));
                if (row >= bitmapHeight) continue;
                const auto* source = static_cast<const uint32_t*>(bitmapPixels) + row * bitmapWidth;
                for (int column = 0; column < bitmapWidth; ++column) {
                    const uint32_t sample = source[column];
                    const uint32_t coverage = (std::max)({sample & 0xFFu,
                        (sample >> 8) & 0xFFu, (sample >> 16) & 0xFFu});
                    destination[column] = (coverage << 24) | 0x00FFFFFFu;
                }
            }
            value.texture->UnlockRect(0);
            value.width = bitmapWidth;
            value.height = bitmapHeight;
            value.maximumU = bitmapWidth / static_cast<float>(textureWidth);
            value.maximumV = bitmapHeight / static_cast<float>(textureHeight);
        } else {
            value.texture.Reset();
        }
    }

    SelectObject(g_DeviceContext, previousBitmap);
    DeleteObject(bitmap);
    if (!value.texture) {
        Logger::Error("Custom Chat failed to create a cached text texture. hr=0x%08X",
            static_cast<unsigned>(createResult));
        return nullptr;
    }
    auto inserted = g_TextureCache.emplace(text, std::move(value));
    return &inserted.first->second;
}

TextTexture* GetTexture(const std::wstring& text) {
    const auto found = g_TextureCache.find(text);
    return found != g_TextureCache.end() ? &found->second : CreateTexture(text);
}

} // namespace

namespace HUB::Chat::TextRenderer {

bool Initialize(IDirect3DDevice9* device, int fontHeight) {
    if (!device) return false;
    fontHeight = (std::max)(10, fontHeight);
    if (g_Device == device && g_DeviceContext && g_Font && g_FontHeight == fontHeight) return true;
    Shutdown();
    g_Device = device;
    g_FontHeight = fontHeight;
    if (!CreateGdiResources(fontHeight)) {
        Logger::Error("Custom Chat failed to create GDI text resources. error=%lu", GetLastError());
        Shutdown();
        return false;
    }
    Logger::D3DLog("Custom Chat Unicode texture font created. height=%d", fontHeight);
    return true;
}

float MeasureWidth(const std::wstring& text) {
    return static_cast<float>(Measure(text).cx);
}

bool Draw(const std::wstring& text, float x, float y, float right, float height,
    D3DCOLOR color, DWORD format) {
    TextTexture* cached = GetTexture(text);
    if (!cached || !cached->texture || right <= x) return false;
    const float drawWidth = (std::min)(static_cast<float>(cached->width), right - x);
    const float drawHeight = static_cast<float>(cached->height);
    const float drawY = (format & DT_VCENTER) != 0 ? y + (height - drawHeight) * 0.5f : y;
    const float maximumU = cached->maximumU * drawWidth / cached->width;
    const float left = x - 0.5f;
    const float top = drawY - 0.5f;
    const float rightEdge = left + drawWidth;
    const float bottom = top + drawHeight;
    TexturedVertex vertices[4] = {
        {left, top, 0.0f, 1.0f, color, 0.0f, 0.0f},
        {rightEdge, top, 0.0f, 1.0f, color, maximumU, 0.0f},
        {left, bottom, 0.0f, 1.0f, color, 0.0f, cached->maximumV},
        {rightEdge, bottom, 0.0f, 1.0f, color, maximumU, cached->maximumV},
    };
    g_Device->SetVertexShader(nullptr);
    g_Device->SetPixelShader(nullptr);
    g_Device->SetTexture(0, cached->texture.Get());
    g_Device->SetFVF(kTexturedFvf);
    g_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    g_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_Device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    g_Device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    g_Device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_Device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    g_Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_Device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    g_Device->SetRenderState(D3DRS_COLORWRITEENABLE,
        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
        D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
    return SUCCEEDED(g_Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices,
        sizeof(TexturedVertex)));
}

void OnLostDevice() {
    g_TextureCache.clear();
}

void Shutdown() {
    g_TextureCache.clear();
    if (g_DeviceContext && g_PreviousFont) SelectObject(g_DeviceContext, g_PreviousFont);
    if (g_Font) DeleteObject(g_Font);
    if (g_DeviceContext) DeleteDC(g_DeviceContext);
    g_Device = nullptr;
    g_DeviceContext = nullptr;
    g_Font = nullptr;
    g_PreviousFont = nullptr;
    g_FontHeight = 0;
    g_LineHeight = 0;
}

} // namespace HUB::Chat::TextRenderer
