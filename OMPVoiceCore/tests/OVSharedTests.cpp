#include "shared/OVConfig.h"
#include "shared/OVCrashSafety.h"
#include "shared/OVPacket.h"

#include <filesystem>
#include <iostream>

int RunChannelTests();

namespace
{
int failures = 0;
void Check(bool condition, const char* name)
{
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; ++failures; }
}
}

int main()
{
    const ov::Handshake handshake{};
    const auto handshakeBytes = ov::SerializeHandshake(handshake);
    const auto parsedHandshake = ov::ParseHandshake(handshakeBytes);
    Check(parsedHandshake.has_value(), "handshake parses");
    Check(parsedHandshake && parsedHandshake->magic == ov::OMPVOICE_MAGIC, "handshake magic");
    Check(parsedHandshake && parsedHandshake->uid == ov::OMPVOICE_UID, "handshake uid");

    ov::VoiceFrame frame;
    frame.playerId = 7;
    frame.channelId = 42;
    frame.sequence = 65530;
    frame.timestampMs = 123456;
    frame.mode = ov::VoiceMode::Radio;
    frame.encoded = {1, 2, 3, 4};
    const auto frameBytes = ov::SerializeVoiceFrame(frame);
    const auto parsedFrame = ov::ParseVoiceFrame(frameBytes);
    Check(parsedFrame && parsedFrame->encoded == frame.encoded, "voice payload roundtrip");
    Check(parsedFrame && parsedFrame->mode == ov::VoiceMode::Radio, "voice mode roundtrip");
    auto corrupt = frameBytes;
    corrupt.pop_back();
    Check(!ov::ParseVoiceFrame(corrupt), "truncated datagram rejected");

    const auto configPath = std::filesystem::temp_directory_path() / "ompvoice_config_test.json";
    ov::Config config;
    config.sound.masterVolume = 150;
    config.phoneLeakRadius = 2.0F;
    ov::ConfigStore store(configPath);
    std::string error;
    Check(store.Save(config, error), "config save");
    ov::Config loaded;
    Check(store.Load(loaded, error), "config load");
    Check(loaded.sound.masterVolume == 100, "volume validation");
    Check(loaded.phoneLeakRadius == 5.0F, "phone radius validation");
    std::error_code ignored;
    std::filesystem::remove(configPath, ignored);
    std::filesystem::remove(configPath.string() + ".bak", ignored);

    failures += RunChannelTests();
    Check(ov::CrashSafety::Install(std::filesystem::temp_directory_path() / "ompvoice-crash-test"), "crash handler install");
    ov::CrashSafety::Uninstall();

    if (failures == 0) std::cout << "All shared tests passed\n";
    return failures == 0 ? 0 : 1;
}
