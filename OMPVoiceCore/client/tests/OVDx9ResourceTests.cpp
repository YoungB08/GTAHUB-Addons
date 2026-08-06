#include "render/OVHudIcon.h"
#include "render/OVResourceManager.h"
#include "render/OVSpeakerRenderer.h"

#include <Windows.h>
#include <Psapi.h>
#include <d3d9.h>

#include <cstdint>
#include <filesystem>
#include <iostream>

namespace
{
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}

std::size_t CountColoredPixels(IDirect3DSurface9* surface, int left, int top, int right, int bottom)
{
    D3DLOCKED_RECT locked{};
    if (FAILED(surface->LockRect(&locked, nullptr, D3DLOCK_READONLY))) return 0;
    std::size_t count = 0;
    for (int y = top; y < bottom; ++y)
    {
        const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(locked.pBits) + y * locked.Pitch);
        for (int x = left; x < right; ++x) if ((row[x] & 0x00FFFFFFU) != 0) ++count;
    }
    surface->UnlockRect();
    return count;
}
}

int main(int argc, char** argv)
{
    if (argc != 2 || !std::filesystem::is_directory(argv[1])) return 1;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"OMPVoiceDx9ResourceTest";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = &WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) return 2;
    HWND window = CreateWindowExW(0, className, L"", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, instance, nullptr);
    if (!window) return 3;
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return 4;
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 640;
    parameters.BackBufferHeight = 480;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    IDirect3DDevice9* device = nullptr;
    HRESULT created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created)) created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created) || !device) return 5;

    int result = 0;
    {
        ov::client::OVResourceManager resources;
        if (!resources.Initialize(device, argv[1]) || !resources.ReloadTextures()) result = 6;
        static constexpr const char* names[]{"logo.png", "micro_active.png", "micro_muted.png", "micro_passive.png", "speaker.png"};
        for (const auto* name : names) if (!resources.IsLoaded(name) || resources.IsFallback(name)) result = 7;

        if (result == 0)
        {
            device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0F, 0);
            device->BeginScene();
            ov::client::OVHudIcon hud;
            hud.Render(device, resources.Get("micro_active.png"), 640.0F, 480.0F, true, false, 0.0F);
            ov::client::OVSpeakerRenderer speakers;
            speakers.SetLocalPlayerId(1);
            speakers.SetViewport(640.0F, 480.0F);
            speakers.Submit(ov::client::SpeakerState{999, 100.0F, 100.0F, true, 5.0F});
            speakers.Render(device, resources.Get("speaker.png"));
            device->EndScene();

            IDirect3DSurface9* backBuffer = nullptr;
            IDirect3DSurface9* copy = nullptr;
            if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer)) ||
                FAILED(device->CreateOffscreenPlainSurface(640, 480, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &copy, nullptr)) ||
                FAILED(device->GetRenderTargetData(backBuffer, copy))) result = 8;
            if (result == 0 && (CountColoredPixels(copy, 200, 330, 440, 430) < 500 || CountColoredPixels(copy, 50, 50, 150, 150) < 200)) result = 9;
            if (copy) copy->Release();
            if (backBuffer) backBuffer->Release();
        }

        DWORD handlesMidpoint{};
        PROCESS_MEMORY_COUNTERS_EX memoryMidpoint{sizeof(memoryMidpoint)};
        for (int reset = 0; reset < 20 && result == 0; ++reset)
        {
            resources.ReleaseAllTextures();
            if (FAILED(device->Reset(&parameters)) || !resources.ReloadTextures()) result = 10;
            if (reset == 9)
            {
                GetProcessHandleCount(GetCurrentProcess(), &handlesMidpoint);
                GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryMidpoint), sizeof(memoryMidpoint));
            }
        }
        DWORD handlesAfter{};
        PROCESS_MEMORY_COUNTERS_EX memoryAfter{sizeof(memoryAfter)};
        GetProcessHandleCount(GetCurrentProcess(), &handlesAfter);
        GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryAfter), sizeof(memoryAfter));
        if (result == 0 && (handlesAfter > handlesMidpoint + 2 || memoryAfter.PrivateUsage > memoryMidpoint.PrivateUsage + 4U * 1024U * 1024U)) result = 11;
        resources.ReleaseAllTextures();
    }
    const ULONG deviceRefs = device->Release();
    const ULONG d3dRefs = d3d->Release();
    DestroyWindow(window);
    UnregisterClassW(className, instance);
    if (result != 0) return result;
    if (deviceRefs != 0 || d3dRefs != 0) return 12;
    std::cout << "WIC textures, HUD/speaker pixels, and 20 DX9 resets passed\n";
    return 0;
}
