#include "pch.h"
#include "D3DHook.h"
#include "Nametag.h"

#include <atomic>

namespace {

constexpr size_t kResetIndex = 16;
constexpr size_t kPresentIndex = 17;

using ResetFn = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using PresentFn = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

IDirect3DDevice9* g_Device = nullptr;
ResetFn g_OriginalReset = nullptr;
PresentFn g_OriginalPresent = nullptr;
std::atomic<bool> g_Installed{false};
std::atomic_flag g_Rendering = ATOMIC_FLAG_INIT;

bool IsReadable(const void* address, size_t size) {
    if (!address || reinterpret_cast<uintptr_t>(address) < 0x10000 || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) {
        return false;
    }

    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + size;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool WriteVmtEntry(DWORD* vmt, size_t index, DWORD value) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(&vmt[index], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    vmt[index] = value;
    FlushInstructionCache(GetCurrentProcess(), &vmt[index], sizeof(DWORD));

    DWORD unusedProtect = 0;
    VirtualProtect(&vmt[index], sizeof(DWORD), oldProtect, &unusedProtect);
    return true;
}

HRESULT __stdcall HookedReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* parameters) {
    if (!g_OriginalReset) {
        return D3DERR_INVALIDCALL;
    }

    Nametag::OnLostDevice();
    const HRESULT result = g_OriginalReset(device, parameters);
    if (SUCCEEDED(result)) {
        Nametag::OnResetDevice(device);
    }
    return result;
}

HRESULT __stdcall HookedPresent(IDirect3DDevice9* device, const RECT* sourceRect,
    const RECT* destinationRect, HWND destinationWindow, const RGNDATA* dirtyRegion) {
    if (!g_OriginalPresent) {
        return D3DERR_INVALIDCALL;
    }

    if (g_Rendering.test_and_set(std::memory_order_acquire)) {
        return g_OriginalPresent(device, sourceRect, destinationRect, destinationWindow, dirtyRegion);
    }

    if (device && device->TestCooperativeLevel() == D3D_OK && SUCCEEDED(device->BeginScene())) {
        Nametag::RenderAll(device);
        device->EndScene();
    }
    const HRESULT result = g_OriginalPresent(device, sourceRect, destinationRect, destinationWindow, dirtyRegion);
    g_Rendering.clear(std::memory_order_release);
    return result;
}

} // namespace

void D3DHook::Install(IDirect3DDevice9* device) {
    if (!device || !IsReadable(device, sizeof(void*))) {
        return;
    }

    if (g_Installed.load() && g_Device == device) {
        return;
    }

    if (g_Installed.load()) {
        Uninstall();
    }

    DWORD** vmtPointer = reinterpret_cast<DWORD**>(device);
    if (!IsReadable(vmtPointer, sizeof(*vmtPointer)) ||
        !IsReadable(*vmtPointer, sizeof(DWORD) * (kPresentIndex + 1))) {
        return;
    }

    DWORD* vmt = *vmtPointer;
    ResetFn originalReset = reinterpret_cast<ResetFn>(vmt[kResetIndex]);
    PresentFn originalPresent = reinterpret_cast<PresentFn>(vmt[kPresentIndex]);
    if (!originalReset || !originalPresent) {
        return;
    }

    g_Device = device;
    g_OriginalReset = originalReset;
    g_OriginalPresent = originalPresent;

    if (!WriteVmtEntry(vmt, kResetIndex, reinterpret_cast<DWORD>(&HookedReset))) {
        g_Device = nullptr;
        g_OriginalReset = nullptr;
        g_OriginalPresent = nullptr;
        return;
    }
    if (!WriteVmtEntry(vmt, kPresentIndex, reinterpret_cast<DWORD>(&HookedPresent))) {
        WriteVmtEntry(vmt, kResetIndex, reinterpret_cast<DWORD>(originalReset));
        g_Device = nullptr;
        g_OriginalReset = nullptr;
        g_OriginalPresent = nullptr;
        return;
    }

    g_Installed.store(true);
    Log("D3D hooks installed: device=%p Reset=%p Present=%p", device, originalReset, originalPresent);
}

void D3DHook::Uninstall() {
    if (!g_Installed.exchange(false)) {
        return;
    }

    if (g_Device && IsReadable(g_Device, sizeof(void*))) {
        DWORD** vmtPointer = reinterpret_cast<DWORD**>(g_Device);
        if (IsReadable(vmtPointer, sizeof(*vmtPointer)) &&
            IsReadable(*vmtPointer, sizeof(DWORD) * (kPresentIndex + 1))) {
            DWORD* vmt = *vmtPointer;
            if (vmt[kResetIndex] == reinterpret_cast<DWORD>(&HookedReset) && g_OriginalReset) {
                WriteVmtEntry(vmt, kResetIndex, reinterpret_cast<DWORD>(g_OriginalReset));
            }
            if (vmt[kPresentIndex] == reinterpret_cast<DWORD>(&HookedPresent) && g_OriginalPresent) {
                WriteVmtEntry(vmt, kPresentIndex, reinterpret_cast<DWORD>(g_OriginalPresent));
            }
        }
    }

    Nametag::Shutdown();
    g_Device = nullptr;
    g_OriginalReset = nullptr;
    g_OriginalPresent = nullptr;
}

bool D3DHook::IsInstalled() {
    return g_Installed.load();
}
