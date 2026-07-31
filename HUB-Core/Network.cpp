/**
 * @file Network.cpp
 * @brief Hook RakClientInterface::Receive, parse packet 220, 222, 223.
 */
#include "pch.h"
#include "Network.h"
#include "PlayerData.h"
#include "RoleConfig.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <cstring>
#include <cstdint>
#include <algorithm>

using namespace sampapi::v03dl;

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
constexpr int kVmtReceive          = 5;  ///< Receive()
constexpr int kVmtDeallocatePacket = 6;  ///< DeallocatePacket(Packet*)
constexpr int kVmtSendRaw          = 20; ///< Send(char* data, ...)

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static tReceive s_OrigReceive = NULL;
static DWORD*   s_VMT         = NULL;
static bool     s_Ready       = false;

// ---------------------------------------------------------------------------
// Packet reader helper
// ---------------------------------------------------------------------------

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
        slen = (uint8_t)(std::min)((uint32_t)slen, (uint32_t)maxLen);
        if (!canRead(slen)) return false;
        out.assign(reinterpret_cast<const char*>(buf + pos), slen);
        pos += slen;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Free Packet Helper
// ---------------------------------------------------------------------------

static void DeallocateRakPacket(void* pRak, RakPacket* pkt) {
    if (!pRak || !pkt) return;
    reinterpret_cast<void(__fastcall*)(void*, void*, RakPacket*)>(
        reinterpret_cast<DWORD*>(*(DWORD*)pRak)[kVmtDeallocatePacket])(pRak, nullptr, pkt);
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

/**
 * @brief Parse PACKET_NAMETAG_DATA (ID=220) → cập nhật g_Players[].
 */
static void ParseNametagData(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

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
    tagCount = (uint8_t)(std::min)((int)tagCount, kMaxTagsPerPlayer);

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

/**
 * @brief Parse PACKET_SET_PRESET_ROLE (ID=222) → gán role cài sẵn (Admin, VIP, Mod...).
 */
static void ParsePresetRole(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId)) return;
    if (playerId >= (uint16_t)kMaxPlayers) return;

    uint8_t roleType = 0;
    if (!r.read(roleType)) return;

    uint8_t useSvg = 0;
    r.read(useSvg); // 0 = Text Badge Only, 1 = Load SVG Icon từ JSON

    std::string roleName;
    switch (roleType) {
        case Network::ROLE_ADMIN:     roleName = "ADMIN"; break;
        case Network::ROLE_VIP:       roleName = "VIP"; break;
        case Network::ROLE_MODERATOR: roleName = "MOD"; break;
        case Network::ROLE_HELPER:    roleName = "HELPER"; break;
        case Network::ROLE_DEVELOPER: roleName = "DEV"; break;
        default: break;
    }

    PlayerNametag& pn = g_Players[playerId];
    pn.tagCount = 0;
    pn.iconUrl.clear();

    if (!roleName.empty()) {
        RoleConfig::RolePresetConfig cfg = RoleConfig::GetPresetRoleConfig(roleName);
        if (cfg.hasConfig) {
            pn.tags[0] = { cfg.text, cfg.color, cfg.stroke };
            pn.tagCount = 1;
            if (useSvg != 0 && !cfg.svgPath.empty()) {
                pn.iconUrl = cfg.svgPath;
            }
        }
    }

    pn.hasData = true;
}

/**
 * @brief Parse PACKET_SET_ROLE_BY_NAME (ID=224) → Nạp Role theo tên định danh JSON.
 */
static void ParseRoleByName(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId)) return;
    if (playerId >= (uint16_t)kMaxPlayers) return;

    std::string roleName;
    if (!r.readString(roleName, 63)) return;

    uint8_t useSvg = 0;
    r.read(useSvg); // 0 = Text Badge Only, 1 = Load SVG Icon từ JSON

    PlayerNametag& pn = g_Players[playerId];
    pn.tagCount = 0;
    pn.iconUrl.clear();

    RoleConfig::RolePresetConfig cfg = RoleConfig::GetPresetRoleConfig(roleName);
    if (cfg.hasConfig) {
        pn.tags[0] = { cfg.text, cfg.color, cfg.stroke };
        pn.tagCount = 1;

        if (useSvg != 0 && !cfg.svgPath.empty()) {
            pn.iconUrl = cfg.svgPath;
        }
    } else {
        pn.tags[0] = { roleName, D3DCOLOR_ARGB(255, 200, 200, 200), false };
        pn.tagCount = 1;
        if (useSvg != 0) {
            pn.iconUrl = "HUB-Core/icons/" + roleName + ".svg";
        }
    }

    pn.hasData = true;
}

/**
 * @brief Parse PACKET_CLEAR_ROLE (ID=223) → xóa sạch role của player.
 */
static void ParseClearRole(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId)) return;
    if (playerId < (uint16_t)kMaxPlayers) {
        ResetPlayerData(playerId);
    }
}

// ---------------------------------------------------------------------------
// Hook
// ---------------------------------------------------------------------------

static RakPacket* __fastcall hkReceive(void* pRak, void* edx) {
    RakPacket* pkt = s_OrigReceive(pRak);
    if (!pkt || !pkt->data || pkt->length == 0) return pkt;

    switch (pkt->data[0]) {
        case Network::kPktNametagData:
            ParseNametagData(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL; // Hide packet from SAMP

        case Network::kPktPresetRole:
            ParsePresetRole(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL;

        case Network::kPktClearRole:
            ParseClearRole(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL;

        case Network::kPktSetRoleByName:
            ParseRoleByName(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL;

        default:
            return pkt;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Network::Init() {
    if (s_Ready) return;

    CNetGame* pNet = GetRefNetGame();
    if (!pNet) return;

    SAMPVersionInfo vInfo = SAMPVersionInfo::Get();
    if (!vInfo.fnGetRakClient) return;

    void* pRak = reinterpret_cast<void*(__thiscall*)(void*)>(vInfo.fnGetRakClient)(pNet);
    if (!pRak || IsBadReadPtr(pRak, sizeof(void*))) return;

    s_VMT = *reinterpret_cast<DWORD**>(pRak);
    if (!s_VMT || IsBadReadPtr(s_VMT, sizeof(void*) * 10)) return;

    s_OrigReceive = reinterpret_cast<tReceive>(s_VMT[kVmtReceive]);

    DWORD old;
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &old);
    s_VMT[kVmtReceive] = reinterpret_cast<DWORD>(hkReceive);
    VirtualProtect(&s_VMT[kVmtReceive], sizeof(DWORD), old, &old);

    s_Ready = true;
    Log("Network::Init succeeded! pRak=%p", pRak);
}

void Network::RequestData() {
    if (!s_Ready) return;

    CNetGame* pNet = GetRefNetGame();
    if (!pNet) return;

    SAMPVersionInfo vInfo = SAMPVersionInfo::Get();
    if (!vInfo.fnGetRakClient) return;

    void* pRak = reinterpret_cast<void*(__thiscall*)(void*)>(vInfo.fnGetRakClient)(pNet);
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
