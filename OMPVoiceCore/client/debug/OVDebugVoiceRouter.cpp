#include "OVDebugVoiceRouter.h"

#include <cmath>

namespace ov::client
{
std::optional<VoiceFrame> OVDebugVoiceRouter::Route(const VoiceFrame& source, bool mirrorMode,
                                                    bool fakeRemote, std::uint64_t nowMs)
{
    if (!mirrorMode && !fakeRemote) return std::nullopt;
    VoiceFrame routed = source;
    routed.playerId = 999;
    if (fakeRemote)
    {
        const float phase = static_cast<float>(nowMs % 6000U) / 6000.0F * 6.2831853F;
        routed.pan = std::sin(phase) * 0.85F;
        routed.gain = 0.35F + 0.45F * (std::sin(phase * 0.5F) * 0.5F + 0.5F);
    }
    return routed;
}
}
