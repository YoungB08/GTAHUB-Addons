/**
 * @file Network.cpp
 * @brief Implementation RakNet custom packet handler.
 *
 * Kỹ thuật hook:
 *  RakClientInterface là abstract class — mọi method đều là virtual.
 *  Ta patch VMT[5] (Receive) để intercept tất cả packet đến.
 *  Packet không phải của ta → forward nguyên cho original Receive.
 *
 * VMT index của RakClientInterface trong SAMP 0.3.DL:
 *  [5]  = Receive()            ← hook tại đây để đọc packet đến
 *  [21] = Send(BitStream*)     ← dùng để gửi packet lên server
 *
 * Lưu ý thread safety:
 *  Receive() được gọi từ game main thread trong CNetGame::Process().
 *  g_Players[] được đọc từ render thread (EndScene).
 *  Vì GTA SA single-threaded main loop, cả hai đều chạy sequential
 *  nên không cần mutex cho g_Players.
 */
#include "pch.h"
#include "Network.h"
#include "PlayerData.h"

#include <0.3.DL-1/CNetGame.h>
#include <cstring>
#include <cstdint>

using namespace sampapi::v03dl;

// ---------------------------------------------------------------------------
// Protocol constants (phải khớp với server plugin)
// ---------------------------------------------------------------------------

constexpr uint8_t kPacketPlayerRole   = 220; ///< Server → Client: role data
constexpr uint8_t kPacketRequestRoles = 221; ///< Client → Server: yêu cầu data

// ---------------------------------------------------------------------------
// Minimal RakNet types
// Không có header RakNet — dùng forward declarations tối thiểu.
// Cấu trúc Packet lấy từ RakNet 2.x (version SAMP 0.3.DL dùng).
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct RakPacket {
    uint16_t     systemIndex;  ///< Index trong connection list
    char         systemAddress[6]; ///< IP+Port (PlayerID trong RakNet 2.x)
    uint32_t     length;       ///< Tổng số byte của data
    uint8_t*     data;         ///< Payload (data[0] = packet ID)
};
#pragma pack(pop)

/// Signature của RakClientInterface::Receive()
using tReceive = RakPacket*(__thiscall*)(void*);

/// Signature của RakClientInterface::Send(char* data, int length, ...)
/// VMT[20] = Send(char*, int, PacketPriority, PacketReliability, char, PlayerID, bool)
using tSend = bool(__thiscall*)(void*, const char*, int, int, int, char, char[6], bool);

// ---------------------------------------------------------------------------
// VMT indices cho RakClientInterface trong SAMP 0.3.DL
// (Dựa trên RakNet 2.x embedded trong samp.dll)
// ---------------------------------------------------------------------------

constexpr int kVmtReceive = 5;  ///< Receive() — nhận packet
constexpr int kVmtSend    = 20; ///< Send(char*,...) — gửi raw bytes

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static tReceive  s_OrigReceive = NULL;
static DWORD*    s_RakVMT      = NULL;
static bool      s_Ready       = false;

// ---------------------------------------------------------------------------
// Packet parser
// ---------------------------------------------------------------------------

/**
 * @brief Parse PACKET_PLAYER_ROLE (ID=220) và cập nhật g_Players[].
 *
 * Layout:
 *   [0]    BYTE   = 220
 *   [1-2]  WORD   = playerID
 *   [3]    BYTE   = flags (bit0=admin, bit1=vip)
 *   [4]    BYTE   = iconUrlLen
 *   [5..N] char[] = iconUrl
 */
static void HandlePlayerRole(const uint8_t* data, uint32_t len) {
    // Minimum: 1 (id) + 2 (playerid) + 1 (flags) + 1 (urllen) = 5 bytes
    if (len < 5) return;

    uint16_t playerId  = *reinterpret_cast<const uint16_t*>(&data[1]);
    uint8_t  flags     = data[3];
    uint8_t  urlLen    = data[4];

    if (playerId >= kMaxPlayers)          return;
    if (5u + urlLen > len)                return; // tránh out-of-bounds

    PlayerRoleData& pd = g_Players[playerId];
    pd.isAdmin  = (flags & 0x01) != 0;
    pd.isVIP    = (flags & 0x02) != 0;
    pd.iconUrl  = urlLen > 0
        ? std::string(reinterpret_cast<const char*>(&data[5]), urlLen)
        : std::string{};
    pd.hasData  = true;
}

// ---------------------------------------------------------------------------
// Hook function
// ---------------------------------------------------------------------------

/**
 * @brief Intercept tất cả packet đến từ server.
 *
 * - Nếu packet ID là của ta → xử lý rồi DeallocatePacket + return NULL
 *   (trả NULL để SAMP không xử lý tiếp packet đó).
 * - Còn lại → forward cho original Receive bình thường.
 */
static RakPacket* __thiscall hkReceive(void* pRak) {
    RakPacket* pkt = s_OrigReceive(pRak);
    if (!pkt || !pkt->data || pkt->length == 0) return pkt;

    switch (pkt->data[0]) {
        case kPacketPlayerRole:
            HandlePlayerRole(pkt->data, pkt->length);
            // Không để SAMP thấy packet này — giải phóng và trả NULL
            // DeallocatePacket ở VMT[6]
            reinterpret_cast<void(__thiscall*)(void*, RakPacket*)>(
                reinterpret_cast<DWORD*>(*(DWORD*)pRak)[6])(pRak, pkt);
            return NULL;

        default:
            return pkt; // forward bình thường
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Network::Init() {
    if (s_Ready) return;

    CNetGame* pNet = RefNetGame();
    if (!pNet) return;

    void* pRak = pNet->GetRakClient();
    if (!pRak) return;

    // Lấy VMT của RakClientInterface
    s_RakVMT = *reinterpret_cast<DWORD**>(pRak);

    // Lưu original Receive
    s_OrigReceive = reinterpret_cast<tReceive>(s_RakVMT[kVmtReceive]);

    // Patch VMT
    DWORD old;
    VirtualProtect(&s_RakVMT[kVmtReceive], sizeof(DWORD),
        PAGE_EXECUTE_READWRITE, &old);
    s_RakVMT[kVmtReceive] = reinterpret_cast<DWORD>(hkReceive);
    VirtualProtect(&s_RakVMT[kVmtReceive], sizeof(DWORD), old, &old);

    s_Ready = true;
}

void Network::RequestRoles() {
    if (!s_Ready) return;

    CNetGame* pNet = RefNetGame();
    if (!pNet) return;

    void* pRak = pNet->GetRakClient();
    if (!pRak) return;

    // Gửi packet 221 lên server
    uint8_t buf[1] = { kPacketRequestRoles };

    // Send(char* data, int len, priority=2, reliability=3, orderingChannel=0, playerId, broadcast=false)
    // Dùng SystemAddress của server = GetServerID() — VMT[10] trong RakNet 2.x
    // Đơn giản hơn: dùng RPC nếu server có handler, nhưng ở đây ta dùng raw send
    auto fnSend = reinterpret_cast<tSend>(s_RakVMT[kVmtSend]);
    char serverAddr[6] = {}; // empty = server (RakNet 2.x convention)
    fnSend(pRak, reinterpret_cast<const char*>(buf), 1,
        2 /*HIGH_PRIORITY*/, 3 /*RELIABLE_ORDERED*/, 0, serverAddr, false);
}

void Network::Shutdown() {
    if (!s_Ready || !s_RakVMT) return;

    DWORD old;
    VirtualProtect(&s_RakVMT[kVmtReceive], sizeof(DWORD),
        PAGE_EXECUTE_READWRITE, &old);
    s_RakVMT[kVmtReceive] = reinterpret_cast<DWORD>(s_OrigReceive);
    VirtualProtect(&s_RakVMT[kVmtReceive], sizeof(DWORD), old, &old);

    s_Ready       = false;
    s_OrigReceive = NULL;
    s_RakVMT      = NULL;
}

bool Network::IsReady() {
    return s_Ready;
}
