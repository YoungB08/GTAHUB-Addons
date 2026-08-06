#include "audio/OVAudioEngine.h"
#include "audio/OVBassApi.h"
#include "config/OVClientConfig.h"
#include "debug/OVDiagnostic.h"
#include "render/OVDx9Renderer.h"
#include "ui/OVSettingsPanel.h"

#include <Windows.h>
#include <d3d9.h>

#include <array>
#include <filesystem>
#include <iostream>

namespace
{
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int main()
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"OMPVoiceDx9RendererTest";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = &WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) return 1;
    HWND window = CreateWindowExW(0, className, L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return 3;
    D3DPRESENT_PARAMETERS parameters{};
    parameters.Windowed = TRUE;
    parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    parameters.hDeviceWindow = window;
    parameters.BackBufferWidth = 800;
    parameters.BackBufferHeight = 600;
    parameters.BackBufferFormat = D3DFMT_X8R8G8B8;
    IDirect3DDevice9* device = nullptr;
    HRESULT created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created)) created = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &parameters, &device);
    if (FAILED(created) || !device) return 4;

    int result = 0;
    {
        ov::client::OVBassApi bass;
        if (!bass.Load()) result = 5;
        ov::client::OVAudioEngine audio(bass);
        ov::client::OVClientConfig config(std::filesystem::path("ompvoice") / "renderer_test_config.json");
        ov::client::OVDiagnostic diagnostic(std::filesystem::path("ompvoice") / "debug");
        ov::client::OVDx9Renderer renderer(config, audio, bass, diagnostic);
        renderer.ToggleSettings();
        renderer.SetOverlayVisible(true);
        renderer.SetFakeRemote(true);
        const std::array tabs{ov::client::SettingsTab::General, ov::client::SettingsTab::Microphone,
                              ov::client::SettingsTab::Blacklist, ov::client::SettingsTab::Debug};
        for (const auto tab : tabs)
        {
            renderer.SelectSettingsTab(tab);
            for (int frame = 0; frame < 2; ++frame)
            {
                if (frame == 1) renderer.SelectSettingsTab(tab);
                device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0F, 0);
                if (FAILED(device->BeginScene())) { result = 6; break; }
                renderer.OnEndScene(device);
                device->EndScene();
            }
            if (!renderer.IsInitialized() || renderer.LastRenderedSettingsTab() != tab)
            {
                std::cerr << "Settings tab mismatch: expected " << static_cast<int>(tab)
                          << ", rendered " << static_cast<int>(renderer.LastRenderedSettingsTab()) << '\n';
                result = 7;
                break;
            }
        }
        for (int reset = 0; reset < 20 && result == 0; ++reset)
        {
            renderer.OnResetBefore();
            if (FAILED(device->Reset(&parameters))) { result = 8; break; }
            renderer.OnResetAfter(device);
            device->BeginScene();
            renderer.OnEndScene(device);
            device->EndScene();
        }
        renderer.Shutdown();
        bass.Unload();
    }
    const ULONG deviceRefs = device->Release();
    const ULONG d3dRefs = d3d->Release();
    DestroyWindow(window);
    UnregisterClassW(className, instance);
    if (result != 0) return result;
    if (deviceRefs != 0 || d3dRefs != 0) return 9;
    std::cout << "All settings tabs and ImGui DX9 reset lifecycle passed\n";
    return 0;
}
