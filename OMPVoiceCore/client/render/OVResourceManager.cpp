#include "OVResourceManager.h"

#include "shared/OVLogger.h"

#include <Windows.h>
#include <wincodec.h>

namespace ov::client
{
namespace
{
template <typename T>
void ReleaseCom(T*& value)
{
    if (value) value->Release();
    value = nullptr;
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
void OVResourceManager::ReleaseAllTextures() { textures_.clear(); fallbacks_.clear(); }
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
    const auto path = directory_ / name;
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitializeCom = comResult == S_OK || comResult == S_FALSE;
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    IDirect3DTexture9* texture = nullptr;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    UINT width{}, height{};
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    if (SUCCEEDED(result) && width > 0 && height > 0)
    {
        result = device_->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr);
        D3DLOCKED_RECT locked{};
        if (SUCCEEDED(result)) result = texture->LockRect(0, &locked, nullptr, 0);
        if (SUCCEEDED(result))
        {
            result = converter->CopyPixels(nullptr, width * 4U, locked.Pitch * height, static_cast<BYTE*>(locked.pBits));
            texture->UnlockRect(0);
        }
    }
    ReleaseCom(converter);
    ReleaseCom(frame);
    ReleaseCom(decoder);
    ReleaseCom(factory);
    if (uninitializeCom) CoUninitialize();
    if (SUCCEEDED(result) && texture)
    {
        textures_[name] = TextureHandle(texture);
        fallbacks_.erase(name);
        return true;
    }
    if (texture) texture->Release();
    OV_LOG_WARN("Render", "Missing or unsupported texture %s; fallback generated", path.string().c_str());
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
    fallbacks_.insert(name);
    return true;
}
}
