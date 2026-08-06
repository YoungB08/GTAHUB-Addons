#include "server/channels/OVChannelManager.h"

#include <algorithm>
#include <iostream>

namespace
{
int failures = 0;
void Check(bool condition)
{
    if (!condition) { ++failures; std::cerr << "channel check " << failures << " failed\n"; }
}
}

int RunChannelTests()
{
    ov::server::OVChannelManager manager;
    manager.UpsertPlayer(1, {0.0F, 0.0F, 0.0F}, -1);
    manager.UpsertPlayer(2, {5.0F, 0.0F, 0.0F}, -1);
    manager.UpsertPlayer(3, {50.0F, 0.0F, 0.0F}, -1);
    const auto proximity = manager.CreateChannel(25.0F);
    const auto nearby = manager.Recipients(1, proximity);
    Check(std::find_if(nearby.begin(), nearby.end(), [](const auto& recipient) { return recipient.playerId == 2; }) != nearby.end());
    Check(nearby.size() == 1);

    const auto global = manager.CreateChannel(-1.0F, ov::VoiceMode::Global);
    Check(manager.Recipients(1, global).size() == 2);
    manager.UpsertPlayer(2, {5.0F, 0.0F, 0.0F}, 9);
    manager.UpsertPlayer(1, {0.0F, 0.0F, 0.0F}, 9);
    const auto vehicle = manager.CreateChannel(5.0F, ov::VoiceMode::Vehicle);
    Check(manager.Recipients(1, vehicle).size() == 1);

    const auto radio = manager.CreateRadioChannel();
    Check(manager.JoinRadio(1, radio));
    Check(manager.JoinRadio(2, radio));
    Check(manager.Recipients(1, radio).size() == 1);

    Check(manager.StartPhoneCall(1, 2));
    manager.UpsertPlayer(3, {7.0F, 0.0F, 0.0F}, -1);
    Check(!manager.Recipients(1, proximity).empty());
    manager.EndPhoneCall(1);
    manager.SetMuted(2, 1, true);
    Check(manager.Recipients(1, global).size() == 1);
    return failures;
}
