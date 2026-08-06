#include "OVPawnNatives.h"

#include "server/channels/OVChannelManager.h"
#include "shared/OVConstants.h"

#include <algorithm>
#include <iterator>

#include <Server/Components/Pawn/pawn.hpp>

namespace ov::server
{
namespace
{
OVChannelManager* g_channels = nullptr;
cell Bool(bool value) { return value ? 1 : 0; }
int Player(cell value) { return static_cast<int>(value); }
float Float(cell value) { return amx_ctof(value); }

cell OV_EnableVoice(AMX*, const cell* params) { if (!g_channels || !params) return 0; g_channels->SetEnabled(Player(params[1]), params[2] != 0); return 1; }
cell OV_IsTalking(AMX*, const cell* params) { return g_channels && params ? Bool(g_channels->IsTalking(Player(params[1]))) : 0; }
cell OV_SetVolume(AMX*, const cell* params) { if (!g_channels || !params) return 0; g_channels->SetVolume(Player(params[1]), std::clamp(Float(params[2]), 0.0F, 2.0F)); return 1; }
cell OV_SetMuted(AMX*, const cell* params) { if (!g_channels || !params) return 0; g_channels->SetMuted(Player(params[1]), Player(params[2]), params[3] != 0); return 1; }
cell OV_CreateChannel(AMX*, const cell* params) { return g_channels && params ? static_cast<cell>(g_channels->CreateChannel(Float(params[1]))) : 0; }
cell OV_DestroyChannel(AMX*, const cell* params) { return g_channels && params ? Bool(g_channels->DestroyChannel(static_cast<std::uint32_t>(params[1]))) : 0; }
cell OV_SetTalkKey(AMX*, const cell* params) { if (!g_channels || !params) return 0; g_channels->SetTalkKey(Player(params[1]), Player(params[2])); return 1; }
cell OV_StartPhoneCall(AMX*, const cell* params) { return g_channels && params ? Bool(g_channels->StartPhoneCall(Player(params[1]), Player(params[2]))) : 0; }
cell OV_EndPhoneCall(AMX*, const cell* params) { if (g_channels && params) g_channels->EndPhoneCall(Player(params[1])); return 1; }
cell OV_ShowHudIcon(AMX*, const cell* params) { if (g_channels && params) g_channels->SetHudVisible(Player(params[1]), params[2] != 0); return 1; }
cell OV_CreateRadioChannel(AMX*, const cell*) { return g_channels ? static_cast<cell>(g_channels->CreateRadioChannel()) : 0; }
cell OV_JoinRadioChannel(AMX*, const cell* params) { return g_channels && params ? Bool(g_channels->JoinRadio(Player(params[1]), static_cast<std::uint32_t>(params[2]))) : 0; }
cell OV_LeaveRadioChannel(AMX*, const cell* params) { return g_channels && params ? Bool(g_channels->LeaveRadio(Player(params[1]), static_cast<std::uint32_t>(params[2]))) : 0; }
}

void SetNativeContext(OVChannelManager* channels) { g_channels = channels; }

void RegisterNatives(IPawnScript& script)
{
    const AMX_NATIVE_INFO natives[] = {
        {"OV_EnableVoice", &OV_EnableVoice}, {"OV_IsTalking", &OV_IsTalking}, {"OV_SetVolume", &OV_SetVolume},
        {"OV_SetMuted", &OV_SetMuted}, {"OV_CreateChannel", &OV_CreateChannel}, {"OV_DestroyChannel", &OV_DestroyChannel},
        {"OV_SetTalkKey", &OV_SetTalkKey}, {"OV_StartPhoneCall", &OV_StartPhoneCall}, {"OV_EndPhoneCall", &OV_EndPhoneCall},
        {"OV_ShowHudIcon", &OV_ShowHudIcon}, {"OV_CreateRadioChannel", &OV_CreateRadioChannel},
        {"OV_JoinRadioChannel", &OV_JoinRadioChannel}, {"OV_LeaveRadioChannel", &OV_LeaveRadioChannel}};
    script.Register(natives, static_cast<int>(std::size(natives)));
}
}
