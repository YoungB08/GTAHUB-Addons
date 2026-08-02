#include "../include/RoleComponent.hpp"
#include "../include/amx/amx.h"

#include <cstring>
#include <climits>

namespace HUBRole {

static cell* amx_GetAddress(AMX* amx, cell paramAddress) {
    if (!amx || !amx->base || paramAddress < 0 || paramAddress >= amx->stp) return nullptr;
    const auto* header = reinterpret_cast<const AMX_HEADER*>(amx->base);
    uint8_t* data = amx->data ? amx->data : amx->base + header->dat;
    return reinterpret_cast<cell*>(data + paramAddress);
}

static std::string amx_GetStringParam(AMX* amx, cell paramAddress) {
    cell* address = amx_GetAddress(amx, paramAddress);
    if (!address) return {};

    constexpr size_t maxLength = 255;
    std::string value;
    value.reserve(32);
    if (static_cast<ucell>(*address) > UCHAR_MAX) {
        for (size_t index = 0; value.size() < maxLength; ++index) {
            const ucell packed = static_cast<ucell>(address[index]);
            for (int shift = static_cast<int>(sizeof(cell) * CHAR_BIT - CHAR_BIT); shift >= 0; shift -= CHAR_BIT) {
                const char character = static_cast<char>((packed >> shift) & UCHAR_MAX);
                if (character == '\0') return value;
                value.push_back(character);
                if (value.size() == maxLength) return value;
            }
        }
    }
    for (size_t index = 0; index < maxLength; ++index) {
        const char character = static_cast<char>(address[index] & UCHAR_MAX);
        if (character == '\0') break;
        value.push_back(character);
    }
    return value;
}

static float amx_GetFloat(cell value) {
    float result = 0.0f;
    static_assert(sizeof(result) == sizeof(value));
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

// 1. AddRoleResource(const resourceKey[], const url[])
static cell AMX_NATIVE_CALL n_AddRoleResource(AMX* amx, const cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;
    std::string resourceKey = amx_GetStringParam(amx, params[1]);
    std::string url = amx_GetStringParam(amx, params[2]);

    if (auto comp = GetRoleComponent()) {
        return comp->addRoleResource(resourceKey, url) ? 1 : 0;
    }
    return 0;
}

// SetRoleGlobalConfig(Float:distance, bool:enableLOS, bool:autoHideInVeh, Float:iconWidth, Float:iconHeight)
static cell AMX_NATIVE_CALL n_SetRoleGlobalConfig(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 5 * sizeof(cell)) return 0;
    float distance = amx_GetFloat(params[1]);
    bool enableLOS = (params[2] != 0);
    bool autoHideInVeh = (params[3] != 0);
    float iconWidth = amx_GetFloat(params[4]);
    float iconHeight = amx_GetFloat(params[5]);

    if (auto comp = GetRoleComponent()) {
        return comp->setRoleGlobalConfig(distance, enableLOS, autoHideInVeh, iconWidth, iconHeight) ? 1 : 0;
    }
    return 0;
}

// 2. SetPlayerPresetRole(toPlayerid, targetPlayerid, presetRole, slotID, durationSeconds)
static cell AMX_NATIVE_CALL n_SetPlayerPresetRole(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 3 * sizeof(cell)) return 0;
    int toPlayer = static_cast<int>(params[1]);
    int targetPlayer = static_cast<int>(params[2]);
    uint8_t presetRole = static_cast<uint8_t>(params[3]);
    int slotID = (params[0] >= 4 * sizeof(cell)) ? static_cast<int>(params[4]) : 0;
    int durationSeconds = (params[0] >= 5 * sizeof(cell)) ? static_cast<int>(params[5]) : 0;

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerPresetRole(toPlayer, targetPlayer, presetRole, slotID, durationSeconds) ? 1 : 0;
    }
    return 0;
}

// SetPlayerCustomRole(toPlayerid, targetPlayerid, const tagText[], color, bgColor, bool:stroke, slotID, durationSeconds)
static cell AMX_NATIVE_CALL n_SetPlayerCustomRole(AMX* amx, const cell* params) {
    if (params[0] < 4 * sizeof(cell)) return 0;
    int toPlayer = static_cast<int>(params[1]);
    int targetPlayer = static_cast<int>(params[2]);
    std::string tagText = amx_GetStringParam(amx, params[3]);
    uint32_t color = static_cast<uint32_t>(params[4]);
    uint32_t bgColor = (params[0] >= 5 * sizeof(cell)) ? static_cast<uint32_t>(params[5]) : 0;
    bool stroke = (params[0] >= 6 * sizeof(cell)) ? (params[6] != 0) : true;
    int slotID = (params[0] >= 7 * sizeof(cell)) ? static_cast<int>(params[7]) : 0;
    int durationSeconds = (params[0] >= 8 * sizeof(cell)) ? static_cast<int>(params[8]) : 0;

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerCustomRole(toPlayer, targetPlayer, tagText, color, bgColor, stroke, slotID, durationSeconds) ? 1 : 0;
    }
    return 0;
}

// SetPlayerImageRole(toPlayerid, targetPlayerid, const resourceKey[], const tagText[], color, slotID, durationSeconds)
static cell AMX_NATIVE_CALL n_SetPlayerImageRole(AMX* amx, const cell* params) {
    if (params[0] < 3 * sizeof(cell)) return 0;
    int toPlayer = static_cast<int>(params[1]);
    int targetPlayer = static_cast<int>(params[2]);
    std::string resourceKey = amx_GetStringParam(amx, params[3]);
    std::string tagText = (params[0] >= 4 * sizeof(cell)) ? amx_GetStringParam(amx, params[4]) : "";
    uint32_t color = (params[0] >= 5 * sizeof(cell)) ? static_cast<uint32_t>(params[5]) : 0xFFFFFFFF;
    int slotID = (params[0] >= 6 * sizeof(cell)) ? static_cast<int>(params[6]) : 0;
    int durationSeconds = (params[0] >= 7 * sizeof(cell)) ? static_cast<int>(params[7]) : 0;

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerImageRole(toPlayer, targetPlayer, resourceKey, tagText, color, slotID, durationSeconds) ? 1 : 0;
    }
    return 0;
}

// 3. SetPlayerRainbowRole(targetPlayerid, bool:toggle, speed_ms, slotID)
static cell AMX_NATIVE_CALL n_SetPlayerRainbowRole(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 2 * sizeof(cell)) return 0;
    int targetPlayer = static_cast<int>(params[1]);
    bool toggle = (params[2] != 0);
    int speed_ms = (params[0] >= 3 * sizeof(cell)) ? static_cast<int>(params[3]) : 500;
    int slotID = (params[0] >= 4 * sizeof(cell)) ? static_cast<int>(params[4]) : -1;

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerRainbowRole(targetPlayer, toggle, speed_ms, slotID) ? 1 : 0;
    }
    return 0;
}

// IsPlayerRainbowActive(playerid, slotID)
static cell AMX_NATIVE_CALL n_IsPlayerRainbowActive(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 1 * sizeof(cell)) return 0;
    int playerid = static_cast<int>(params[1]);
    int slotID = (params[0] >= 2 * sizeof(cell)) ? static_cast<int>(params[2]) : -1;

    if (auto comp = GetRoleComponent()) {
        return comp->isPlayerRainbowActive(playerid, slotID) ? 1 : 0;
    }
    return 0;
}

// 4. SetPlayerNametagColor(toPlayerid, targetPlayerid, color)
static cell AMX_NATIVE_CALL n_SetPlayerNametagColor(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 3 * sizeof(cell)) return 0;
    int toPlayer = static_cast<int>(params[1]);
    int targetPlayer = static_cast<int>(params[2]);
    uint32_t color = static_cast<uint32_t>(params[3]);

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerNametagColor(toPlayer, targetPlayer, color) ? 1 : 0;
    }
    return 0;
}

// GetPlayerNametagColor(targetPlayerid, &color)
static cell AMX_NATIVE_CALL n_GetPlayerNametagColor(AMX* amx, const cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;
    int targetPlayer = static_cast<int>(params[1]);
    RoleComponent* component = GetRoleComponent();
    cell* colorPtr = amx_GetAddress(amx, params[2]);
    if (!component || !colorPtr) return 0;

    if (auto comp = component) {
        uint32_t color = 0;
        if (comp->getPlayerNametagColor(targetPlayer, color)) {
            *colorPtr = static_cast<cell>(color);
            return 1;
        }
    }
    return 0;
}

// ClearPlayerRole(toPlayerid, targetPlayerid, slotID)
static cell AMX_NATIVE_CALL n_ClearPlayerRole(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 2 * sizeof(cell)) return 0;
    int toPlayer = static_cast<int>(params[1]);
    int targetPlayer = static_cast<int>(params[2]);
    int slotID = (params[0] >= 3 * sizeof(cell)) ? static_cast<int>(params[3]) : -1;

    if (auto comp = GetRoleComponent()) {
        return comp->clearPlayerRole(toPlayer, targetPlayer, slotID) ? 1 : 0;
    }
    return 0;
}

// SetPlayerRoleVisible(targetPlayerid, bool:toggle, toPlayerid)
static cell AMX_NATIVE_CALL n_SetPlayerRoleVisible(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 2 * sizeof(cell)) return 0;
    int targetPlayer = static_cast<int>(params[1]);
    bool toggle = (params[2] != 0);
    int toPlayer = (params[0] >= 3 * sizeof(cell)) ? static_cast<int>(params[3]) : -1;

    if (auto comp = GetRoleComponent()) {
        return comp->setPlayerRoleVisible(targetPlayer, toggle, toPlayer) ? 1 : 0;
    }
    return 0;
}

// 5. HasPlayerRole(playerid, slotID)
static cell AMX_NATIVE_CALL n_HasPlayerRole(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 1 * sizeof(cell)) return 0;
    int playerid = static_cast<int>(params[1]);
    int slotID = (params[0] >= 2 * sizeof(cell)) ? static_cast<int>(params[2]) : 0;

    if (auto comp = GetRoleComponent()) {
        return comp->hasPlayerRole(playerid, slotID) ? 1 : 0;
    }
    return 0;
}

// IsPlayerRoleVisible(playerid)
static cell AMX_NATIVE_CALL n_IsPlayerRoleVisible(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 1 * sizeof(cell)) return 0;
    int playerid = static_cast<int>(params[1]);

    if (auto comp = GetRoleComponent()) {
        return comp->isPlayerRoleVisible(playerid) ? 1 : 0;
    }
    return 0;
}

// IsRoleResourceLoaded(const resourceKey[])
static cell AMX_NATIVE_CALL n_IsRoleResourceLoaded(AMX* amx, const cell* params) {
    if (params[0] < 1 * sizeof(cell)) return 0;
    std::string resourceKey = amx_GetStringParam(amx, params[1]);

    if (auto comp = GetRoleComponent()) {
        return comp->isRoleResourceLoaded(resourceKey) ? 1 : 0;
    }
    return 0;
}

const AMX_NATIVE_INFO g_RoleNatives[] = {
    { "AddRoleResource",        n_AddRoleResource },
    { "SetRoleGlobalConfig",   n_SetRoleGlobalConfig },
    { "SetPlayerPresetRole",    n_SetPlayerPresetRole },
    { "SetPlayerCustomRole",    n_SetPlayerCustomRole },
    { "SetPlayerImageRole",     n_SetPlayerImageRole },
    { "SetPlayerRainbowRole",   n_SetPlayerRainbowRole },
    { "IsPlayerRainbowActive",  n_IsPlayerRainbowActive },
    { "SetPlayerNametagColor",  n_SetPlayerNametagColor },
    { "GetPlayerNametagColor",  n_GetPlayerNametagColor },
    { "ClearPlayerRole",       n_ClearPlayerRole },
    { "SetPlayerRoleVisible",   n_SetPlayerRoleVisible },
    { "HasPlayerRole",         n_HasPlayerRole },
    { "IsPlayerRoleVisible",   n_IsPlayerRoleVisible },
    { "IsRoleResourceLoaded",  n_IsRoleResourceLoaded },
    { nullptr,                 nullptr }
};

void RegisterRoleNatives(IPawnScript& script) {
    script.Register(g_RoleNatives, -1);
}

} // namespace HUBRole
