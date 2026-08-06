#include <Windows.h>

#include <component.hpp>
#include <sdk.hpp>

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2 || !std::filesystem::is_regular_file(argv[1])) return 1;
    const HMODULE module = LoadLibraryA(argv[1]);
    if (!module) return 2;
    const auto entryPoint = reinterpret_cast<ComponentEntryPoint_t>(GetProcAddress(module, "ComponentEntryPoint"));
    if (!entryPoint) { FreeLibrary(module); return 3; }
    IComponent* component = entryPoint();
    if (!component) { FreeLibrary(module); return 4; }
    if (std::string(component->componentName()) != "OMPVoiceCore" || component->getUID() != 0xD6FEE4A6B0EA27A3ULL)
    {
        component->free();
        FreeLibrary(module);
        return 5;
    }
    component->free();
    if (!FreeLibrary(module)) return 6;
    std::cout << "Component entry point load/free/unload lifecycle passed\n";
    return 0;
}
