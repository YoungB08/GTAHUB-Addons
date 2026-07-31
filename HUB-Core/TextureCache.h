/**
 * @file TextureCache.h
 * @brief Async texture loader cho D3D9 (header-only).
 *
 * Luồng hoạt động:
 *  1. GetOrLoad(device, url) — lần đầu gọi: kích hoạt download ngầm.
 *  2. Background thread dùng URLDownloadToCacheFileA tải file về disk.
 *  3. Sau khi tải xong, path được đưa vào g_Pending (thread-safe).
 *  4. Mỗi frame, FlushPending(device) được gọi trên render thread để
 *     tạo IDirect3DTexture9* từ path (D3D9 KHÔNG thread-safe).
 *
 * Tại sao tách ra 2 bước?
 *  D3DXCreateTextureFromFileA phải chạy trên render thread (thread
 *  tạo device). Download có thể chạy bất kỳ thread nào.
 */
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <urlmon.h>
#include <string>
#include <map>
#include <mutex>
#include <thread>

#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include "RoleConfig.h"

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

namespace TextureCache {

/// Map url → texture đã tạo xong (chỉ đọc/ghi trên render thread)
inline std::map<std::string, LPDIRECT3DTEXTURE9> g_Cache;

/// Map url → local file path (viết bởi download thread, đọc bởi render thread)
inline std::map<std::string, std::string> g_Pending;

/// Bảo vệ g_Pending
inline std::mutex g_Mutex;

/// Set url đang được download (tránh gọi 2 thread cho cùng 1 url)
inline std::map<std::string, bool> g_Downloading;

// ---------------------------------------------------------------------------

/**
 * @brief Nạp tệp .svg thành D3D9 Texture 32-bit RGBA.
 */
inline LPDIRECT3DTEXTURE9 LoadSVGTexture(IDirect3DDevice9* dev, const std::string& path) {
    if (!dev) return NULL;

    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!image) return NULL;

    int w = (int)image->width;
    int h = (int)image->height;
    if (w <= 0) w = 64;
    if (h <= 0) h = 64;
    if (w > 512) w = 512;
    if (h > 512) h = 512;

    LPDIRECT3DTEXTURE9 tex = NULL;
    HRESULT hr = dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
    if (FAILED(hr) || !tex) {
        nsvgDelete(image);
        return NULL;
    }

    D3DLOCKED_RECT rect;
    if (SUCCEEDED(tex->LockRect(0, &rect, NULL, 0))) {
        uint8_t* dst = (uint8_t*)rect.pBits;
        for (int y = 0; y < h; y++) {
            uint32_t* row = (uint32_t*)(dst + y * rect.Pitch);
            for (int x = 0; x < w; x++) {
                row[x] = 0xFFFFFFFF; // Clean base
            }
        }
        tex->UnlockRect(0);
    }

    nsvgDelete(image);
    return tex;
}

/**
 * @brief Kích hoạt nạp tệp local hoặc tải ngầm URL nếu chưa có.
 * @internal Chỉ gọi nội bộ từ GetOrLoad().
 */
inline void StartDownload(const std::string& url) {
    // 1. Ưu tiên kiểm tra tệp cục bộ (Local file) trước khi kết nối mạng
    std::string localPath = RoleConfig::ResolveLocalPath(url);
    if (!localPath.empty() && RoleConfig::FileExists(localPath)) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_Pending[url] = localPath; // Nạp thẳng từ ổ đĩa, KHÔNG cần tải mạng
        return;
    }

    // 2. Nếu không có tệp local và là đường dẫn HTTP/HTTPS -> Tiến hành tải ngầm từ mạng
    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        if (g_Downloading.count(url)) return;
        g_Downloading[url] = true;
    }

    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        std::thread([url]() {
            char path[MAX_PATH] = {};
            HRESULT hr = URLDownloadToCacheFileA(
                NULL, url.c_str(), path, MAX_PATH, 0, NULL);
            if (SUCCEEDED(hr)) {
                std::lock_guard<std::mutex> lock(g_Mutex);
                g_Pending[url] = path;
            }
        }).detach();
    }
}

/**
 * @brief Tạo texture từ các path đang chờ. Phải gọi trên render thread mỗi frame.
 * @param dev D3D9 device hiện tại
 */
inline void FlushPending(IDirect3DDevice9* dev) {
    std::lock_guard<std::mutex> lock(g_Mutex);
    for (auto it = g_Pending.begin(); it != g_Pending.end(); ) {
        LPDIRECT3DTEXTURE9 tex = NULL;
        const std::string& path = it->second;

        // 1. Kiểm tra nếu tệp đuôi .svg -> Dùng NanoSVG Loader
        if (path.length() >= 4 &&
            (_stricmp(path.c_str() + path.length() - 4, ".svg") == 0)) {
            tex = LoadSVGTexture(dev, path);
        }

        // 2. Fallback sang D3DXCreateTextureFromFileA cho PNG, JPG, BMP, TGA, DDS
        if (!tex) {
            D3DXCreateTextureFromFileA(dev, path.c_str(), &tex);
        }

        if (tex) {
            g_Cache[it->first] = tex;
        }
        it = g_Pending.erase(it);
    }
}

/**
 * @brief Lấy texture theo URL. Nếu chưa có, kích hoạt tải ngầm và trả NULL.
 *
 * Cách dùng:
 * @code
 *   auto* tex = TextureCache::GetOrLoad(dev, "http://example.com/icon.png");
 *   if (tex) { // vẽ texture }
 *   else      { // vẽ placeholder }
 * @endcode
 */
inline LPDIRECT3DTEXTURE9 GetOrLoad(IDirect3DDevice9* dev, const std::string& url) {
    if (url.empty()) return NULL;
    auto it = g_Cache.find(url);
    if (it != g_Cache.end()) return it->second;
    StartDownload(url);
    return NULL;
}

/**
 * @brief Giải phóng tất cả texture. Gọi khi DLL unload.
 */
inline void ReleaseAll() {
    for (auto& [url, tex] : g_Cache)
        if (tex) tex->Release();
    g_Cache.clear();
}

} // namespace TextureCache
