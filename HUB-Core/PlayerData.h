/**
 * @file PlayerData.h
 * @brief Dữ liệu nametag động của mỗi player, nhận từ server qua RakNet.
 *
 * Server gửi packet 220 chứa:
 *   - URL ảnh icon (tải async, không lag game)
 *   - Tối đa 2 RoleTag, mỗi tag gồm: text, màu ARGB, có stroke hay không
 *
 * Thread-safety:
 *   g_Players[] được ghi bởi Network::hkReceive (game main thread)
 *   và đọc bởi Nametag::RenderAll (render thread = cùng main thread GTA SA).
 *   Không cần mutex.
 */
#pragma once
#include <string>
#include <array>
#include <d3d9.h>

/// Số player tối đa trong SAMP
constexpr int kMaxPlayers = 1004;

/// Số tag tối đa hiển thị trên một nametag (tối đa 5 roles mỗi hàng)
constexpr int kMaxTagsPerPlayer = 5;

// ---------------------------------------------------------------------------

/**
 * @brief Một role tag hiển thị trên nametag.
 *
 * Ví dụ server Pawn gửi:
 *   tag[0] = { "ADMIN", 0xFFB30000, stroke=true  }
 *   tag[1] = { "VIP",   0xFFCC9900, stroke=false }
 */
struct RoleTag {
    std::string text;           ///< Nội dung badge (vd: "ADMIN", "VIP", "MOD")
    D3DCOLOR    color  = 0;     ///< Màu nền badge dạng ARGB (0xAARRGGBB)
    bool        stroke = false; ///< true = vẽ viền đen 8 hướng quanh text
    std::string imagePath;      ///< Tệp ảnh PNG / JPG của Role (vd: "HUB-Core/icons/admin.png")
};

/**
 * @brief Toàn bộ dữ liệu nametag của 1 player, sync từ server.
 */
struct PlayerNametag {
    std::array<RoleTag, kMaxTagsPerPlayer> tags; ///< Danh sách tag (index 0-1)
    int         tagCount = 0;    ///< Số tag thực tế server gửi (0-2)
    std::string iconUrl;         ///< URL ảnh icon (rỗng = không hiển thị)
    bool        hasData  = false;///< true sau khi nhận packet đầu tiên từ server
};

/// Global array, index = playerID (0–1003)
inline std::array<PlayerNametag, kMaxPlayers> g_Players;

/**
 * @brief Xóa data của player (gọi khi player disconnect).
 */
inline void ResetPlayerData(int id) {
    if (id >= 0 && id < kMaxPlayers)
        g_Players[id] = PlayerNametag{};
}
