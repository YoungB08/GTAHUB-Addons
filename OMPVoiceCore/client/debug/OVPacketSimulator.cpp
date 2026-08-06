#include "OVPacketSimulator.h"

#include <algorithm>

namespace ov::client
{
bool OVPacketSimulator::Drop()
{
    std::uniform_real_distribution<float> distribution(0.0F, 100.0F);
    return distribution(random_) < std::clamp(packetLoss_, 0.0F, 50.0F);
}
int OVPacketSimulator::Jitter() const
{
    std::uniform_int_distribution<int> distribution(-std::max(0, jitterMs_), std::max(0, jitterMs_));
    return distribution(random_);
}
}
