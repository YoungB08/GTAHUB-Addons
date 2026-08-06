#include <Windows.h>
#include <Psapi.h>

#include <cstdint>
#include <filesystem>
#include <iostream>

namespace
{
using ShutdownFn = BOOL (WINAPI*)(DWORD);

std::uint64_t FileTimeValue(const FILETIME& value)
{
    ULARGE_INTEGER result{};
    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

std::uint64_t ProcessCpuTime()
{
    FILETIME creation{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) return 0;
    return FileTimeValue(kernel) + FileTimeValue(user);
}
}

int main(int argc, char** argv)
{
    if (argc != 2 || !std::filesystem::is_regular_file(argv[1])) return 1;
    DWORD handlesBaseline{}, handlesMidpoint{}, handlesAfter{};
    PROCESS_MEMORY_COUNTERS_EX memoryBaseline{sizeof(memoryBaseline)};
    PROCESS_MEMORY_COUNTERS_EX memoryMidpoint{sizeof(memoryMidpoint)};
    PROCESS_MEMORY_COUNTERS_EX memoryAfter{sizeof(memoryAfter)};
    for (int iteration = 0; iteration < 21; ++iteration)
    {
        const HMODULE module = LoadLibraryA(argv[1]);
        if (!module) return 2;
        const auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(module, "OV_Shutdown"));
        if (!shutdown) { FreeLibrary(module); return 3; }
        bool idleBudgetPassed = true;
        if (iteration == 0)
        {
            Sleep(2000);
            const auto cpuBefore = ProcessCpuTime();
            Sleep(10000);
            const auto cpuAfter = ProcessCpuTime();
            const double idleCpuPercent = static_cast<double>(cpuAfter - cpuBefore) / 10000000.0 / 10.0 * 100.0;
            std::cout << "ASI idle CPU: " << idleCpuPercent << "% of one core\n";
            idleBudgetPassed = cpuAfter >= cpuBefore && idleCpuPercent < 2.0;
        }
        if (!shutdown(10000)) return 4;
        if (!FreeLibrary(module)) return 5;
        if (!idleBudgetPassed) return 6;
        if (iteration == 0)
        {
            if (!GetProcessHandleCount(GetCurrentProcess(), &handlesBaseline) ||
                !GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryBaseline), sizeof(memoryBaseline))) return 7;
        }
        else if (iteration == 10)
        {
            if (!GetProcessHandleCount(GetCurrentProcess(), &handlesMidpoint) ||
                !GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryMidpoint), sizeof(memoryMidpoint))) return 8;
        }
        else if (iteration == 20)
        {
            if (!GetProcessHandleCount(GetCurrentProcess(), &handlesAfter) ||
                !GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryAfter), sizeof(memoryAfter))) return 9;
        }
    }
    std::cout << "ASI lifecycle handles: " << handlesBaseline << " -> " << handlesMidpoint << " -> " << handlesAfter
              << ", private bytes: " << memoryBaseline.PrivateUsage << " -> " << memoryMidpoint.PrivateUsage
              << " -> " << memoryAfter.PrivateUsage << '\n';
    if (handlesAfter > handlesMidpoint + 2) return 10;
    if (memoryAfter.PrivateUsage > memoryMidpoint.PrivateUsage + 4U * 1024U * 1024U) return 11;
    std::cout << "ASI synchronous shutdown and 20-cycle unload stability passed\n";
    return 0;
}
