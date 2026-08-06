#pragma once

#include "shared/OVConfig.h"

#include <filesystem>
#include <string>

namespace ov::client
{
class OVClientConfig final
{
public:
    explicit OVClientConfig(std::filesystem::path path = std::filesystem::path("ompvoice") / "config.json");
    bool Load();
    bool Save();
    [[nodiscard]] Config& Values() noexcept { return values_; }
    [[nodiscard]] const Config& Values() const noexcept { return values_; }
    [[nodiscard]] const std::string& LastError() const noexcept { return lastError_; }
    [[nodiscard]] const std::filesystem::path& Path() const noexcept { return store_.Path(); }

private:
    Config values_;
    ConfigStore store_;
    std::string lastError_;
};
}
