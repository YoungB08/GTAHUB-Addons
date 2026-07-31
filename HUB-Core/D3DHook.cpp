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

/// Index của EndScene trong IDirect3DDevice9 VMT (D3D9 COM spec)
constexpr int kEndSceneVmtIndex = 42;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

using tEndScene = HRESULT(__stdcall*)(IDirect3DDevice9*);

static tEndScene  s_Original = NULL; ///< Pointer EndScene gốc
static DWORD*     s_VMT      = NULL; ///< VMT của device (để uninstall)
static bool       s_Hooked   = false;

// ---------------------------------------------------------------------------
// Render state helpers
// ---------------------------------------------------------------------------

/**
 * @brief Thiết lập render state phù hợp cho vẽ 2D.
 *
 * Cần set trước khi gọi DrawPrimitiveUP để:
 *  - Alpha blending hoạt động đúng
 *  - Không bị depth test chặn
 *  - Lighting không ảnh hưởng màu vertex
 *  - Texture stage không lọc màu diffuse
 */
static void SetupRenderState(IDirect3DDevice9* dev) {
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_ZENABLE,          FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE,     FALSE);
    dev->SetRenderState(D3DRS_LIGHTING,         FALSE);
    dev->SetRenderState(D3DRS_CULLMODE,         D3DCULL_NONE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    dev->SetTexture(0, NULL);
}

// ---------------------------------------------------------------------------
// Hook function
// ---------------------------------------------------------------------------

/**
 * @brief Thay thế EndScene. Chạy mỗi frame trên render thread.
 *
 * Thứ tự thực hiện:
 *  1. Lazy-init Nametag resources (chỉ lần đầu)
 *  2. Capture toàn bộ D3D state vào StateBlock
 *  3. Setup render state 2D
 *  4. Render tất cả nametag
 *  5. Restore D3D state từ StateBlock
 *  6. Gọi EndScene gốc (của SAMP hoặc D3D9)
 */
static HRESULT __stdcall hkEndScene(IDirect3DDevice9* dev) {
    // Lazy-init font/sprite
    Nametag::Init(dev);

    // Capture state
    IDirect3DStateBlock9* pState = NULL;
    dev->CreateStateBlock(D3DSBT_ALL, &pState);

    SetupRenderState(dev);
    Nametag::RenderAll(dev);

    // Restore state
    if (pState) {
        pState->Apply();
        pState->Release();
    }

    return s_Original(dev);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void D3DHook::Install(IDirect3DDevice9* dev) {
    if (s_Hooked) return;

    // Lấy VMT của device object
    s_VMT = *reinterpret_cast<DWORD**>(dev);

    // Lưu original EndScene
    s_Original = reinterpret_cast<tEndScene>(s_VMT[kEndSceneVmtIndex]);

    // Patch VMT[42] = hkEndScene (cần VirtualProtect vì VMT nằm trong .rdata)
    DWORD oldProtect;
    VirtualProtect(&s_VMT[kEndSceneVmtIndex], sizeof(DWORD),
        PAGE_EXECUTE_READWRITE, &oldProtect);
    s_VMT[kEndSceneVmtIndex] = reinterpret_cast<DWORD>(hkEndScene);
    VirtualProtect(&s_VMT[kEndSceneVmtIndex], sizeof(DWORD),
        oldProtect, &oldProtect);

    s_Hooked = true;
}

void D3DHook::Uninstall() {
    if (!s_Hooked || !s_VMT) return;

    DWORD oldProtect;
    VirtualProtect(&s_VMT[kEndSceneVmtIndex], sizeof(DWORD),
        PAGE_EXECUTE_READWRITE, &oldProtect);
    s_VMT[kEndSceneVmtIndex] = reinterpret_cast<DWORD>(s_Original);
    VirtualProtect(&s_VMT[kEndSceneVmtIndex], sizeof(DWORD),
        oldProtect, &oldProtect);

    s_Hooked   = false;
    s_Original = NULL;
    s_VMT      = NULL;
}

bool D3DHook::IsInstalled() {
    return s_Hooked;
}
