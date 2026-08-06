#include "OVResourceManager.h"

#include "shared/OVLogger.h"

#include <Windows.h>

namespace ov::client
{
namespace
{
using CreateTextureFromFileFn = HRESULT(WINAPI*)(LPDIRECT3DDEVICE9, LPCSTR, LPDIRECT3DTEXTURE9*);
CreateTextureFromFileFn FindTextureLoader()
{
    static HMODULE module = LoadLibraryA("d3dx9_43.dll");
    return module ? reinterpret_cast<CreateTextureFromFileFn>(GetProcAddress(module, "D3DXCreateTextureFromFileA")) : nullptr;
}
}

OVResourceManager::~OVResourceManager() { ReleaseAllTextures(); }
bool OVResourceManager::Initialize(IDirect3DDevice9* device, std::filesystem::path directory)
{
    device_ = device;
    directory_ = std::move(directory);
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    return device_ != nullptr;
}
void OVResourceManager::ReleaseAllTextures() { textures_.clear(); }
bool OVResourceManager::ReloadTextures()
{
    ReleaseAllTextures();
    static constexpr const char* names[] = {"logo.png", "micro_active.png", "micro_muted.png", "micro_passive.png", "speaker.png"};
    bool allLoaded = true;
    for (const auto* name : names) allLoaded = LoadTexture(name) && allLoaded;
    return allLoaded;
}
IDirect3DTexture9* OVResourceManager::Get(const std::string& name) const
{
    const auto it = textures_.find(name);
    return it == textures_.end() ? nullptr : it->second.get();
}
bool OVResourceManager::IsLoaded(const std::string& name) const { return Get(name) != nullptr; }
bool OVResourceManager::LoadTexture(const std::string& name)
{
    const auto path = (directory_ / name).string();
    IDirect3DTexture9* texture = nullptr;
    if (const auto loader = FindTextureLoader(); loader && SUCCEEDED(loader(device_, path.c_str(), &texture)))
    {
        textures_[name] = TextureHandle(texture);
        return true;
    }
    OV_LOG_WARN("Render", "Missing or unsupported texture %s; fallback generated", path.c_str());
    return CreateFallback(name);
}
bool OVResourceManager::CreateFallback(const std::string& name)
{
    IDirect3DTexture9* texture = nullptr;
    if (!device_ || FAILED(device_->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr))) return false;
    D3DLOCKED_RECT locked{};
    if (FAILED(texture->LockRect(0, &locked, nullptr, 0))) { texture->Release(); return false; }
    const DWORD color = name.find("muted") != std::string::npos ? 0xD0E05252U : 0xD02BC7A8U;
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) static_cast<DWORD*>(locked.pBits)[y * locked.Pitch / 4 + x] = (x > 2 && x < 61 && y > 2 && y < 61) ? color : 0;
    texture->UnlockRect(0);
    textures_[name] = TextureHandle(texture);
    return true;
}
}
