#include "pch.h"
#include "Network.h"

#include "PlayerData.h"
#include "RoleConfig.h"

#include <sampapi/0.3.DL-1/CNetGame.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

using namespace sampapi::v03dl;

namespace {

struct RakPlayerId {
    uint32_t binaryAddress = 0xFFFFFFFF;
    uint16_t port = 0xFFFF;
    uint16_t padding = 0;
};

struct RakPacket {
    RakPlayerId playerId;
    uint32_t length;
    uint32_t bitSize;
    uint8_t* data;
    bool deleteData;
};

using ReceiveFn = RakPacket*(__thiscall*)(void*);
using DeallocatePacketFn = void(__thiscall*)(void*, RakPacket*);

constexpr size_t kReceiveIndex = 8;
constexpr size_t kDeallocatePacketIndex = 9;

ReceiveFn g_OriginalReceive = nullptr;
DeallocatePacketFn g_DeallocatePacket = nullptr;
DWORD* g_RakVmt = nullptr;
void* g_RakClient = nullptr;
std::atomic<bool> g_Ready{false};

bool IsReadable(const void* address, size_t size) {
    if (!address || reinterpret_cast<uintptr_t>(address) < 0x10000 || size == 0) return false;

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + size;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool WriteVmtEntry(DWORD* vmt, size_t index, DWORD value) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(&vmt[index], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    vmt[index] = value;
    FlushInstructionCache(GetCurrentProcess(), &vmt[index], sizeof(DWORD));
    DWORD unusedProtect = 0;
    VirtualProtect(&vmt[index], sizeof(DWORD), oldProtect, &unusedProtect);
    return true;
}

class PacketReader {
public:
    PacketReader(const uint8_t* data, size_t length) : data_(data), length_(length) {}

    template <typename T>
    bool Read(T& value) {
        if (!CanRead(sizeof(T))) return false;
        std::memcpy(&value, data_ + position_, sizeof(T));
        position_ += sizeof(T);
        return true;
    }

    bool ReadString(std::string& value, size_t maximumLength) {
        uint8_t encodedLength = 0;
        if (!Read(encodedLength) || encodedLength > maximumLength || !CanRead(encodedLength)) return false;
        value.assign(reinterpret_cast<const char*>(data_ + position_), encodedLength);
        position_ += encodedLength;
        return true;
    }

private:
    bool CanRead(size_t count) const {
        return position_ <= length_ && count <= length_ - position_;
    }

    const uint8_t* data_ = nullptr;
    size_t length_ = 0;
    size_t position_ = 0;
};

bool ReadHeader(PacketReader& reader, uint8_t expectedPacket, uint16_t& playerId) {
    uint8_t packetId = 0;
    return reader.Read(packetId) && packetId == expectedPacket &&
        reader.Read(playerId) && playerId < kMaxPlayers;
}

void ParseNametagData(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    uint8_t slotId = 0;
    uint8_t active = 0;
    if (!ReadHeader(reader, Network::kPktNametagData, playerId) ||
        !reader.Read(slotId) || slotId >= kMaxRoleSlots || !reader.Read(active)) {
        return;
    }

    RoleSlotClientData slot{};
    slot.active = active != 0;
    if (slot.active) {
        uint8_t stroke = 0;
        if (!reader.Read(slot.color) || !reader.Read(slot.bgColor) || !reader.Read(stroke) ||
            !reader.ReadString(slot.text, 63) || !reader.ReadString(slot.imagePath, 255)) {
            return;
        }
        slot.stroke = stroke != 0;
    }

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    g_Players[playerId].slots[slotId] = std::move(slot);
    g_Players[playerId].hasData = true;
    Log("Role packet 220 received: player=%u slot=%u active=%u bytes=%u",
        playerId, slotId, active, static_cast<unsigned>(length));
}

void ParsePresetRole(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    uint8_t slotId = 0;
    uint8_t roleType = 0;
    if (!ReadHeader(reader, Network::kPktPresetRole, playerId) ||
        !reader.Read(slotId) || slotId >= kMaxRoleSlots || !reader.Read(roleType)) {
        return;
    }

    const char* roleName = nullptr;
    switch (roleType) {
        case Network::ROLE_ADMIN: roleName = "ADMIN"; break;
        case Network::ROLE_VIP: roleName = "VIP"; break;
        case Network::ROLE_MODERATOR: roleName = "MOD"; break;
        case Network::ROLE_HELPER: roleName = "HELPER"; break;
        case Network::ROLE_DEVELOPER: roleName = "DEV"; break;
        default: break;
    }

    RoleSlotClientData slot{};
    if (roleName) {
        const RoleConfig::RolePresetConfig config = RoleConfig::GetPresetRoleConfig(roleName);
        slot.active = true;
        slot.text = config.hasConfig ? config.text : roleName;
        slot.color = config.hasConfig ? config.color : D3DCOLOR_ARGB(255, 200, 200, 200);
        slot.stroke = config.hasConfig ? config.stroke : true;
        slot.imagePath = config.hasConfig ? config.imagePath : "HUB-Core/icons/" + std::string(roleName) + ".png";
    }

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    g_Players[playerId].slots[slotId] = std::move(slot);
    g_Players[playerId].hasData = true;
}

void ParseClearRole(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    int8_t slotId = -1;
    if (!ReadHeader(reader, Network::kPktClearRole, playerId) || !reader.Read(slotId)) return;

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    if (slotId == -1) {
        for (RoleSlotClientData& slot : g_Players[playerId].slots) slot = RoleSlotClientData{};
    } else if (slotId >= 0 && slotId < kMaxRoleSlots) {
        g_Players[playerId].slots[slotId] = RoleSlotClientData{};
    }
}

void ParseRainbow(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    int8_t slotId = -1;
    uint8_t enabled = 0;
    uint32_t speedMs = 500;
    if (!ReadHeader(reader, Network::kPktSetRainbow, playerId) || !reader.Read(slotId) ||
        !reader.Read(enabled) || !reader.Read(speedMs)) {
        return;
    }
    speedMs = (std::max)(speedMs, 50u);

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    PlayerNametag& nametag = g_Players[playerId];
    if (slotId == -1) {
        nametag.isNametagRainbow = enabled != 0;
        nametag.nametagRainbowSpeedMs = speedMs;
    } else if (slotId >= 0 && slotId < kMaxRoleSlots) {
        nametag.slots[slotId].isRainbow = enabled != 0;
        nametag.slots[slotId].rainbowSpeedMs = speedMs;
    }
}

void ParseNametagColor(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    uint32_t color = 0;
    if (!ReadHeader(reader, Network::kPktNametagColor, playerId) || !reader.Read(color)) return;

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    g_Players[playerId].nametagColor = color;
    g_Players[playerId].hasCustomNametagColor = true;
}

void ParseVisibility(const uint8_t* data, size_t length) {
    PacketReader reader(data, length);
    uint16_t playerId = 0;
    uint8_t visible = 1;
    if (!ReadHeader(reader, Network::kPktSetVisibility, playerId) || !reader.Read(visible)) return;

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    g_Players[playerId].visible = visible != 0;
}

bool HandlePacket(const RakPacket* packet) {
    if (!packet || !IsReadable(packet, sizeof(RakPacket)) || packet->length == 0 ||
        packet->length > 4096 || !IsReadable(packet->data, packet->length)) {
        return false;
    }

    switch (packet->data[0]) {
        case Network::kPktNametagData: ParseNametagData(packet->data, packet->length); return true;
        case Network::kPktPresetRole: ParsePresetRole(packet->data, packet->length); return true;
        case Network::kPktClearRole: ParseClearRole(packet->data, packet->length); return true;
        case Network::kPktSetRainbow: ParseRainbow(packet->data, packet->length); return true;
        case Network::kPktNametagColor: ParseNametagColor(packet->data, packet->length); return true;
        case Network::kPktSetVisibility: ParseVisibility(packet->data, packet->length); return true;
        default: return false;
    }
}

RakPacket* __fastcall HookedReceive(void* rakClient, void*) {
    if (!g_OriginalReceive) return nullptr;

    RakPacket* packet = g_OriginalReceive(rakClient);
    if (!packet || !HandlePacket(packet)) return packet;

    if (g_DeallocatePacket) g_DeallocatePacket(rakClient, packet);
    return nullptr;
}

} // namespace

void Network::Init() {
    if (g_Ready.load(std::memory_order_acquire)) return;

    CNetGame* netGame = GetRefNetGame();
    const SAMPVersionInfo version = SAMPVersionInfo::Get();
    if (!netGame || !version.fnGetRakClient) return;

    void* rakClient = reinterpret_cast<void*(__thiscall*)(void*)>(version.fnGetRakClient)(netGame);
    if (!IsReadable(rakClient, sizeof(void*))) return;

    DWORD** vmtPointer = reinterpret_cast<DWORD**>(rakClient);
    if (!IsReadable(vmtPointer, sizeof(*vmtPointer)) ||
        !IsReadable(*vmtPointer, sizeof(DWORD) * (kDeallocatePacketIndex + 1))) {
        return;
    }

    DWORD* vmt = *vmtPointer;
    ReceiveFn originalReceive = reinterpret_cast<ReceiveFn>(vmt[kReceiveIndex]);
    if (!originalReceive) return;

    g_RakClient = rakClient;
    g_RakVmt = vmt;
    g_OriginalReceive = originalReceive;
    g_DeallocatePacket = reinterpret_cast<DeallocatePacketFn>(vmt[kDeallocatePacketIndex]);

    if (!WriteVmtEntry(vmt, kReceiveIndex, reinterpret_cast<DWORD>(&HookedReceive))) {
        g_RakClient = nullptr;
        g_RakVmt = nullptr;
        g_OriginalReceive = nullptr;
        g_DeallocatePacket = nullptr;
        return;
    }

    g_Ready.store(true, std::memory_order_release);
    Log("RakNet Receive hook installed: client=%p", rakClient);
}

void Network::Shutdown() {
    if (g_Ready.exchange(false, std::memory_order_acq_rel) && g_RakVmt &&
        g_RakVmt[kReceiveIndex] == reinterpret_cast<DWORD>(&HookedReceive) && g_OriginalReceive) {
        WriteVmtEntry(g_RakVmt, kReceiveIndex, reinterpret_cast<DWORD>(g_OriginalReceive));
    }

    g_RakClient = nullptr;
    g_RakVmt = nullptr;
    g_OriginalReceive = nullptr;
    g_DeallocatePacket = nullptr;
}

bool Network::IsReady() {
    return g_Ready.load(std::memory_order_acquire);
}
