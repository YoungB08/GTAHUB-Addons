#include <Windows.h>

#include <filesystem>
#include <iostream>

namespace
{
using ShutdownFn = BOOL (WINAPI*)(DWORD);
}

int main(int argc, char** argv)
{
    if (argc != 2 || !std::filesystem::is_regular_file(argv[1])) return 1;
    for (int iteration = 0; iteration < 5; ++iteration)
    {
        const HMODULE module = LoadLibraryA(argv[1]);
        if (!module) return 2;
        const auto shutdown = reinterpret_cast<ShutdownFn>(GetProcAddress(module, "OV_Shutdown"));
        if (!shutdown) { FreeLibrary(module); return 3; }
        if (!shutdown(10000)) return 4;
        if (!FreeLibrary(module)) return 5;
    }
    std::cout << "ASI synchronous shutdown and unload lifecycle passed\n";
    return 0;
}
