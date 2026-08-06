#include "server/natives/OVPawnNatives.h"

#include <array>
#include <iostream>
#include <string_view>

int main()
{
    static constexpr std::array<std::string_view, 13> expected{
        "OV_EnableVoice", "OV_IsTalking", "OV_SetVolume", "OV_SetMuted",
        "OV_CreateChannel", "OV_DestroyChannel", "OV_SetTalkKey",
        "OV_StartPhoneCall", "OV_EndPhoneCall", "OV_ShowHudIcon",
        "OV_CreateRadioChannel", "OV_JoinRadioChannel", "OV_LeaveRadioChannel"};

    const auto& registered = ov::server::RegisteredNativeNames();
    if (registered.size() != expected.size()) return 1;
    for (const auto name : expected) if (!ov::server::HasRegisteredNative(name)) return 2;
    if (ov::server::HasRegisteredNative("OV_MissingNative")) return 3;
    std::cout << "All Pawn natives are available through the registration lookup\n";
    return 0;
}
