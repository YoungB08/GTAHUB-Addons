#include "OVFakeRemote.h"

#include <cmath>

namespace ov::client
{
void OVFakeRemote::Update(float deltaSeconds)
{
    if (!enabled_) return;
    player_.elapsed += deltaSeconds;
    player_.talking = std::fmod(player_.elapsed, 3.0F) < 1.5F;
}
}
