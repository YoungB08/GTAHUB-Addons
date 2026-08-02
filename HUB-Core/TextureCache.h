#pragma once

#include "RoleConfig.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <urlmon.h>
#include <wrl/client.h>

#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "urlmon.lib")

namespace TextureCache {

using Microsoft::WRL::ComPtr;

inline std::unordered_map<std::string, ComPtr<IDirect3DTexture9>> g_Cache;
inline std::unordered_map<std::string, std::string> g_Pending;
inline std::unordered_set<std::string> g_Loading;
inline std::vector<std::thread> g_Workers;
inline std::mutex g_Mutex;

inline void QueueLocalPath(const std::string& key, const std::string& path) {
    std::lock_guard<std::mutex> guard(g_Mutex);
    g_Pending[key] = path;
    g_Loading.erase(key);
}

inline void StartLoad(const std::string& key) {
    std::string localPath = RoleConfig::ResolveLocalPath(key);
    if (localPath.empty() && RoleConfig::FileExists(key)) {
        localPath = key;
    }
    if (!localPath.empty()) {
        QueueLocalPath(key, localPath);
        return;
    }

    const bool isUrl = key.rfind("http://", 0) == 0 || key.rfind("https://", 0) == 0;
    if (!isUrl) {
        std::lock_guard<std::mutex> guard(g_Mutex);
        g_Loading.erase(key);
        return;
    }

    std::thread worker([key]() {
        char cachePath[MAX_PATH] = {};
        const HRESULT result = URLDownloadToCacheFileA(nullptr, key.c_str(), cachePath,
            static_cast<DWORD>(std::size(cachePath)), 0, nullptr);
        if (SUCCEEDED(result)) {
            QueueLocalPath(key, cachePath);
        } else {
            std::lock_guard<std::mutex> guard(g_Mutex);
            g_Loading.erase(key);
        }
    });
    std::lock_guard<std::mutex> guard(g_Mutex);
    g_Workers.emplace_back(std::move(worker));
}

inline IDirect3DTexture9* GetOrLoad(IDirect3DDevice9*, const std::string& key) {
    if (key.empty()) return nullptr;

    {
        std::lock_guard<std::mutex> guard(g_Mutex);
        auto cached = g_Cache.find(key);
        if (cached != g_Cache.end()) return cached->second.Get();
        if (!g_Loading.insert(key).second) return nullptr;
    }

    StartLoad(key);
    return nullptr;
}

inline void FlushPending(IDirect3DDevice9* device) {
    if (!device) return;

    std::unordered_map<std::string, std::string> pending;
    {
        std::lock_guard<std::mutex> guard(g_Mutex);
        pending.swap(g_Pending);
    }

    for (const auto& [key, path] : pending) {
        ComPtr<IDirect3DTexture9> texture;
        const HRESULT result = D3DXCreateTextureFromFileExA(device, path.c_str(),
            D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT, 0,
            D3DFMT_UNKNOWN, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT,
            0, nullptr, nullptr, texture.GetAddressOf());

        std::lock_guard<std::mutex> guard(g_Mutex);
        if (SUCCEEDED(result) && texture) {
            g_Cache[key] = std::move(texture);
        }
        g_Loading.erase(key);
    }
}

inline void ReleaseAll() {
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> guard(g_Mutex);
        workers.swap(g_Workers);
    }
    for (std::thread& worker : workers) {
        if (worker.joinable()) worker.join();
    }
    std::lock_guard<std::mutex> guard(g_Mutex);
    g_Cache.clear();
    g_Pending.clear();
    g_Loading.clear();
}

} // namespace TextureCache
