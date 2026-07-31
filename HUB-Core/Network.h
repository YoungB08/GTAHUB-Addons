/**
 * @file Network.h
 * @brief Giao tiếp client-server qua RakNet custom packets.
 *
 * ============================================================
 * GIAO THỨC (Protocol v2)
 * ============================================================
 *
 * ## Packet IDs
 *
 * | ID  | Tên                  | Hướng           | Mô tả                                |
 * |-----|----------------------|-----------------|--------------------------------------|
 * | 220 | PACKET_NAMETAG_DATA  | Server → Client | Toàn bộ nametag data của 1 player    |
 * | 221 | PACKET_REQUEST_DATA  | Client → Server | Client yêu cầu server gửi lại data   |
 *
 * ------------------------------------------------------------
 * ## PACKET_NAMETAG_DATA (ID = 220) — Server → Client
 * ------------------------------------------------------------
 *
 * ```
 * [0]       BYTE    Packet ID = 220
 * [1-2]     WORD    targetPlayerID  (player được gắn tag, 0–1003)
 * [3]       BYTE    iconUrlLen      (0 = không có icon)
 * [4..4+N)  char[]  iconUrl         (không null-terminated)
 * [4+N]     BYTE    tagCount        (số tag, 0–2)
 *
 * Lặp tagCount lần:
 *   BYTE    textLen             (độ dài text badge)
 *   char[]  text                (không null-terminated)
 *   DWORD   colorARGB           (màu nền, 0xAARRGGBB — big-endian Pawn)
 *   BYTE    stroke              (0 = không stroke, 1 = stroke đen 8 hướng)
 * ```
 *
 * **Lưu ý colorARGB từ Pawn:**
 * Pawn truyền màu dạng `0xRRGGBBAA` (Pawn convention).
 * Plugin C++ phải đổi sang `0xAARRGGBB` (D3DCOLOR) trước khi gửi.
 * Xem ví dụ hàm `PawnColorToD3D()` bên dưới.
 *
 * ------------------------------------------------------------
 * ## PACKET_REQUEST_DATA (ID = 221) — Client → Server
 * ------------------------------------------------------------
 *
 * ```
 * [0]  BYTE  Packet ID = 221
 * ```
 * Server nhận → gửi lại PACKET_NAMETAG_DATA cho tất cả player đang online.
 *
 * ============================================================
 * PHÍA SERVER — SA-MP Plugin (C++)
 * ============================================================
 *
 * ### Hàm tiện ích:
 * ```cpp
 * // Đổi màu từ Pawn (RRGGBBAA) sang D3DCOLOR (AARRGGBB)
 * inline uint32_t PawnColorToD3D(uint32_t pawnColor) {
 *     uint8_t r = (pawnColor >> 24) & 0xFF;
 *     uint8_t g = (pawnColor >> 16) & 0xFF;
 *     uint8_t b = (pawnColor >>  8) & 0xFF;
 *     uint8_t a = (pawnColor >>  0) & 0xFF;
 *     return (a << 24) | (r << 16) | (g << 8) | b; // D3DCOLOR_ARGB
 * }
 *
 * // Gửi toàn bộ nametag data cho 1 client
 * void SendNametagData(RakServerInterface* pRak, int toPlayer, int targetPlayer,
 *                      const char* iconUrl,
 *                      const RoleTagData* tags, int tagCount)
 * {
 *     uint8_t buf[512];
 *     int pos = 0;
 *
 *     buf[pos++] = 220;                                    // packet ID
 *     *(uint16_t*)&buf[pos] = (uint16_t)targetPlayer; pos += 2;
 *
 *     uint8_t urlLen = (uint8_t)min(strlen(iconUrl), 255u);
 *     buf[pos++] = urlLen;
 *     memcpy(&buf[pos], iconUrl, urlLen); pos += urlLen;
 *
 *     int count = min(tagCount, 2);
 *     buf[pos++] = (uint8_t)count;
 *     for (int i = 0; i < count; i++) {
 *         uint8_t tlen = (uint8_t)min(strlen(tags[i].text), 63u);
 *         buf[pos++] = tlen;
 *         memcpy(&buf[pos], tags[i].text, tlen); pos += tlen;
 *         uint32_t d3dColor = PawnColorToD3D(tags[i].pawnColor);
 *         *(uint32_t*)&buf[pos] = d3dColor; pos += 4;
 *         buf[pos++] = tags[i].stroke ? 1 : 0;
 *     }
 *
 *     PlayerID pid = pRak->GetPlayerIDFromIndex(toPlayer);
 *     pRak->Send((char*)buf, pos, HIGH_PRIORITY, RELIABLE_ORDERED, 0, pid, false);
 * }
 * ```
 *
 * ============================================================
 * PHÍA SERVER — Pawn (gamemode)
 * ============================================================
 *
 * ```pawn
 * // Native được export bởi server plugin C++:
 * native SendPlayerRoleData(toPlayerid, targetPlayerid, imgUrl[],
 *                           tag1Text[]="", tag1Color=0, tag1Stroke=0,
 *                           tag2Text[]="", tag2Color=0, tag2Stroke=0);
 *
 * // Gửi khi player login:
 * public OnPlayerLogin(playerid) {
 *     new imgUrl[128];
 *     format(imgUrl, sizeof(imgUrl), "https://cdn.example.com/avatar/%d.png", playerid);
 *
 *     if (IsAdmin(playerid)) {
 *         SendPlayerRoleData(
 *             playerid, playerid, imgUrl,
 *             "ADMIN", 0xB30000FF, 1,   // tag 0: đỏ, có stroke
 *             "VIP",   0xCC9900FF, 0    // tag 1: vàng, không stroke
 *         );
 *     } else if (IsVIP(playerid)) {
 *         SendPlayerRoleData(
 *             playerid, playerid, imgUrl,
 *             "VIP", 0xCC9900FF, 0      // tag 0 duy nhất
 *         );
 *     } else {
 *         SendPlayerRoleData(playerid, playerid, imgUrl); // chỉ icon, không tag
 *     }
 * }
 *
 * // Broadcast cho tất cả khi 1 player mới join:
 * public OnPlayerConnect(playerid) {
 *     for (new i = 0; i < MAX_PLAYERS; i++) {
 *         if (IsPlayerConnected(i) && i != playerid) {
 *             // Gửi data của player mới cho player cũ
 *             SendPlayerRoleData(i, playerid, GetImgUrl(playerid), ...);
 *             // Gửi data của player cũ cho player mới
 *             SendPlayerRoleData(playerid, i, GetImgUrl(i), ...);
 *         }
 *     }
 * }
 * ```
 */
#pragma once

namespace Network {

/// Hook RakClientInterface::Receive(). Gọi sau khi SAMP connected.
void Init();

/// Gửi packet 221 để server resync toàn bộ nametag data.
void RequestData();

/// Gỡ hook. Gọi khi DLL unload.
void Shutdown();

/// true nếu hook đang active.
bool IsReady();

} // namespace Network
