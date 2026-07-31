/**
 * @file Network.cpp
 * @brief Hook RakClientInterface::Receive, parse packet 220 (nametag data).
 */
#include "pch.h"
#include "Network.h"
#include "PlayerData.h"

#include <0.3.DL-1/CNetGame.h>
#include <cstring>
#include <cstdint>
#include <algorithm>

using namespace sampapi::v03dl;

// ---------------------------------------------------------------------------
// Protocol
// ---------------------------------------------------------------------------

constexpr uint8_t kPktNametagData  = 220; ///< Server → Client
constexpr uint8_t kPktRequestData  = 221; ///< Client → Server

// ---------------------------------------------------------------------------
// Minimal RakNet types (RakNet 2.x embedded trong samp.dll)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)
struct RakPacket {
    uint16_t systemIndex;
    char     systemAddress[6]; ///< PlayerID (IP+Port)
    uint32_t length;
    uint8_t* data;
};
#pragma pack(pop)

using tReceive = RakPacket*(__thiscall*)(void*);
using tSendRaw = bool(__thiscall*)(void*, const char*, int, int, int, char, char[6], bool);

/// VMT indices trên RakClientInterface trong SAMP 0.3.DL (RakNet 2.x)
constexpr int kVmtReceive = 5;  ///< Receive()
constexpr int kVmtSendRaw = 20; ///< Send(char* data, ...)

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static tReceive s_OrigReceive = NULL;
static DWORD*   s_VMT         = NULL;
static bool     s_Ready       = false;

// ---------------------------------------------------------------------------
// Packet reader helper
// ---------------------------------------------------------------------------

/**
 * @brief Cursor đọc byte-aligned trên buffer packet.
 * Tất cả trường trong packet đều byte-aligned nên không cần bit ops.
 */
struct PacketReader {
    const uint8_t* buf;
    uint32_t       len;
    uint32_t       pos = 0;

    bool canRead(uint32_t n) const { return pos + n <= len; }

    template<typename T>
    bool read(T& out) {
        if (!canRead(sizeof(T))) return false;
        memcpy(&out, buf + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }

    bool readString(std::string& out, uint8_t maxLen = 255) {
        uint8_t slen = 0;
        if (!read(slen)) return false;
        slen = (uint8_t)std::min((uint32_t)slen, (uint32_t)maxLen);
        if (!canRead(slen)) return false;
        out.assign(reinterpret_cast<const char*>(buf + pos), slen);
        pos += slen;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

/**
 * @brief Parse PACKET_NAMETAG_DATA (ID=220) → cập nhật g_Players[].
 *
 * Layout:
 *   [0]       BYTE   220
 *   [1-2]     WORD   targetPlayerID
 *   [3]       BYTE   iconUrlLen
 *   [4..4+N)  char[] iconUrl
 *   [4+N]     BYTE   tagCount (0-2)
 *   Per tag:
 *     BYTE    textLen
 *     char[]  text
 *     DWORD   colorARGB  (D3DCOLOR, đã convert từ Pawn RRGGBBAA bởi plugin)
 *     BYTE    stroke     (0=off, 1=on)
 */
static void ParseNametagData(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    // Bỏ qua byte đầu (packet ID đã biết)
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId)) return;
    if (playerId >= (uint16_t)kMaxPlayers) return;

    PlayerNametag& pn = g_Players[playerId];

    // iconUrl
    if (!r.readString(pn.iconUrl)) return;

    // tags
    uint8_t tagCount = 0;
    if (!r.read(tagCount)) return;
    tagCount = (uint8_t)std::min((int)tagCount, kMaxTagsPerPlayer);

    pn.tagCount = 0;
    for (int i = 0; i < tagCount; i++) {
        RoleTag& tag = pn.tags[i];

        if (!r.readString(tag.text, 63)) return;

        uint32_t color = 0;
        if (!r.read(color)) return;
        tag.color = static_cast<D3DCOLOR>(color);

        uint8_t stroke = 0;
        if (!r.read(stroke)) return;
        tag.stroke = (stroke != 0);

        pn.tagCount++;
    }

    pn.hasData = true;
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

static RakPacket* __thiscall hkReceive(void* pRak) {
    RakPacket* pkt = s_OrigReceive(pRak);
    if (!pkt || !pkt->data || pkt->length == 0) return pkt;

    switch (pkt->data[0]) {
        case kPktNametagData:
            ParseNametagData(pkt->data, pkt->length);
            // Deallocate packet — VMT[6] = DeallocatePacket(Packet*)
            reinterpret_cast<void(__thiscall*)(void*, RakPacket*)>(
                reinterpret_cast<DWORD*>(*(DWORD*)pRak)[6])(pRak, pkt);
            return NULL; // SAMP không thấy packet này

        default:
            return pkt;
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

    s_VMT = *reinterpret_cast<DWORD**>(pRak);
    s_OrigReceive = reinterpret_cast<tReceive>(s_VMT[kVmtReceive]);

    DWORD old;
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &old);
    s_VMT[kVmtReceive] = reinterpret_cast<DWORD>(hkReceive);
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), old, &old);

    s_Ready = true;
}

void Network::RequestData() {
    if (!s_Ready) return;

    CNetGame* pNet = RefNetGame();
    if (!pNet) return;
    void* pRak = pNet->GetRakClient();
    if (!pRak) return;

    uint8_t buf[1] = { kPktRequestData };
    char serverAddr[6] = {};
    auto fnSend = reinterpret_cast<tSendRaw>(s_VMT[kVmtSendRaw]);
    fnSend(pRak, reinterpret_cast<const char*>(buf), 1,
        2 /*HIGH_PRIORITY*/, 3 /*RELIABLE_ORDERED*/, 0, serverAddr, false);
}

void Network::Shutdown() {
    if (!s_Ready || !s_VMT) return;
    DWORD old;
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &old);
    s_VMT[kVmtReceive] = reinterpret_cast<DWORD>(s_OrigReceive);
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), old, &old);
    s_Ready = false; s_OrigReceive = NULL; s_VMT = NULL;
}

bool Network::IsReady() { return s_Ready; }
