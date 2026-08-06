#include <MinHook.h>

#include <iostream>

namespace
{
using TargetFn = int (*)(int);
TargetFn g_original{};

__declspec(noinline) int Target(int value) { return value + 1; }
__declspec(noinline) int Detour(int value) { return g_original(value) + 10; }
}

int main()
{
    volatile TargetFn target = &Target;
    if (target(5) != 6) return 1;
    if (MH_Initialize() != MH_OK) return 2;
    if (MH_CreateHook(reinterpret_cast<void*>(&Target), reinterpret_cast<void*>(&Detour),
                      reinterpret_cast<void**>(&g_original)) != MH_OK) return 3;
    if (MH_EnableHook(reinterpret_cast<void*>(&Target)) != MH_OK) return 4;
    if (target(5) != 16) return 5;
    if (MH_DisableHook(reinterpret_cast<void*>(&Target)) != MH_OK) return 6;
    if (target(5) != 6) return 7;
    if (MH_RemoveHook(reinterpret_cast<void*>(&Target)) != MH_OK) return 8;
    if (MH_Uninitialize() != MH_OK) return 9;
    std::cout << "MinHook detour, trampoline, disable, and removal passed\n";
    return 0;
}
