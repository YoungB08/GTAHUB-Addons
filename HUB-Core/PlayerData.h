/**
 * @file PlayerData.h
 * @brief Struct lưu dữ liệu role/badge của player nhận từ server.
 *
 * Dữ liệu này được server gửi qua custom RPC (xem Network.h).
 * Network.cpp ghi vào g_Players[], Nametag.cpp đọc để render badge.
 *
 * Thread-safety:
 *  g_Players được ghi bởi network callback (main thread của SAMP)
 *  và đọc bởi render callback (render thread = cùng main thread).
 *  Trong thực tế cả hai đều chạy trên game main thread nên không
 *  cần mutex. Nếu sau này có multi-thread đọc/ghi, thêm mutex vào.
 */
#pragma once
#include <string>
#include <array>

/// ID player tối đa trong SAMP
constexpr int kMaxPlayers = 1004;

/**
 * @brief Dữ liệu role/badge của một player.
 *
 * Server cập nhật mỗi khi player đăng nhập hoặc đổi role.
 * Không cần server gửi lại mỗi frame — dữ liệu static cho đến khi thay đổi.
 */
struct PlayerRoleData {
    bool        isAdmin   = false; ///< Player có quyền Admin
    bool        isVIP     = false; ///< Player có VIP
    std::string iconUrl;           ///< URL icon hiển thị trên nametag (rỗng = ẩn)
    bool        hasData   = false; ///< true nếu đã nhận data từ server ít nhất 1 lần
};

/// Global array, index = playerID (0-1003)
inline std::array<PlayerRoleData, kMaxPlayers> g_Players;

/**
 * @brief Reset data của player (khi player disconnect).
 * @param id PlayerID cần reset
 */
inline void ResetPlayerData(int id) {
    if (id >= 0 && id < kMaxPlayers)
        g_Players[id] = PlayerRoleData{};
}
