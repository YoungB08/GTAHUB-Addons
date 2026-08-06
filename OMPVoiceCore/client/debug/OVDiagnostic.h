#pragma once

#include <filesystem>
#include <string>

namespace ov::client
{
class OVBassApi;
class OVAudioEngine;
class OVDiagnostic final
{
public:
    explicit OVDiagnostic(std::filesystem::path directory = std::filesystem::path("ompvoice") / "debug");
    bool Run(const OVBassApi& bass, const OVAudioEngine& audio, bool dx9Hooked, std::string& outputPath) const;

private:
    std::filesystem::path directory_;
};
}
