#include "server/channels/OVChannelManager.h"

#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

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
    const auto nearbyPlayer = std::find_if(nearby.begin(), nearby.end(), [](const auto& recipient) { return recipient.playerId == 2; });
    Check(nearbyPlayer != nearby.end());
    Check(nearbyPlayer != nearby.end() && nearbyPlayer->gain > 0.0F && nearbyPlayer->gain < 1.0F);
    Check(nearbyPlayer != nearby.end() && nearbyPlayer->pan >= -1.0F && nearbyPlayer->pan <= 1.0F);
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

    ov::server::OVChannelManager phoneManager;
    phoneManager.UpsertPlayer(1, {0.0F, 0.0F, 0.0F}, -1);
    phoneManager.UpsertPlayer(2, {100.0F, 0.0F, 0.0F}, -1);
    phoneManager.UpsertPlayer(3, {7.0F, 0.0F, 0.0F}, -1);
    Check(phoneManager.StartPhoneCall(1, 2));
    phoneManager.SetPhoneLeakRadius(4.0F);
    Check(phoneManager.PhoneLeakRadius() == 5.0F);
    auto phoneRecipients = phoneManager.Recipients(1, 0);
    Check(std::find_if(phoneRecipients.begin(), phoneRecipients.end(), [](const auto& recipient) { return recipient.playerId == 3; }) == phoneRecipients.end());
    phoneManager.SetPhoneLeakRadius(12.0F);
    Check(phoneManager.PhoneLeakRadius() == 10.0F);
    phoneRecipients = phoneManager.Recipients(1, 0);
    Check(std::find_if(phoneRecipients.begin(), phoneRecipients.end(), [](const auto& recipient) { return recipient.playerId == 3 && recipient.mode == ov::VoiceMode::Phone; }) != phoneRecipients.end());

    ov::server::OVChannelManager concurrent;
    const auto stressChannel = concurrent.CreateChannel(25.0F);
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker)
    {
        workers.emplace_back([&concurrent, stressChannel, worker] {
            for (int iteration = 0; iteration < 2000; ++iteration)
            {
                const int playerId = worker * 100 + iteration % 50;
                concurrent.UpsertPlayer(playerId, {static_cast<float>(iteration % 20), static_cast<float>(worker), 0.0F}, -1);
                concurrent.SetTalking(playerId, (iteration & 1) != 0);
                (void)concurrent.Recipients(playerId, stressChannel);
            }
        });
    }
    for (auto& worker : workers) worker.join();
    return failures;
}
