/**
 * @file D3DHook.cpp
 * @brief Implementation D3D9 EndScene hook.
 */
#include "pch.h"
#include "D3DHook.h"
#include "Nametag.h"

// ---------------------------------------------------------------------------
// VMT index
// ---------------------------------------------------------------------------

#include <unordered_map>

// ---------------------------------------------------------------------------
// VMT index
// ---------------------------------------------------------------------------

constexpr int kPresentVmtIndex  = 17;
constexpr int kEndSceneVmtIndex = 42;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

using tPresent  = HRESULT(__stdcall*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using tEndScene = HRESULT(__stdcall*)(IDirect3DDevice9*);

static std::unordered_map<IDirect3DDevice9*, tPresent>  s_OrigPresentMap;
static std::unordered_map<IDirect3DDevice9*, tEndScene> s_OrigEndSceneMap;
static bool s_Hooked = false;

// ---------------------------------------------------------------------------
// Render state helpers
// ---------------------------------------------------------------------------

static void SetupRenderState(IDirect3DDevice9* dev) {
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
    dev->SetRenderState(D3DRS_LIGHTING,         FALSE);
    dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

struct SavedState {
    DWORD alphaBlend, srcBlend, destBlend;
    DWORD zEnable, zWriteEnable;
    DWORD lighting, cullMode, scissor, fog;
    DWORD fvf;
    IDirect3DBaseTexture9* tex0;
};

static void SaveState(IDirect3DDevice9* dev, SavedState& s) {
    s.tex0 = nullptr;
    dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &s.alphaBlend);
    dev->GetRenderState(D3DRS_SRCBLEND,         &s.srcBlend);
    dev->GetRenderState(D3DRS_DESTBLEND,        &s.destBlend);
    dev->GetRenderState(D3DRS_ZENABLE,          &s.zEnable);
    dev->GetRenderState(D3DRS_ZWRITEENABLE,     &s.zWriteEnable);
    dev->GetRenderState(D3DRS_LIGHTING,         &s.lighting);
    dev->GetRenderState(D3DRS_CULLMODE,         &s.cullMode);
    dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &s.scissor);
    dev->GetRenderState(D3DRS_FOGENABLE,        &s.fog);
    dev->GetFVF(&s.fvf);
    dev->GetTexture(0, &s.tex0);
}

static void RestoreState(IDirect3DDevice9* dev, const SavedState& s) {
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, s.alphaBlend);
    dev->SetRenderState(D3DRS_SRCBLEND,         s.srcBlend);
    dev->SetRenderState(D3DRS_DESTBLEND,        s.destBlend);
    dev->SetRenderState(D3DRS_ZENABLE,          s.zEnable);
    dev->SetRenderState(D3DRS_ZWRITEENABLE,     s.zWriteEnable);
    dev->SetRenderState(D3DRS_LIGHTING,         s.lighting);
    dev->SetRenderState(D3DRS_CULLMODE,         s.cullMode);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, s.scissor);
    dev->SetRenderState(D3DRS_FOGENABLE,        s.fog);
    dev->SetFVF(s.fvf);
    dev->SetTexture(0, s.tex0);
    if (s.tex0) s.tex0->Release();
}

static void DoRender(IDirect3DDevice9* dev) {
    if (!dev || IsBadReadPtr(dev, sizeof(void*))) return;

    // Kiểm tra device status (tránh vẽ khi đang load / alt-tab / reset màn hình đen)
    HRESULT hr = dev->TestCooperativeLevel();
    if (hr != D3D_OK) {
        Nametag::Release();
        return;
    }

    Nametag::Init(dev);

    SavedState s;
    SaveState(dev, s);

    SetupRenderState(dev);
    Nametag::RenderAll(dev);

    RestoreState(dev, s);
}

// ---------------------------------------------------------------------------
// Hook functions
// ---------------------------------------------------------------------------

static HRESULT __stdcall hkPresent(IDirect3DDevice9* dev,
    const RECT* pSourceRect, const RECT* pDestRect,
    HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
    tPresent origP = NULL;
    auto it = s_OrigPresentMap.find(dev);
    if (it != s_OrigPresentMap.end()) {
        origP = it->second;
    }

    DWORD* currentVmt = *reinterpret_cast<DWORD**>(dev);
    if (currentVmt && !IsBadReadPtr(currentVmt, sizeof(void*) * 18) &&
        currentVmt[kPresentVmtIndex] != reinterpret_cast<DWORD>(hkPresent))
    {
        origP = reinterpret_cast<tPresent>(currentVmt[kPresentVmtIndex]);
        s_OrigPresentMap[dev] = origP;
        DWORD oldProtect;
        VirtualProtect(&currentVmt[kPresentVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
        currentVmt[kPresentVmtIndex] = reinterpret_cast<DWORD>(hkPresent);
        VirtualProtect(&currentVmt[kPresentVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
    }

    DoRender(dev);

    return origP ? origP(dev, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion) : D3D_OK;
}

static HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev) {
    tEndScene origE = NULL;
    auto it = s_OrigEndSceneMap.find(dev);
    if (it != s_OrigEndSceneMap.end()) {
        origE = it->second;
    }

    DWORD* currentVmt = *reinterpret_cast<DWORD**>(dev);
    if (currentVmt && !IsBadReadPtr(currentVmt, sizeof(void*) * 43) &&
        currentVmt[kEndSceneVmtIndex] != reinterpret_cast<DWORD>(hkEndScene))
    {
        origE = reinterpret_cast<tEndScene>(currentVmt[kEndSceneVmtIndex]);
        s_OrigEndSceneMap[dev] = origE;
        DWORD oldProtect;
        VirtualProtect(&currentVmt[kEndSceneVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
        currentVmt[kEndSceneVmtIndex] = reinterpret_cast<DWORD>(hkEndScene);
        VirtualProtect(&currentVmt[kEndSceneVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
    }

    return origE ? origE(dev) : D3D_OK;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void D3DHook::Install(IDirect3DDevice9* dev) {
    if (!dev || IsBadReadPtr(dev, sizeof(void*))) return;

    DWORD* vmt = *reinterpret_cast<DWORD**>(dev);
    if (!vmt || IsBadReadPtr(vmt, sizeof(void*) * 43)) return;

    DWORD oldProtect;
    if (vmt[kPresentVmtIndex] != reinterpret_cast<DWORD>(hkPresent)) {
        tPresent origP = reinterpret_cast<tPresent>(vmt[kPresentVmtIndex]);
        s_OrigPresentMap[dev] = origP;
        Log("D3DHook::Install Present on dev=%p, original Present=%p", dev, (void*)origP);

        VirtualProtect(&vmt[kPresentVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
        vmt[kPresentVmtIndex] = reinterpret_cast<DWORD>(hkPresent);
        VirtualProtect(&vmt[kPresentVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
    }

    if (vmt[kEndSceneVmtIndex] != reinterpret_cast<DWORD>(hkEndScene)) {
        tEndScene origE = reinterpret_cast<tEndScene>(vmt[kEndSceneVmtIndex]);
        s_OrigEndSceneMap[dev] = origE;
        Log("D3DHook::Install EndScene on dev=%p, original EndScene=%p", dev, (void*)origE);

        VirtualProtect(&vmt[kEndSceneVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
        vmt[kEndSceneVmtIndex] = reinterpret_cast<DWORD>(hkEndScene);
        VirtualProtect(&vmt[kEndSceneVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
    }

    s_Hooked = true;
    Log("D3DHook::Install success on dev=%p!", dev);
}

void D3DHook::Uninstall() {
    for (auto& pair : s_OrigPresentMap) {
        IDirect3DDevice9* dev = pair.first;
        tPresent origP = pair.second;
        if (dev && !IsBadReadPtr(dev, sizeof(void*)) && origP) {
            DWORD* vmt = *reinterpret_cast<DWORD**>(dev);
            if (vmt && !IsBadReadPtr(vmt, sizeof(void*) * 43)) {
                DWORD oldProtect;
                VirtualProtect(&vmt[kPresentVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
                vmt[kPresentVmtIndex] = reinterpret_cast<DWORD>(origP);
                VirtualProtect(&vmt[kPresentVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
            }
        }
    }
    for (auto& pair : s_OrigEndSceneMap) {
        IDirect3DDevice9* dev = pair.first;
        tEndScene origE = pair.second;
        if (dev && !IsBadReadPtr(dev, sizeof(void*)) && origE) {
            DWORD* vmt = *reinterpret_cast<DWORD**>(dev);
            if (vmt && !IsBadReadPtr(vmt, sizeof(void*) * 43)) {
                DWORD oldProtect;
                VirtualProtect(&vmt[kEndSceneVmtIndex], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
                vmt[kEndSceneVmtIndex] = reinterpret_cast<DWORD>(origE);
                VirtualProtect(&vmt[kEndSceneVmtIndex], sizeof(DWORD), oldProtect, &oldProtect);
            }
        }
    }
    s_OrigPresentMap.clear();
    s_OrigEndSceneMap.clear();
    s_Hooked = false;
}

bool D3DHook::IsInstalled() {
    return s_Hooked;
}

