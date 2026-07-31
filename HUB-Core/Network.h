/**
 * @file Network.h
 * @brief Giao tiếp client-server qua RakNet custom packets/RPC.
 *
 * ============================================================
 * GIAO THỨC (Protocol)
 * ============================================================
 *
 * ## Packet IDs
 *
 * | ID  | Tên                    | Hướng          | Mô tả                              |
 * |-----|------------------------|----------------|------------------------------------|
 * | 220 | PACKET_PLAYER_ROLE     | Server → Client| Server push role data cho 1 player |
 * | 221 | PACKET_REQUEST_ROLES   | Client → Server| Client yêu cầu data tất cả player  |
 *
 * ## Cấu trúc PACKET_PLAYER_ROLE (Server → Client)
 *
 * Offset | Size | Type    | Mô tả
 * -------|------|---------|--------------------------------------
 * 0      | 1    | BYTE    | Packet ID = 220
 * 1      | 2    | WORD    | PlayerID (0-1003)
 * 3      | 1    | BYTE    | flags: bit0=isAdmin, bit1=isVIP
 * 4      | 1    | BYTE    | iconUrlLen (độ dài URL, 0 = không có)
 * 5      | N    | char[N] | iconUrl (không null-terminated)
 *
 * ## Cấu trúc PACKET_REQUEST_ROLES (Client → Server)
 *
 * Offset | Size | Type    | Mô tả
 * -------|------|---------|--------------------------------------
 * 0      | 1    | BYTE    | Packet ID = 221
 *
 * ============================================================
 * PHÍA SERVER (Pawn / SA-MP Plugin)
 * ============================================================
 *
 * ### Gửi role cho 1 player (từ Pawn):
 * ```pawn
 * // Cần SA-MP plugin (C++) làm bridge để gửi custom packet
 * // Ví dụ dùng plugin "packet_manager" hoặc tự viết:
 * native SendPlayerRoleData(toPlayerid, targetPlayerid, isAdmin, isVIP, iconUrl[]);
 *
 * // Gọi khi player login:
 * public OnPlayerConnect(playerid) {
 *     // Gửi role của tất cả player đang online cho player mới
 *     for (new i = 0; i < MAX_PLAYERS; i++) {
 *         if (IsPlayerConnected(i)) {
 *             SendPlayerRoleData(playerid, i, IsAdmin(i), IsVIP(i), GetIconUrl(i));
 *         }
 *     }
 * }
 * ```
 *
 * ### Plugin C++ gửi packet:
 * ```cpp
 * // Trong SA-MP server plugin
 * void SendRolePacket(int toPlayer, int targetPlayer,
 *                     bool isAdmin, bool isVIP, const char* iconUrl)
 * {
 *     uint8_t buf[260];
 *     int pos = 0;
 *     buf[pos++] = 220; // PACKET_PLAYER_ROLE
 *     *(uint16_t*)&buf[pos] = (uint16_t)targetPlayer; pos += 2;
 *     buf[pos++] = (isAdmin ? 1 : 0) | (isVIP ? 2 : 0);
 *     uint8_t urlLen = (uint8_t)min(strlen(iconUrl), 255u);
 *     buf[pos++] = urlLen;
 *     memcpy(&buf[pos], iconUrl, urlLen); pos += urlLen;
 *     // Gửi qua RakServer API
 *     pRakServer->Send((char*)buf, pos, HIGH_PRIORITY,
 *                      RELIABLE_ORDERED, 0,
 *                      pRakServer->GetPlayerIDFromIndex(toPlayer), false);
 * }
 * ```
 *
 * ============================================================
 * PHÍA CLIENT (DLL này)
 * ============================================================
 *
 * - Network::Init(): hook Receive() trên RakClientInterface VMT
 * - Khi nhận packet ID 220: parse và cập nhật g_Players[id]
 * - Network::RequestRoles(): gửi packet 221 lên server (yêu cầu data)
 * - Network::Shutdown(): restore hook khi DLL unload
 */
#pragma once

namespace Network {

/// Hook Receive() trên RakClientInterface. Gọi sau khi SAMP connect xong.
void Init();

/// Gửi packet 221 lên server để yêu cầu sync role data.
void RequestRoles();

/// Gỡ hook, gọi khi DLL unload.
void Shutdown();

/// Trả true nếu đã hook thành công.
bool IsReady();

} // namespace Network
