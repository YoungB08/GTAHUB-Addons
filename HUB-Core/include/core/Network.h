/**
 * @file Network.h
 * @brief Giao tiếp client-server qua RakNet custom binary packets trong SAMP 0.3.DL / open:mp.
 * Hỗ trợ 3 Multi-Slot, Rainbow Effects, Custom Nametag Color và Visibility Toggle.
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
constexpr uint8_t kPktNametagData   = 220; ///< Server → Client (Full Slot Data)
constexpr uint8_t kPktRequestData   = 221; ///< Client → Server (Resync Request)
constexpr uint8_t kPktPresetRole    = 222; ///< Server → Client (Preset Role)
constexpr uint8_t kPktClearRole     = 223; ///< Server → Client (Clear Role / Slot)
constexpr uint8_t kPktSetRoleByName = 224; ///< Server → Client (Set Role By Name)
constexpr uint8_t kPktSetRainbow    = 225; ///< Server → Client (Set Rainbow Effect)
constexpr uint8_t kPktNametagColor  = 226; ///< Server → Client (Set Custom Nametag Color)
constexpr uint8_t kPktSetVisibility = 227; ///< Server → Client (Set Role Visibility / Undercover)

constexpr uint8_t kPktChatChannel   = 228;
constexpr uint8_t kPktChatRemove    = 229;
constexpr uint8_t kPktChatMessage   = 230;
constexpr uint8_t kPktChatActive    = 231;
constexpr uint8_t kPktChatClear     = 232;

/// Hook RakClientInterface::Receive().
void Init();

/// Gỡ hook. Gọi khi DLL unload.
void Shutdown();

/// true nếu hook đang active.
bool IsReady();

/// Gửi trực tiếp packet chat 101 (UTF-8) qua RakNet bypass filter mặc định của samp.dll.
bool SendChatPacket(const char* text);

} // namespace Network
