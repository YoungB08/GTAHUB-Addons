#include "OVGameHooks.h"

#include "shared/OVLogger.h"
#include "client/render/OVDx9Renderer.h"

#ifdef _WIN32
#include <Windows.h>
#include <cstring>
#include <d3d9.h>
#include <memory>
#include <MinHook.h>
#include <Psapi.h>
#endif

namespace ov::client
{
#ifdef _WIN32
namespace
{
using EndSceneFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
EndSceneFn g_originalEndScene{};
ResetFn g_originalReset{};
std::weak_ptr<OVDx9Renderer> g_renderer;
void* g_endSceneTarget{};
void* g_resetTarget{};

HRESULT WINAPI HookEndScene(IDirect3DDevice9* device)
{
    if (const auto renderer = g_renderer.lock()) renderer->OnEndScene(device);
    return g_originalEndScene ? g_originalEndScene(device) : D3D_OK;
}
HRESULT WINAPI HookReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters)
{
    if (const auto renderer = g_renderer.lock()) renderer->OnResetBefore();
    const HRESULT result = g_originalReset ? g_originalReset(device, parameters) : D3D_OK;
    if (SUCCEEDED(result)) if (const auto renderer = g_renderer.lock()) renderer->OnResetAfter(device);
    return result;
}
LRESULT CALLBACK DummyWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}
}
#endif

bool OVGameHooks::Initialize()
{
#ifdef _WIN32
    const HMODULE samp = GetModuleHandleA("samp.dll");
    const HMODULE openMp = GetModuleHandleA("open.mp.dll");
    if (openMp) supported_ = true;
    if (samp)
    {
        MODULEINFO info{};
        GetModuleInformation(GetCurrentProcess(), samp, &info, sizeof(info));
        const auto* bytes = static_cast<const char*>(info.lpBaseOfDll);
        const std::size_t size = info.SizeOfImage;
        const auto contains = [bytes, size](const char* needle) {
            const std::size_t length = std::strlen(needle);
            for (std::size_t index = 0; index + length <= size; ++index) if (std::memcmp(bytes + index, needle, length) == 0) return true;
            return false;
        };
        supported_ = supported_ || contains("0.3.DL-R1") || contains("0.3.DL R1") || contains("0.3.DL");
        if (!supported_) OV_LOG_WARN("Hooks", "samp.dll found but SA:MP 0.3.DL R1 signature was not detected");
    }
#else
    supported_ = false;
#endif
    if (!supported_) OV_LOG_ERROR("Hooks", "Unsupported client: SA:MP 0.3.DL R1/open.mp module not detected");
    return true;
}
bool OVGameHooks::InstallDx9Hooks(const std::shared_ptr<OVDx9Renderer>& renderer)
{
#ifdef _WIN32
    if (dx9Hooked_) return true;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"OMPVoiceDx9Probe";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = &DummyWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);
    HWND window = CreateWindowExW(0, className, L"", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, instance, nullptr);
    if (!window) return false;
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { DestroyWindow(window); UnregisterClassW(className, instance); return false; }
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    IDirect3DDevice9* device = nullptr;
    HRESULT created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created)) created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created) || !device)
    {
        d3d->Release(); DestroyWindow(window); UnregisterClassW(className, instance); return false;
    }
    void** vtable = *reinterpret_cast<void***>(device);
    g_resetTarget = vtable[16];
    g_endSceneTarget = vtable[42];
    const MH_STATUS initializeStatus = MH_Initialize();
    const bool initialized = initializeStatus == MH_OK || initializeStatus == MH_ERROR_ALREADY_INITIALIZED;
    const bool resetHook = initialized && MH_CreateHook(g_resetTarget, &HookReset, reinterpret_cast<void**>(&g_originalReset)) == MH_OK;
    const bool endSceneHook = resetHook && MH_CreateHook(g_endSceneTarget, &HookEndScene, reinterpret_cast<void**>(&g_originalEndScene)) == MH_OK;
    const bool enabled = endSceneHook && MH_EnableHook(MH_ALL_HOOKS) == MH_OK;
    device->Release(); d3d->Release(); DestroyWindow(window); UnregisterClassW(className, instance);
    if (!enabled)
    {
        if (g_endSceneTarget) MH_RemoveHook(g_endSceneTarget);
        if (g_resetTarget) MH_RemoveHook(g_resetTarget);
        MH_Uninitialize();
        return false;
    }
    g_renderer = renderer;
    dx9Hooked_ = true;
    OV_LOG_INFO("Hooks", "DX9 EndScene and Reset hooks installed");
    return true;
#else
    (void)renderer; return false;
#endif
}
void OVGameHooks::Shutdown()
{
#ifdef _WIN32
    if (dx9Hooked_)
    {
        MH_DisableHook(MH_ALL_HOOKS);
        if (g_endSceneTarget) MH_RemoveHook(g_endSceneTarget);
        if (g_resetTarget) MH_RemoveHook(g_resetTarget);
        MH_Uninitialize();
        g_renderer.reset(); g_originalEndScene = nullptr; g_originalReset = nullptr;
        g_endSceneTarget = nullptr; g_resetTarget = nullptr;
    }
#endif
    dx9Hooked_ = false;
    supported_ = false;
}
bool OVGameHooks::IsKeyDown(int virtualKey) const noexcept
{
#ifdef _WIN32
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
#else
    (void)virtualKey; return false;
#endif
}
void OVGameHooks::PollHotkeys()
{
    const std::uint16_t keys = static_cast<std::uint16_t>((IsKeyDown(VK_F11) ? 1 : 0) | (IsKeyDown(VK_F10) ? 2 : 0) | (IsKeyDown(VK_F8) ? 4 : 0) | (IsKeyDown(VK_F9) ? 8 : 0));
    settingsPressed_ = (keys & 1U) != 0 && (previousKeys_ & 1U) == 0;
    debugPressed_ = (keys & 2U) != 0 && (previousKeys_ & 2U) == 0;
    if ((keys & 4U) != 0 && (previousKeys_ & 4U) == 0) loopbackToggled_ = !loopbackToggled_;
    if ((keys & 8U) != 0 && (previousKeys_ & 8U) == 0) fakeRemoteToggled_ = !fakeRemoteToggled_;
    if (debugPressed_) overlayToggled_ = !overlayToggled_;
    previousKeys_ = keys;
}
}
