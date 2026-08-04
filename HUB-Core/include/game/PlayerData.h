/**
 * @file PlayerData.h
 * @brief Dữ liệu nametag và role của mỗi player trên Client ASI (Direct3D 9 Engine).
 */
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <array>
#include <cmath>
#include <d3d9.h>
#include <mutex>
#include <string>

/// Số player tối đa trong SAMP / open:mp
constexpr int kMaxPlayers = 1004;

/// Số slot role tối đa hiển thị cùng lúc (Tối đa 10 Slots)
constexpr int kMaxRoleSlots = 10;

/**
 * @brief Dữ liệu hiển thị Role Badge tại 1 Slot.
 */
struct RoleSlotClientData {
    bool        active     = false;
    std::string text;
    D3DCOLOR    color      = 0;
    D3DCOLOR    bgColor    = 0;
    bool        stroke     = true;
    std::string imagePath;

    // Rainbow Effect per Slot
    bool        isRainbow  = false;
    uint32_t    rainbowSpeedMs = 500;
    float       currentHue = 0.0f;
};

/**
 * @brief Dữ liệu Nametag đầy đủ của 1 player trên Client.
 */
struct PlayerNametag {
    std::array<RoleSlotClientData, kMaxRoleSlots> slots;
    
    // Custom Nametag Color
    D3DCOLOR    nametagColor = D3DCOLOR_ARGB(255, 255, 255, 255);
    bool        hasCustomNametagColor = false;
    
    // Rainbow Effect cho Nametag chính
    bool        isNametagRainbow = false;
    uint32_t    nametagRainbowSpeedMs = 500;
    float       nametagRainbowHue = 0.0f;

    // Direct3D 9 Visibility Toggle
    bool        visible  = true;
    bool        hasData  = false;
};

/// Global array lưu trữ trạng thái người chơi, index = playerID (0–1003)
inline std::array<PlayerNametag, kMaxPlayers> g_Players;
inline std::mutex g_PlayerDataMutex;

/**
 * @brief Helper đổi góc màu HSV sang D3DCOLOR (ARGB).
 */
inline D3DCOLOR GetRainbowD3DColor(float h, uint8_t alpha = 255) {
    float c = 1.0f;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 60)       { r = c; g = x; b = 0; }
    else if (h >= 60 && h < 120){ r = x; g = c; b = 0; }
    else if (h >= 120 && h < 180){ r = 0; g = c; b = x; }
    else if (h >= 180 && h < 240){ r = 0; g = x; b = c; }
    else if (h >= 240 && h < 300){ r = x; g = 0; b = c; }
    else                        { r = c; g = 0; b = x; }

    uint8_t R = static_cast<uint8_t>(r * 255.0f);
    uint8_t G = static_cast<uint8_t>(g * 255.0f);
    uint8_t B = static_cast<uint8_t>(b * 255.0f);

    return D3DCOLOR_ARGB(alpha, R, G, B);
}

/**
 * @brief Xóa data của player (gọi khi player disconnect).
 */
inline void ResetPlayerData(int id) {
    if (id >= 0 && id < kMaxPlayers)
        g_Players[id] = PlayerNametag{};
}
