#pragma once

#include "shared/OVPacket.h"

#include <cstdint>
#include <optional>

namespace ov::client
{
class OVDebugVoiceRouter final
{
public:
    [[nodiscard]] static std::optional<VoiceFrame> Route(const VoiceFrame& source, bool mirrorMode,
                                                         bool fakeRemote, std::uint64_t nowMs);
};
}
