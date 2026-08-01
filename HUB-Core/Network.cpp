/**
 * @file Network.cpp
 * @brief Hook RakClientInterface::Receive(), parse packet 220, 222, 223, 224, 225, 226, 227.
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

constexpr int kVmtReceive          = 5;  ///< Receive()
constexpr int kVmtDeallocatePacket = 6;  ///< DeallocatePacket(Packet*)
constexpr int kVmtSendRaw          = 20; ///< Send(char* data, ...)

static tReceive s_OrigReceive = NULL;
static DWORD*   s_VMT         = NULL;
static bool     s_Ready       = false;

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

static void DeallocateRakPacket(void* pRak, RakPacket* pkt) {
    if (!pRak || !pkt) return;
    reinterpret_cast<void(__fastcall*)(void*, void*, RakPacket*)>(
        reinterpret_cast<DWORD*>(*(DWORD*)pRak)[kVmtDeallocatePacket])(pRak, nullptr, pkt);
}

// ---------------------------------------------------------------------------
// Parsers
// ---------------------------------------------------------------------------

static void ParseNametagData(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    uint8_t slotID = 0;
    if (!r.read(slotID) || slotID >= kMaxRoleSlots) return;

    PlayerNametag& pn = g_Players[playerId];
    auto& slot = pn.slots[slotID];
    slot = RoleSlotClientData{};

    uint8_t active = 0;
    if (!r.read(active)) return;
    slot.active = (active != 0);

    if (slot.active) {
        r.readString(slot.text, 63);
        uint32_t color = 0; r.read(color); slot.color = color;
        uint32_t bgColor = 0; r.read(bgColor); slot.bgColor = bgColor;
        uint8_t stroke = 0; r.read(stroke); slot.stroke = (stroke != 0);
        r.readString(slot.imagePath, 255);
    }

    pn.hasData = true;
}

static void ParsePresetRole(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    uint8_t roleType = 0; r.read(roleType);
    uint8_t slotID = 0; r.read(slotID);
    if (slotID >= kMaxRoleSlots) slotID = 0;

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
    auto& slot = pn.slots[slotID];
    slot = RoleSlotClientData{};

    if (!roleName.empty()) {
        slot.active = true;
        slot.text = roleName;
        slot.imagePath = "HUB-Core/icons/" + roleName + ".png";
        
        RoleConfig::RolePresetConfig cfg = RoleConfig::GetPresetRoleConfig(roleName);
        if (cfg.hasConfig) {
            slot.color = cfg.color;
            slot.stroke = cfg.stroke;
        } else {
            slot.color = D3DCOLOR_ARGB(255, 200, 200, 200);
            slot.stroke = true;
        }
    }

    pn.hasData = true;
}

static void ParseRoleByName(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };

    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    uint8_t slotID = 0; r.read(slotID);
    if (slotID >= kMaxRoleSlots) slotID = 0;

    std::string roleName;
    if (!r.readString(roleName, 63)) return;

    PlayerNametag& pn = g_Players[playerId];
    auto& slot = pn.slots[slotID];
    slot = RoleSlotClientData{};

    if (!roleName.empty()) {
        slot.active = true;
        slot.text = roleName;
        slot.imagePath = "HUB-Core/icons/" + roleName + ".png";
        slot.color = D3DCOLOR_ARGB(255, 200, 200, 200);
        slot.stroke = true;
    }

    pn.hasData = true;
}

static void ParseClearRole(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    int8_t slotID = -1;
    r.read(slotID);

    PlayerNametag& pn = g_Players[playerId];
    if (slotID == -1) {
        ResetPlayerData(playerId);
    } else if (slotID >= 0 && slotID < kMaxRoleSlots) {
        pn.slots[slotID] = RoleSlotClientData{};
    }
}

static void ParseSetRainbow(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    int8_t slotID = -1; r.read(slotID);
    uint8_t toggle = 0; r.read(toggle);
    uint16_t speed = 500; r.read(speed);

    PlayerNametag& pn = g_Players[playerId];
    if (slotID == -1) {
        pn.isNametagRainbow = (toggle != 0);
        pn.nametagRainbowSpeedMs = speed;
    } else if (slotID >= 0 && slotID < kMaxRoleSlots) {
        pn.slots[slotID].isRainbow = (toggle != 0);
        pn.slots[slotID].rainbowSpeedMs = speed;
    }
}

static void ParseNametagColor(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    uint32_t color = 0;
    if (!r.read(color)) return;

    PlayerNametag& pn = g_Players[playerId];
    pn.nametagColor = color;
    pn.hasCustomNametagColor = true;
}

static void ParseSetVisibility(const uint8_t* data, uint32_t len) {
    PacketReader r{ data, len };
    uint8_t  pktId   = 0; r.read(pktId);
    uint16_t playerId = 0;
    if (!r.read(playerId) || playerId >= (uint16_t)kMaxPlayers) return;

    uint8_t visible = 1;
    if (!r.read(visible)) return;

    PlayerNametag& pn = g_Players[playerId];
    pn.visible = (visible != 0);
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
            return NULL;

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

        case Network::kPktSetRainbow:
            ParseSetRainbow(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL;

        case Network::kPktNametagColor:
            ParseNametagColor(pkt->data, pkt->length);
            DeallocateRakPacket(pRak, pkt);
            return NULL;

        case Network::kPktSetVisibility:
            ParseSetVisibility(pkt->data, pkt->length);
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
