#pragma once

#include <filesystem>
#include <string>

namespace ov::server
{
class ServerVoiceDebug final
{
public:
    explicit ServerVoiceDebug(std::filesystem::path outputDirectory);
    bool Run(bool udpBound, std::string& outputPath) const;

private:
    std::filesystem::path outputDirectory_;
};
}
