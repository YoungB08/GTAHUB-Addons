#include "../include/RoleComponent.hpp"
#include "../include/amx/amx.h"

namespace HUBRole {

static std::string amx_GetStringParam(AMX* amx, cell paramAddress) {
    if (!amx) return "";
    cell* addr = reinterpret_cast<cell*>(amx->base + (amx->data ? (amx->data - amx->base) : 0) + paramAddress);
    if (!addr) return "";
    std::string str;
    while (*addr) {
        str.push_back(static_cast<char>(*addr & 0xFF));
        addr++;
    }
    return str;
}

// 1. AddRoleResource(const resourceKey[], const url[])
static cell AMXAPI n_AddRoleResource(AMX* amx, const cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;
    std::string resourceKey = amx_GetStringParam(amx, params[1]);
    std::string url = amx_GetStringParam(amx, params[2]);

    if (auto comp = GetRoleComponent()) {
        return comp->addRoleResource(resourceKey, url) ? 1 : 0;
    }
    return 0;
}

// SetRoleGlobalConfig(Float:distance, bool:enableLOS, bool:autoHideInVeh, Float:iconWidth, Float:iconHeight)
static cell AMXAPI n_SetRoleGlobalConfig(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 5 * sizeof(cell)) return 0;
    float distance = *reinterpret_cast<const float*>(&params[1]);
    bool enableLOS = (params[2] != 0);
    bool autoHideInVeh = (params[3] != 0);
    float iconWidth = *reinterpret_cast<const float*>(&params[4]);
    float iconHeight = *reinterpret_cast<const float*>(&params[5]);

    if (auto comp = GetRoleComponent()) {
        return comp->setRoleGlobalConfig(distance, enableLOS, autoHideInVeh, iconWidth, iconHeight) ? 1 : 0;
    }
    return 0;
}

// 2. SetPlayerPresetRole(toPlayerid, targetPlayerid, presetRole, slotID, durationSeconds)
static cell AMXAPI n_SetPlayerPresetRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_SetPlayerCustomRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_SetPlayerImageRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_SetPlayerRainbowRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_IsPlayerRainbowActive(AMX* amx, const cell* params) {
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
static cell AMXAPI n_SetPlayerNametagColor(AMX* amx, const cell* params) {
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
static cell AMXAPI n_GetPlayerNametagColor(AMX* amx, const cell* params) {
    if (params[0] < 2 * sizeof(cell)) return 0;
    int targetPlayer = static_cast<int>(params[1]);
    cell* colorPtr = reinterpret_cast<cell*>(amx->base + (amx->data ? (amx->data - amx->base) : 0) + params[2]);

    if (!colorPtr) return 0;

    if (auto comp = GetRoleComponent()) {
        uint32_t color = 0;
        if (comp->getPlayerNametagColor(targetPlayer, color)) {
            *colorPtr = static_cast<cell>(color);
            return 1;
        }
    }
    return 0;
}

// ClearPlayerRole(toPlayerid, targetPlayerid, slotID)
static cell AMXAPI n_ClearPlayerRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_SetPlayerRoleVisible(AMX* amx, const cell* params) {
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
static cell AMXAPI n_HasPlayerRole(AMX* amx, const cell* params) {
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
static cell AMXAPI n_IsPlayerRoleVisible(AMX* amx, const cell* params) {
    (void)amx;
    if (params[0] < 1 * sizeof(cell)) return 0;
    int playerid = static_cast<int>(params[1]);

    if (auto comp = GetRoleComponent()) {
        return comp->isPlayerRoleVisible(playerid) ? 1 : 0;
    }
    return 0;
}

// IsRoleResourceLoaded(const resourceKey[])
static cell AMXAPI n_IsRoleResourceLoaded(AMX* amx, const cell* params) {
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
    { "IsRoleResourceLoaded",   n_IsRoleResourceLoaded },
    { nullptr,                 nullptr }
};

} // namespace HUBRole
