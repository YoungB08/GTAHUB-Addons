#pragma once

#include <d3d9.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace ov::client
{
struct TextureDeleter { void operator()(IDirect3DTexture9* texture) const noexcept { if (texture) texture->Release(); } };
using TextureHandle = std::unique_ptr<IDirect3DTexture9, TextureDeleter>;

class OVResourceManager final
{
public:
    ~OVResourceManager();
    bool Initialize(IDirect3DDevice9* device, std::filesystem::path directory);
    void ReleaseAllTextures();
    bool ReloadTextures();
    [[nodiscard]] IDirect3DTexture9* Get(const std::string& name) const;
    [[nodiscard]] bool IsLoaded(const std::string& name) const;
    [[nodiscard]] const std::filesystem::path& Directory() const noexcept { return directory_; }

private:
    bool LoadTexture(const std::string& name);
    bool CreateFallback(const std::string& name);
    IDirect3DDevice9* device_{};
    std::filesystem::path directory_;
    std::unordered_map<std::string, TextureHandle> textures_;
};
}
