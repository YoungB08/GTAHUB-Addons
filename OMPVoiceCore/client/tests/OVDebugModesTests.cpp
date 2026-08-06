#include "audio/OVAudioEngine.h"
#include "audio/OVBassApi.h"
#include "debug/OVDebugVoiceRouter.h"
#include "shared/OVConstants.h"

#include <Windows.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    ov::client::OVBassApi bass;
    ov::client::OVAudioEngine audio(bass);
    if (!audio.Initialize(0)) return 1;
    audio.SetTransmitting(true);
    audio.SetLoopback(true);
    audio.EnableNoiseSuppression(false);
    audio.EnableAGC(false);

    ov::VoiceFrame captured;
    bool frameCaptured = false;
    audio.SetFrameHandler([&](std::vector<std::uint8_t> encoded) {
        captured.playerId = 1;
        captured.channelId = 1;
        captured.sequence = 1;
        captured.encoded = std::move(encoded);
        frameCaptured = true;
    });
    std::vector<std::int16_t> pcm(ov::FRAME_SAMPLES);
    for (std::size_t index = 0; index < pcm.size(); ++index)
        pcm[index] = static_cast<std::int16_t>(std::sin(static_cast<double>(index) * 0.08) * 10000.0);
    if (!audio.InjectDebugCaptureFrame(std::move(pcm)) || !frameCaptured) return 2;
    for (int attempt = 0; attempt < 50 && audio.PlayedSamples() == 0; ++attempt) Sleep(10);
    if (audio.PlayedSamples() == 0) return 3;

    const auto mirror = ov::client::OVDebugVoiceRouter::Route(captured, true, false, 0);
    if (!mirror || mirror->playerId != 999 || mirror->gain != captured.gain || mirror->pan != captured.pan) return 4;
    audio.OnRemoteFrame(*mirror);
    audio.Update();
    if (audio.RemoteStreamCount() != 1) return 5;

    const auto fake = ov::client::OVDebugVoiceRouter::Route(captured, false, true, 1500);
    if (!fake || fake->playerId != 999 || fake->pan < -0.85F || fake->pan > 0.85F || fake->gain < 0.35F || fake->gain > 0.80F) return 6;
    if (ov::client::OVDebugVoiceRouter::Route(captured, false, false, 0)) return 7;
    audio.Shutdown();
    std::cout << "Loopback, mirror, and fake-remote integration passed\n";
    return 0;
}
