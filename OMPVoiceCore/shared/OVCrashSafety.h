#pragma once

#include <filesystem>

namespace ov
{
class CrashSafety final
{
public:
    static bool Install(std::filesystem::path directory);
    static void Uninstall();
};
}
