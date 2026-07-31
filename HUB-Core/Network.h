/**
 * @file Network.h
 * @brief Giao tiếp client-server qua RakNet custom packets (RakNet 2.x embedded trong samp.dll).
 *
 * ============================================================
 * GIAO THỨC MẠNG (Protocol v4)
 * ============================================================
 *
 * ## Packet IDs
 *
 * | ID  | Tên                     | Hướng           | Mô tả                                      |
 * |-----|-------------------------|-----------------|--------------------------------------------|
 * | 220 | PACKET_NAMETAG_DATA     | Server → Client | Nametag tùy chỉnh (Icon URL + Max 2 Role Tags) |
 * | 221 | PACKET_REQUEST_DATA     | Client → Server | Client yêu cầu server resync toàn bộ data  |
 * | 222 | PACKET_SET_PRESET_ROLE  | Server → Client | Gán role cài sẵn (Admin, VIP, Mod, ...)    |
 * | 223 | PACKET_CLEAR_ROLE       | Server → Client | Xóa toàn bộ role & icon của 1 player       |
 * | 224 | PACKET_SET_ROLE_BY_NAME | Server → Client | Gán role theo tên định danh JSON (vd: ADMIN, VIP, PNG)|
 *
 * ------------------------------------------------------------
 * ## 1. PACKET_SET_ROLE_BY_NAME (ID = 224) — Server → Client
 * ------------------------------------------------------------
 * Server chỉ cần gửi Tên Role (vd: "ADMIN", "VIP") và cờ `type` (0=Text Badge, 1=PNG Image Badge).
 * Client tự đọc text, màu sắc, viền đen và tệp .png từ `HUB-Roles.json`.
 *
 * Cấu trúc:
 * ```
 * [0]       BYTE    Packet ID = 224
 * [1-2]     WORD    targetPlayerID  (0–1003)
 * [3]       BYTE    roleNameLen     (Độ dài tên role, vd: 5 cho "ADMIN")
 * [4..4+N)  char[]  roleName        (Chuỗi tên role, không null-terminated)
 * [4+N]     BYTE    type            (0 = Text Badge Only, 1 = Load PNG Image Badge từ JSON)
 * ```
 */
#pragma once
#include <cstdint>

namespace Network {

/// Enum định nghĩa các Preset Role hỗ trợ bởi packet 222
enum PresetRole : uint8_t {
    ROLE_NONE      = 0,
    ROLE_ADMIN     = 1,
    ROLE_VIP       = 2,
    ROLE_MODERATOR = 3,
    ROLE_HELPER    = 4,
    ROLE_DEVELOPER = 5
};

/// Packet IDs
constexpr uint8_t kPktNametagData   = 220; ///< Server → Client (Custom Data & Image-only)
constexpr uint8_t kPktRequestData   = 221; ///< Client → Server (Resync Request)
constexpr uint8_t kPktPresetRole    = 222; ///< Server → Client (Preset Role)
constexpr uint8_t kPktClearRole     = 223; ///< Server → Client (Clear Role)
constexpr uint8_t kPktSetRoleByName = 224; ///< Server → Client (Set Role By Name JSON)

/// Hook RakClientInterface::Receive(). Gọi sau khi SAMP connected.
void Init();

/// Gửi packet 221 để server resync toàn bộ nametag data.
void RequestData();

/// Gỡ hook. Gọi khi DLL unload.
void Shutdown();

/// true nếu hook đang active.
bool IsReady();

} // namespace Network
