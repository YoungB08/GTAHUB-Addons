#include "pch.h"
#include <windows.h>
#include <psapi.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <thread>
#include <map>
#include <mutex>
#include <urlmon.h>
#include <0.3.DL-1/CNetGame.h>
#include <0.3.DL-1/CPlayerTags.h>
#include <0.3.DL-1/CRemotePlayer.h>
#include <0.3.DL-1/CPlayerPool.h>
#include <0.3.DL-1/CPed.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "urlmon.lib")

using namespace sampapi::v03dl;

// ============================================================
// VEHICLE LIMIT PATCH
// ============================================================

DWORD FindAddress(HMODULE hModule, const char* pattern, size_t patternSize) {
    MODULEINFO moduleInfo;
    DWORD baseAddress = reinterpret_cast<DWORD>(hModule);
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));
    for (DWORD i = baseAddress; i < baseAddress + moduleInfo.SizeOfImage - patternSize; ++i) {
        if (memcmp(reinterpret_cast<void*>(i), pattern, patternSize) == 0) return i;
    }
    return 0;
}

void MemoryFill(DWORD address, BYTE value, size_t size) {
    DWORD oldProtect;
    VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);
    memset((void*)address, value, size);
    VirtualProtect((LPVOID)address, size, oldProtect, &oldProtect);
}

void PatchVehicleLimit() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return;
    DWORD patternAddr = FindAddress(hSamp,
        "\x3D\x90\x01\x00\x00\x0F\x8C\x32\x01\x00\x00\x3D\x63\x02\x00\x00\x0F\x8F\x27\x01\x00\x00", 22);
    if (patternAddr) {
        DWORD addrLimit = patternAddr + 11 + 1;
        DWORD newLimit = 8000;
        DWORD oldProtect;
        VirtualProtect((LPVOID)addrLimit, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)addrLimit, &newLimit, 4);
        VirtualProtect((LPVOID)addrLimit, 4, oldProtect, &oldProtect);
    }
}

// ============================================================
// WORLD TO SCREEN
// ============================================================

typedef bool(__cdecl* tCalcScreenCoors)(sampapi::CVector* pWorld, sampapi::CVector* pScreen, float* w, float* h);
static tCalcScreenCoors CalcScreenCoors = (tCalcScreenCoors)0x71DA00;

static bool WorldToScreen(sampapi::CVector worldPos, float& screenX, float& screenY) {
    sampapi::CVector screen;
    float w = 0.0f, h = 0.0f;
    if (!CalcScreenCoors(&worldPos, &screen, &w, &h)) return false;
    if (w < 0.0f || h < 0.0f) return false;
    screenX = screen.x;
    screenY = screen.y;
    return true;
}

// ============================================================
// NAMETAG DRAW HELPERS
// ============================================================

struct NTVertex {
    float x, y, z, rhw;
    D3DCOLOR color;
};
#define NT_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

static void DrawFilledRect(IDirect3DDevice9* pDevice, float x, float y, float w, float h, D3DCOLOR color) {
    NTVertex v[4] = {
        { x,     y + h, 0.0f, 1.0f, color },
        { x,     y,     0.0f, 1.0f, color },
        { x + w, y + h, 0.0f, 1.0f, color },
        { x + w, y,     0.0f, 1.0f, color }
    };
    pDevice->SetFVF(NT_FVF);
    pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(NTVertex));
}

static void DrawBorderRect(IDirect3DDevice9* pDevice, float x, float y, float w, float h, float t, D3DCOLOR color) {
    DrawFilledRect(pDevice, x, y, w, t, color);
    DrawFilledRect(pDevice, x, y + h - t, w, t, color);
    DrawFilledRect(pDevice, x, y, t, h, color);
    DrawFilledRect(pDevice, x + w - t, y, t, h, color);
}

static void DrawTextStroke(ID3DXFont* pFont, const char* text, RECT rect, DWORD fmt, D3DCOLOR textColor, D3DCOLOR strokeColor) {
    int offsets[8][2] = {{-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}};
    for (int i = 0; i < 8; i++) {
        RECT r = { rect.left + offsets[i][0], rect.top + offsets[i][1],
                   rect.right + offsets[i][0], rect.bottom + offsets[i][1] };
        pFont->DrawTextA(NULL, text, -1, &r, fmt, strokeColor);
    }
    pFont->DrawTextA(NULL, text, -1, &rect, fmt, textColor);
}

// ============================================================
// TEXTURE CACHE (Thread-safe: download async, create on render thread)
// ============================================================

static std::map<std::string, LPDIRECT3DTEXTURE9> g_TextureCache;
static std::map<std::string, std::string>         g_PendingTextures; // url -> local path (ready to create)
static std::mutex g_TextureMutex;

static void DownloadTextureAsync(const std::string& url) {
    std::thread([url]() {
        char cachePath[MAX_PATH];
        if (SUCCEEDED(URLDownloadToCacheFileA(NULL, url.c_str(), cachePath, MAX_PATH, 0, NULL))) {
            std::lock_guard<std::mutex> lock(g_TextureMutex);
            if (!g_TextureCache.count(url) && !g_PendingTextures.count(url))
                g_PendingTextures[url] = cachePath;
        }
    }).detach();
}

// Call from render thread each frame to finalize pending textures
static void FlushPendingTextures(IDirect3DDevice9* pDevice) {
    std::lock_guard<std::mutex> lock(g_TextureMutex);
    for (auto it = g_PendingTextures.begin(); it != g_PendingTextures.end(); ) {
        LPDIRECT3DTEXTURE9 pTex = NULL;
        if (SUCCEEDED(D3DXCreateTextureFromFileA(pDevice, it->second.c_str(), &pTex)))
            g_TextureCache[it->first] = pTex;
        it = g_PendingTextures.erase(it);
    }
}

static LPDIRECT3DTEXTURE9 GetTexture(IDirect3DDevice9* pDevice, const std::string& url) {
    if (url.empty()) return NULL;
    {
        std::lock_guard<std::mutex> lock(g_TextureMutex);
        auto it = g_TextureCache.find(url);
        if (it != g_TextureCache.end()) return it->second;
    }
    DownloadTextureAsync(url);
    return NULL;
}

// ============================================================
// D3D RESOURCES
// ============================================================

static ID3DXFont*   pFontName  = NULL;
static ID3DXFont*   pFontInfo  = NULL;
static ID3DXFont*   pFontBadge = NULL;
static ID3DXSprite* pSprite    = NULL;
static bool         g_bFontsReady = false;

static void InitFonts(IDirect3DDevice9* pDevice) {
    D3DXCreateFontA(pDevice, 18, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &pFontName);
    D3DXCreateFontA(pDevice, 11, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &pFontInfo);
    D3DXCreateFontA(pDevice, 10, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &pFontBadge);
    D3DXCreateSprite(pDevice, &pSprite);
    g_bFontsReady = true;
}

static void ReleaseFonts() {
    if (pFontName)  { pFontName->Release();  pFontName  = NULL; }
    if (pFontInfo)  { pFontInfo->Release();  pFontInfo  = NULL; }
    if (pFontBadge) { pFontBadge->Release(); pFontBadge = NULL; }
    if (pSprite)    { pSprite->Release();    pSprite    = NULL; }
    g_bFontsReady = false;
}

// ============================================================
// RENDER ONE NAMETAG
// ============================================================

static void RenderSAMPNametag(IDirect3DDevice9* pDevice,
    float centerX, float topY,
    const char* name, int id, int ping,
    float hpPercent, float armorPercent,
    bool isAdmin, bool isVIP,
    const std::string& iconUrl)
{
    if (!pFontName || !pFontInfo || !pFontBadge || !pSprite) return;

    float currentY = topY;

    // ── HÀNG 1: BADGES ──────────────────────────────────────
    const float badgeW = 45.0f, badgeH = 16.0f, iconSize = 20.0f, gap = 6.0f;
    float totalW = badgeW + gap + iconSize + gap + badgeW;
    float bx = centerX - totalW * 0.5f;

    // Badge ADMIN
    if (isAdmin) {
        DrawFilledRect(pDevice, bx, currentY, badgeW, badgeH, D3DCOLOR_ARGB(230, 179, 0, 0));
        DrawBorderRect(pDevice, bx, currentY, badgeW, badgeH, 1.0f, D3DCOLOR_ARGB(255, 0, 0, 0));
        RECT r = { (LONG)bx, (LONG)currentY, (LONG)(bx + badgeW), (LONG)(currentY + badgeH) };
        pFontBadge->DrawTextA(NULL, "ADMIN", -1, &r, DT_CENTER | DT_VCENTER, D3DCOLOR_ARGB(255, 255, 255, 255));
    }

    // Icon
    float iconX = bx + badgeW + gap;
    LPDIRECT3DTEXTURE9 pIconTex = GetTexture(pDevice, iconUrl);
    if (pIconTex) {
        pSprite->Begin(D3DXSPRITE_ALPHABLEND);
        D3DXMATRIX mat;
        float scale = iconSize / 32.0f; // assume 32x32 source
        D3DXMatrixTransformation2D(&mat, NULL, 0.0f, &D3DXVECTOR2(scale, scale), NULL, 0.0f, &D3DXVECTOR2(iconX, currentY));
        pSprite->SetTransform(&mat);
        pSprite->Draw(pIconTex, NULL, NULL, NULL, D3DCOLOR_ARGB(255, 255, 255, 255));
        pSprite->End();
    } else {
        DrawFilledRect(pDevice, iconX, currentY, iconSize, iconSize, D3DCOLOR_ARGB(100, 0, 0, 0));
    }
    DrawBorderRect(pDevice, iconX, currentY, iconSize, iconSize, 1.0f, D3DCOLOR_ARGB(180, 0, 0, 0));

    // Badge VIP
    float vipX = iconX + iconSize + gap;
    if (isVIP) {
        DrawFilledRect(pDevice, vipX, currentY, badgeW, badgeH, D3DCOLOR_ARGB(230, 204, 153, 0));
        DrawBorderRect(pDevice, vipX, currentY, badgeW, badgeH, 1.0f, D3DCOLOR_ARGB(255, 0, 0, 0));
        RECT r = { (LONG)vipX, (LONG)currentY, (LONG)(vipX + badgeW), (LONG)(currentY + badgeH) };
        pFontBadge->DrawTextA(NULL, "VIP", -1, &r, DT_CENTER | DT_VCENTER, D3DCOLOR_ARGB(255, 255, 255, 255));
    }

    currentY += badgeH + gap;

    // ── HÀNG 2: TÊN (STROKE) ────────────────────────────────
    SIZE nameSize = {};
    if (pFontName) {
        RECT tmp = { 0, 0, 500, 50 };
        pFontName->DrawTextA(NULL, name, -1, &tmp, DT_CALCRECT, 0);
        nameSize.cx = tmp.right - tmp.left;
        nameSize.cy = tmp.bottom - tmp.top;
    }
    float nameX = centerX - nameSize.cx * 0.5f;
    RECT nameRect = { (LONG)nameX, (LONG)currentY, (LONG)(nameX + nameSize.cx + 10), (LONG)(currentY + nameSize.cy) };
    DrawTextStroke(pFontName, name, nameRect, DT_LEFT | DT_NOCLIP,
        D3DCOLOR_ARGB(255, 51, 204, 255),
        D3DCOLOR_ARGB(255, 0, 0, 0));

    currentY += nameSize.cy + 4.0f;

    // ── HÀNG 3: PROGRESS BARS ───────────────────────────────
    const float barTotalW = 180.0f, barH = 9.0f;
    float singleW = (barTotalW - gap) * 0.5f;
    float barsX = centerX - barTotalW * 0.5f;

    float hpFill   = max(0.0f, min(1.0f, hpPercent   / 100.0f));
    float arFill   = max(0.0f, min(1.0f, armorPercent / 100.0f));

    // HP bar
    DrawFilledRect(pDevice, barsX, currentY, singleW, barH, D3DCOLOR_ARGB(200, 0, 0, 0));
    DrawFilledRect(pDevice, barsX + 1, currentY + 1, (singleW - 2) * hpFill, barH - 2, D3DCOLOR_ARGB(255, 210, 30, 30));
    DrawBorderRect(pDevice, barsX, currentY, singleW, barH, 1.0f, D3DCOLOR_ARGB(255, 0, 0, 0));

    // Armour bar
    float arX = barsX + singleW + gap;
    DrawFilledRect(pDevice, arX, currentY, singleW, barH, D3DCOLOR_ARGB(200, 0, 0, 0));
    DrawFilledRect(pDevice, arX + 1, currentY + 1, (singleW - 2) * arFill, barH - 2, D3DCOLOR_ARGB(255, 180, 180, 180));
    DrawBorderRect(pDevice, arX, currentY, singleW, barH, 1.0f, D3DCOLOR_ARGB(255, 0, 0, 0));

    currentY += barH + gap;

    // ── HÀNG 4: CAPSULE INFO ────────────────────────────────
    char info[64];
    sprintf_s(info, "ID: %d  |  %dms", id, ping);

    const float capW = 120.0f, capH = 18.0f;
    float capX = centerX - capW * 0.5f;
    DrawFilledRect(pDevice, capX, currentY, capW, capH, D3DCOLOR_ARGB(200, 0, 0, 0));
    DrawBorderRect(pDevice, capX, currentY, capW, capH, 1.0f, D3DCOLOR_ARGB(180, 255, 255, 255));
    RECT infoRect = { (LONG)capX, (LONG)currentY, (LONG)(capX + capW), (LONG)(currentY + capH) };
    pFontInfo->DrawTextA(NULL, info, -1, &infoRect, DT_CENTER | DT_VCENTER, D3DCOLOR_ARGB(255, 51, 204, 255));
}

// ============================================================
// RENDER ALL NAMETAGS (called each frame)
// ============================================================

static void RenderAllNametags(IDirect3DDevice9* pDevice) {
    if (!g_bFontsReady) return;

    CNetGame* pNetGame = RefNetGame();
    if (!pNetGame) return;
    CPlayerPool* pPool = pNetGame->GetPlayerPool();
    if (!pPool) return;

    // Get screen size for bounds check
    D3DVIEWPORT9 vp;
    pDevice->GetViewport(&vp);

    FlushPendingTextures(pDevice);

    // Save D3D state
    IDirect3DStateBlock9* pState = NULL;
    pDevice->CreateStateBlock(D3DSBT_ALL, &pState);

    pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    pDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
    pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    pDevice->SetTextureStageState(0, D3DTSS_COLOROP,  D3DTOP_SELECTARG1);
    pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,  D3DTOP_SELECTARG1);
    pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    pDevice->SetTexture(0, NULL);

    ID nLocalId = pPool->m_nLocalPlayerId;

    for (int i = 0; i < 1004; i++) {
        if (i == nLocalId) continue;
        if (!pPool->IsConnected(i)) continue;

        CRemotePlayer* pPlayer = pPool->GetPlayer(i);
        if (!pPlayer || !pPlayer->m_pPed) continue;

        // Head position
        sampapi::CVector headPos;
        pPlayer->m_pPed->GetBonePosition(8, &headPos); // 8 = BONE_HEAD
        headPos.z += 0.25f; // offset lên trên đầu

        float sx = 0.0f, sy = 0.0f;
        if (!WorldToScreen(headPos, sx, sy)) continue;

        // Bounds check
        if (sx < 0.0f || sx >(float)vp.Width) continue;
        if (sy < 0.0f || sy >(float)vp.Height) continue;

        const char* name     = pPool->GetName(i);
        int         ping     = pPool->GetPing(i);
        float       hp       = max(0.0f, min(100.0f, pPlayer->m_fReportedHealth));
        float       armour   = max(0.0f, min(100.0f, pPlayer->m_fReportedArmour));

        RenderSAMPNametag(pDevice, sx, sy, name ? name : "?", i, ping, hp, armour,
            false, false, ""); // isAdmin/isVIP/iconUrl: mở rộng sau
    }

    if (pState) { pState->Apply(); pState->Release(); }
}

// ============================================================
// D3D9 ENDSCENE HOOK (VMT)
// ============================================================

typedef HRESULT(__stdcall* tEndScene)(IDirect3DDevice9*);
static tEndScene oEndScene  = NULL;
static bool      g_bHooked  = false;

static HRESULT __stdcall hkEndScene(IDirect3DDevice9* pDevice) {
    if (!g_bFontsReady) InitFonts(pDevice);

    RenderAllNametags(pDevice);

    return oEndScene(pDevice);
}

static void InstallEndSceneHook(IDirect3DDevice9* pDevice) {
    DWORD* vmt = *(DWORD**)pDevice;
    oEndScene = (tEndScene)vmt[42];
    DWORD old;
    VirtualProtect(&vmt[42], 4, PAGE_EXECUTE_READWRITE, &old);
    vmt[42] = (DWORD)hkEndScene;
    VirtualProtect(&vmt[42], 4, old, &old);
}

// ============================================================
// THREADS
// ============================================================

DWORD WINAPI CheckHostAddressThread(LPVOID) {
    while (true) { Sleep(500); }
    return 0;
}

DWORD WINAPI InitializeAndLoad(LPVOID) {
    while (*reinterpret_cast<unsigned char*>(0xC8D4C0) != 9) Sleep(100);
    CreateThread(0, 0, &CheckHostAddressThread, 0, 0, 0);
    return 0;
}

DWORD WINAPI MainThread(LPVOID) {
    PatchVehicleLimit();

    // Chờ SAMP init xong và có D3D device
    while (true) {
        if (sampapi::GetBase() && RefPlayerTags() && RefPlayerTags()->m_pDevice && !g_bHooked) {
            InstallEndSceneHook(RefPlayerTags()->m_pDevice);
            g_bHooked = true;
            break;
        }
        Sleep(500);
    }
    return 0;
}

// ============================================================
// DLLMAIN
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        CreateThread(0, 0, &InitializeAndLoad, 0, 0, 0);
        break;
    case DLL_PROCESS_DETACH:
        ReleaseFonts();
        break;
    }
    return TRUE;
}
