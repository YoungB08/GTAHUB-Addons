/**
 * @file Nametag.cpp
 * @brief Render nametag động cho tất cả player mỗi frame.
 */
#include "pch.h"
#include "Nametag.h"
#include "D3DHelper.h"
#include "W2S.h"
#include "TextureCache.h"
#include "PlayerData.h"
#include "RoleConfig.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>
#include <cmath>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

using namespace sampapi::v03dl;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

constexpr D3DCOLOR kColorName          = D3DCOLOR_ARGB(255,  51, 204, 255); ///< Cyan tên
constexpr D3DCOLOR kColorNameStroke    = D3DCOLOR_ARGB(255,   0,   0,   0); ///< Stroke đen
constexpr D3DCOLOR kColorTagText       = D3DCOLOR_ARGB(255, 255, 255, 255); ///< Text badge
constexpr D3DCOLOR kColorTagStroke     = D3DCOLOR_ARGB(255,   0,   0,   0); ///< Stroke badge
constexpr D3DCOLOR kColorHpFill        = D3DCOLOR_ARGB(255, 210,  30,  30); ///< Đỏ HP
constexpr D3DCOLOR kColorArmourFill    = D3DCOLOR_ARGB(255, 180, 180, 180); ///< Bạc Armour
constexpr D3DCOLOR kColorBarBg         = D3DCOLOR_ARGB(200,   0,   0,   0); ///< Nền bar/capsule
constexpr D3DCOLOR kColorBarBorder     = D3DCOLOR_ARGB(255,   0,   0,   0); ///< Viền bar
constexpr D3DCOLOR kColorCapsuleBorder = D3DCOLOR_ARGB(180, 255, 255, 255); ///< Viền capsule
constexpr D3DCOLOR kColorInfoText      = D3DCOLOR_ARGB(255,  51, 204, 255); ///< Cyan ID/ping

constexpr float kGap       = 5.f;  ///< Khoảng cách dọc giữa các hàng (px)
constexpr float kTagPadX   = 8.f;  ///< Padding ngang trong tag badge
constexpr float kTagH      = 16.f; ///< Chiều cao cố định của tag badge
constexpr float kTagGap    = 5.f;  ///< Khoảng cách ngang giữa 2 tag cùng hàng
constexpr float kIconSize  = 20.f; ///< Kích thước icon (px)
constexpr float kBarTotalW = 180.f;///< Tổng chiều rộng 2 progress bar
constexpr float kBarH      =   9.f;///< Chiều cao progress bar
constexpr float kCapW      = 130.f;///< Chiều rộng capsule info
constexpr float kCapH      =  18.f;///< Chiều cao capsule info
constexpr UINT  kBoneHead  =   8;  ///< Bone ID đầu player (GTA SA)
constexpr float kHeadOffZ  = 0.28f;///< Offset lên trên đầu (tránh đè model)

// ---------------------------------------------------------------------------
// D3D resources
// ---------------------------------------------------------------------------

static ID3DXFont*   s_FontName      = NULL;
static ID3DXFont*   s_FontInfo      = NULL;
static ID3DXFont*   s_FontTag       = NULL;
static ID3DXFont*   s_FontNameSmall = NULL;
static ID3DXFont*   s_FontInfoSmall = NULL;
static ID3DXFont*   s_FontTagSmall  = NULL;
static ID3DXSprite* s_Sprite        = NULL;
static bool         s_Ready         = false;

// ---------------------------------------------------------------------------
// Internal draw helpers
// ---------------------------------------------------------------------------

/**
 * @brief Vẽ 1 tag badge với kích thước co giãn theo distance scale.
 */
/**
 * @brief Vẽ 1 tag badge bo tròn góc với kích thước co giãn theo distance scale.
 */
static float DrawTag(IDirect3DDevice9* dev, float x, float y, float scale, const RoleTag& tag) {
    ID3DXFont* font = (scale < 0.75f) ? s_FontTagSmall : s_FontTag;
    SIZE sz = D3DHelper::MeasureText(font, tag.text.c_str());

    float tagPadX = kTagPadX * scale;
    float tagH    = kTagH * scale;
    float tagW    = static_cast<float>(sz.cx) + tagPadX * 2.f;
    float radius  = 4.f * scale;

    D3DHelper::DrawRoundedFilledRect(dev, x, y, tagW, tagH, radius, tag.color);
    D3DHelper::DrawRoundedBorderRect(dev, x, y, tagW, tagH, radius, 1.f, kColorTagStroke);

    RECT r = { (LONG)x, (LONG)y, (LONG)(x + tagW), (LONG)(y + tagH) };
    if (tag.stroke) {
        D3DHelper::DrawTextStroke(font, tag.text.c_str(), r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            kColorTagText, kColorTagStroke);
    } else {
        font->DrawTextA(NULL, tag.text.c_str(), -1, &r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, kColorTagText);
    }

    return tagW;
}

/**
 * @brief Vẽ PNG Image texture (tự scale theo targetW x targetH).
 */
static void DrawIcon(IDirect3DDevice9* dev, float x, float y, float targetW, float targetH, const std::string& url) {
    LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, url);
    if (!tex) return;

    D3DSURFACE_DESC desc;
    tex->GetLevelDesc(0, &desc);
    if (desc.Width == 0 || desc.Height == 0) return;

    float scaleX = targetW / static_cast<float>(desc.Width);
    float scaleY = targetH / static_cast<float>(desc.Height);

    D3DXMATRIX mat;
    D3DXVECTOR2 scaleVec(scaleX, scaleY);
    D3DXVECTOR2 posVec(x, y);
    D3DXMatrixTransformation2D(&mat, NULL, 0.f, &scaleVec, NULL, 0.f, &posVec);

    s_Sprite->Begin(D3DXSPRITE_ALPHABLEND);
    s_Sprite->SetTransform(&mat);
    s_Sprite->Draw(tex, NULL, NULL, NULL, D3DCOLOR_ARGB(255, 255, 255, 255));
    s_Sprite->End();
}

/**
 * @brief Vẽ hàng tag + icon với tỉ lệ scale theo khoảng cách.
 */
static float DrawTagRow(IDirect3DDevice9* dev, float centerX, float y, float scale, const PlayerNametag& pn) {
    if (pn.tagCount == 0 && pn.iconUrl.empty()) return 0.f;

    // 1. Ưu tiên vẽ PNG Image Role Badge nếu được cấu hình iconUrl
    if (!pn.iconUrl.empty()) {
        LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, pn.iconUrl);
        if (tex) {
            D3DSURFACE_DESC desc;
            tex->GetLevelDesc(0, &desc);
            if (desc.Width > 0 && desc.Height > 0) {
                float badgeH = 22.f * scale;
                float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
                float badgeW = badgeH * aspect;
                float x = centerX - badgeW * 0.5f;

                DrawIcon(dev, x, y, badgeW, badgeH, pn.iconUrl);
                return badgeH;
            }
        }
    }

    // 2. Nếu không dùng PNG Image -> Vẽ Text Badge bo tròn góc
    ID3DXFont* font = (scale < 0.75f) ? s_FontTagSmall : s_FontTag;

    float tag0W = 0.f, tag1W = 0.f;
    float tagPadX = kTagPadX * scale;
    float tagH    = kTagH * scale;
    float tagGap  = kTagGap * scale;

    if (pn.tagCount >= 1) {
        SIZE s = D3DHelper::MeasureText(font, pn.tags[0].text.c_str());
        tag0W = static_cast<float>(s.cx) + tagPadX * 2.f;
    }
    if (pn.tagCount >= 2) {
        SIZE s = D3DHelper::MeasureText(font, pn.tags[1].text.c_str());
        tag1W = static_cast<float>(s.cx) + tagPadX * 2.f;
    }

    const float tag0Gap = tag0W > 0.f ? tagGap : 0.f;
    float totalW = tag0W + tag0Gap + tag1W;
    float x = centerX - totalW * 0.5f;

    if (pn.tagCount >= 1) {
        DrawTag(dev, x, y, scale, pn.tags[0]);
        x += tag0W + tagGap;
    }
    if (pn.tagCount >= 2) {
        DrawTag(dev, x, y, scale, pn.tags[1]);
    }

    return tagH;
}

// ---------------------------------------------------------------------------
// Render 1 player
// ---------------------------------------------------------------------------

static void RenderOne(IDirect3DDevice9* dev,
    float centerX, float topY, float scale,
    const char* name, int id, int ping,
    float hp, float armour,
    const PlayerNametag& pn)
{
    float cy        = topY;
    float gap       = kGap * scale;
    float barTotalW = kBarTotalW * scale;
    float barH      = kBarH * scale;
    float capW      = kCapW * scale;
    float capH      = 22.f * scale; // Tăng chiều cao capsule info cho font rõ ràng hơn

    ID3DXFont* fontName = (scale < 0.75f) ? s_FontNameSmall : s_FontName;
    ID3DXFont* fontInfo = (scale < 0.75f) ? s_FontInfoSmall : s_FontInfo;

    // ── Hàng 1: Tags + Icon ─────────────────────────────────────────────────
    float rowH = DrawTagRow(dev, centerX, cy, scale, pn);
    if (rowH > 0.f) cy += rowH + gap;

    // ── Hàng 2: Tên (cyan + stroke 8 hướng) ─────────────────────────────────
    SIZE nameSz = D3DHelper::MeasureText(fontName, name);
    {
        float nx = centerX - nameSz.cx * 0.5f;
        RECT r = { (LONG)nx, (LONG)cy,
                   (LONG)(nx + nameSz.cx + 2), (LONG)(cy + nameSz.cy) };
        D3DHelper::DrawTextStroke(fontName, name, r,
            DT_LEFT | DT_NOCLIP, kColorName, kColorNameStroke);
        cy += static_cast<float>(nameSz.cy) + gap;
    }

    // ── Hàng 3: Progress bars (HP | Armour) bo tròn góc ─────────────────────
    {
        float singleW = (barTotalW - gap) * 0.5f;
        float bx = centerX - barTotalW * 0.5f;
        float barRadius = 3.f * scale;
        D3DHelper::DrawProgressBar(dev, bx,              cy, singleW, barH, barRadius,
            hp,     kColorBarBg, kColorHpFill);
        D3DHelper::DrawProgressBar(dev, bx + singleW + gap, cy, singleW, barH, barRadius,
            armour, kColorBarBg, kColorArmourFill);
        cy += barH + gap;
    }

    // ── Hàng 4: Capsule ID | Ping bo tròn góc ────────────────────────────────
    {
        char info[64];
        sprintf_s(info, "ID: %d   |   %dms", id, ping);
        float cx2 = centerX - capW * 0.5f;
        float capRadius = 6.f * scale;
        D3DHelper::DrawRoundedFilledRect(dev, cx2, cy, capW, capH, capRadius, kColorBarBg);
        D3DHelper::DrawRoundedBorderRect(dev, cx2, cy, capW, capH, capRadius, 1.f, kColorCapsuleBorder);
        RECT r = { (LONG)cx2, (LONG)cy,
                   (LONG)(cx2 + capW), (LONG)(cy + capH) };
        fontInfo->DrawTextA(NULL, info, -1, &r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, kColorInfoText);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Nametag::Init(IDirect3DDevice9* dev) {
    if (s_Ready && s_FontName && s_FontInfo && s_FontTag && s_Sprite) return;

    Release();

    HRESULT h1 = D3DXCreateFontA(dev, 18, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontName);
    HRESULT h2 = D3DXCreateFontA(dev, 14, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfo);
    HRESULT h3 = D3DXCreateFontA(dev, 10, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontTag);
    
    HRESULT h1s = D3DXCreateFontA(dev, 13, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontNameSmall);
    HRESULT h2s = D3DXCreateFontA(dev, 11, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfoSmall);
    HRESULT h3s = D3DXCreateFontA(dev,  8, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontTagSmall);

    HRESULT h4 = D3DXCreateSprite(dev, &s_Sprite);

    s_Ready = (SUCCEEDED(h1) && SUCCEEDED(h2) && SUCCEEDED(h3) && SUCCEEDED(h1s) && SUCCEEDED(h2s) && SUCCEEDED(h3s) && SUCCEEDED(h4));
}

void Nametag::Release() {
    if (s_FontName)      { s_FontName->Release();      s_FontName      = NULL; }
    if (s_FontInfo)      { s_FontInfo->Release();      s_FontInfo      = NULL; }
    if (s_FontTag)       { s_FontTag->Release();       s_FontTag       = NULL; }
    if (s_FontNameSmall) { s_FontNameSmall->Release(); s_FontNameSmall = NULL; }
    if (s_FontInfoSmall) { s_FontInfoSmall->Release(); s_FontInfoSmall = NULL; }
    if (s_FontTagSmall)  { s_FontTagSmall->Release();  s_FontTagSmall  = NULL; }
    if (s_Sprite)        { s_Sprite->Release();        s_Sprite        = NULL; }
    TextureCache::ReleaseAll();
    s_Ready = false;
}

void Nametag::RenderAll(IDirect3DDevice9* dev) {
    CNetGame* pNet = GetRefNetGame();
    if (pNet) {
        pNet->m_bNametagStatus = false;
        if (pNet->m_pSettings) {
            pNet->m_pSettings->m_bNameTags = false;
            pNet->m_pSettings->m_fNameTagsDrawDist = 0.0f;
        }
    }

    if (!s_Ready || !pNet) return;
    CPlayerPool* pPool = pNet->GetPlayerPool(); if (!pPool) return;

    // KIỂM TRA PLAYER LOCAL ĐÃ SPAWN CHƯA — KHÔNG RENDER NẾU CHƯA SPAWN
    CLocalPlayer* pLocalPlayer = pPool->GetLocalPlayer();
    if (!pLocalPlayer || !pLocalPlayer->m_bIsActive) return;

    TextureCache::FlushPending(dev);

    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);

    SAMPVersionInfo vInfo = SAMPVersionInfo::Get();
    const sampapi::ID localId = pPool->m_nLocalPlayerId;
    const float maxDrawDist = RoleConfig::GetDrawDistance(); // Mặc định 40.0m từ JSON

    // Tọa độ người chơi local để tính khoảng cách
    sampapi::CVector localPos{0.f, 0.f, 0.f};
    auto ppLocalPed = reinterpret_cast<void**>(0xB7CD98);
    if (ppLocalPed && !IsBadReadPtr(ppLocalPed, sizeof(void*)) && *ppLocalPed && !IsBadReadPtr(*ppLocalPed, 0x600)) {
        uintptr_t xform = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*ppLocalPed) + 0x14);
        if (xform && !IsBadReadPtr(reinterpret_cast<void*>(xform), 0x40)) {
            localPos.x = *reinterpret_cast<float*>(xform + 0x30);
            localPos.y = *reinterpret_cast<float*>(xform + 0x34);
            localPos.z = *reinterpret_cast<float*>(xform + 0x38);
        }
    }

    for (int i = 0; i < kMaxPlayers; i++) {
        void* pGamePed = nullptr;
        const char* name = nullptr;
        float hp = 100.f;
        float armour = 0.f;
        int ping = 0;

        if (i == localId) {
            if (ppLocalPed && !IsBadReadPtr(ppLocalPed, sizeof(void*)) && *ppLocalPed && !IsBadReadPtr(*ppLocalPed, 0x600)) {
                pGamePed = *ppLocalPed;
            }
            if (vInfo.fnGetLocalPlayerName)
                name = reinterpret_cast<const char*(__thiscall*)(void*)>(vInfo.fnGetLocalPlayerName)(pPool);
            if (vInfo.fnGetLocalPlayerPing)
                ping = reinterpret_cast<int(__thiscall*)(void*)>(vInfo.fnGetLocalPlayerPing)(pPool);
        } else {
            if (!pPool->IsConnected(i)) continue;
            CRemotePlayer* pPlayer = pPool->GetPlayer(i);
            if (!pPlayer || IsBadReadPtr(pPlayer, sizeof(void*))) continue;

            // KIỂM TRA REMOTE PLAYER ĐÃ SPAWN CHƯA (m_nState != 0 (PLAYER_STATE_NONE))
            if (pPlayer->m_nState == 0) continue;

            pPlayer->m_bDrawLabels = FALSE;

            if (pPlayer->m_pPed && !IsBadReadPtr(pPlayer->m_pPed, sizeof(void*))) {
                pGamePed = pPlayer->m_pPed->m_pGamePed;
            }

            if (vInfo.fnGetName)
                name = reinterpret_cast<const char*(__thiscall*)(void*, int)>(vInfo.fnGetName)(pPool, i);
            if (vInfo.fnGetPing)
                ping = reinterpret_cast<int(__thiscall*)(void*, int)>(vInfo.fnGetPing)(pPool, i);
        }

        if (!pGamePed || IsBadReadPtr(pGamePed, 0x600)) continue;

        hp = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pGamePed) + 0x540);
        armour = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pGamePed) + 0x548);
        if (hp <= 0.0f) continue; // KHÔNG RENDER NẾU ĐÃ CHẾT / CHƯA HỒI SINH

        sampapi::CVector headPos{0.f, 0.f, 0.f};
        bool boneSuccess = false;

        reinterpret_cast<void(__thiscall*)(void*, sampapi::CVector*, int, bool)>(0x5E4280)(
            pGamePed, &headPos, kBoneHead, true);
        if (headPos.z > 0.1f) {
            boneSuccess = true;
        }

        if (!boneSuccess) {
            uintptr_t transform = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(pGamePed) + 0x14);
            if (transform && !IsBadReadPtr(reinterpret_cast<void*>(transform), 0x40)) {
                headPos.x = *reinterpret_cast<float*>(transform + 0x30);
                headPos.y = *reinterpret_cast<float*>(transform + 0x34);
                headPos.z = *reinterpret_cast<float*>(transform + 0x38) + 0.8f;
            } else {
                headPos.x = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pGamePed) + 0x4);
                headPos.y = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pGamePed) + 0x8);
                headPos.z = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(pGamePed) + 0xC) + 0.8f;
            }
        }
        headPos.z += kHeadOffZ;

        // TÍNH KHOẢNG CÁCH D3D VỚI PLAYER LOCAL & FILTER DRAW_DISTANCE
        float dx = headPos.x - localPos.x;
        float dy = headPos.y - localPos.y;
        float dz = headPos.z - localPos.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);

        if (dist > maxDrawDist) continue; // KHÔNG RENDER NẾU QUÁ KHOẢNG CÁCH CẤU HÌNH JSON

        // CÀNG XA NAMETAG CÀNG NHỎ (Scale từ 1.0f xuống 0.45f theo khoảng cách)
        float scale = 1.0f - (dist / maxDrawDist) * 0.55f;
        if (scale < 0.45f) scale = 0.45f;
        if (scale > 1.0f) scale = 1.0f;

        float sx = 0.f, sy = 0.f;
        if (!W2S::ToScreen(headPos, sx, sy)) continue;

        const char* displayName = (name && strlen(name) > 0) ? name : "Unknown";
        const PlayerNametag& pn = g_Players[i];

        RenderOne(dev, sx, sy, scale, displayName, i, ping, hp, armour, pn);
    }
}
