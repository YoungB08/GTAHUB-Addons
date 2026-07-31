/**
 * @file TextureCache.h
 * @brief Async texture loader cho D3D9 (header-only).
 *
 * Luồng hoạt động:
 *  1. GetOrLoad(device, url) — lần đầu gọi: kích hoạt nạp tệp ngầm / local.
 *  2. Background thread dùng URLDownloadToCacheFileA tải file PNG/JPG/BMP/DDS về disk.
 *  3. Sau khi tải xong, path được đưa vào g_Pending (thread-safe).
 *  4. Mỗi frame, FlushPending(device) được gọi trên render thread để
 *     tạo IDirect3DTexture9* từ tệp bằng D3DXCreateTextureFromFileA.
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
 * @brief Kích hoạt nạp tệp local hoặc tải ngầm URL nếu chưa có.
 * @internal Chỉ gọi nội bộ từ GetOrLoad().
 */
inline void StartDownload(const std::string& url) {
    // 1. Ưu tiên kiểm tra tệp cục bộ (Local file PNG/JPG) theo map config hoặc đường dẫn trực tiếp
    std::string localPath = RoleConfig::ResolveLocalPath(url);
    if (!localPath.empty() && RoleConfig::FileExists(localPath)) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_Pending[url] = localPath; // Nạp thẳng từ ổ đĩa
        return;
    }

    if (RoleConfig::FileExists(url)) {
        std::lock_guard<std::mutex> lock(g_Mutex);
        g_Pending[url] = url;
        return;
    }

    // 2. Chỉ tiến hành tải ngầm từ mạng nếu chuỗi là URL HTTP/HTTPS
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
        {
            std::lock_guard<std::mutex> lock(g_Mutex);
            if (g_Downloading.count(url)) return;
            g_Downloading[url] = true;
        }

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

        // Nạp tệp ảnh (PNG, JPG, BMP, TGA, DDS) trực tiếp qua D3DX (Hỗ trợ 100% Alpha Channel)
        D3DXCreateTextureFromFileA(dev, path.c_str(), &tex);

        if (tex) {
            g_Cache[it->first] = tex;
        }
        it = g_Pending.erase(it);
    }
}

/**
 * @brief Lấy texture theo URL / Path. Nếu chưa có, kích hoạt nạp ngầm và trả NULL.
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
