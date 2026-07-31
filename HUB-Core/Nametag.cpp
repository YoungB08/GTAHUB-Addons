/**
 * @file Nametag.cpp
 * @brief Implementaion hệ thống render nametag tùy biến.
 */
#include "pch.h"
#include "Nametag.h"
#include "D3DHelper.h"
#include "W2S.h"
#include "TextureCache.h"
#include "PlayerData.h"

#include <0.3.DL-1/CNetGame.h>
#include <0.3.DL-1/CRemotePlayer.h>
#include <0.3.DL-1/CPlayerPool.h>
#include <0.3.DL-1/CPed.h>
#include <0.3.DL-1/CPlayerTags.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

using namespace sampapi::v03dl;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Màu cyan tên player (#33CCFF)
constexpr D3DCOLOR kColorName        = D3DCOLOR_ARGB(255,  51, 204, 255);
/// Màu stroke đen
constexpr D3DCOLOR kColorStroke      = D3DCOLOR_ARGB(255,   0,   0,   0);
/// Màu nền HP bar (đỏ đậm)
constexpr D3DCOLOR kColorHpFill      = D3DCOLOR_ARGB(255, 210,  30,  30);
/// Màu nền Armour bar (bạc)
constexpr D3DCOLOR kColorArmourFill  = D3DCOLOR_ARGB(255, 180, 180, 180);
/// Màu nền tối cho bar/capsule
constexpr D3DCOLOR kColorBarBg       = D3DCOLOR_ARGB(200,   0,   0,   0);
/// Màu badge ADMIN (đỏ)
constexpr D3DCOLOR kColorAdmin       = D3DCOLOR_ARGB(230, 179,   0,   0);
/// Màu badge VIP (vàng)
constexpr D3DCOLOR kColorVIP         = D3DCOLOR_ARGB(230, 204, 153,   0);
/// Màu viền capsule info
constexpr D3DCOLOR kColorCapsuleBorder = D3DCOLOR_ARGB(180, 255, 255, 255);
/// Màu text capsule (cyan)
constexpr D3DCOLOR kColorInfoText    = D3DCOLOR_ARGB(255,  51, 204, 255);

/// Khoảng cách giữa các hàng (pixel)
constexpr float kGap = 5.f;

/// Kích thước badge
constexpr float kBadgeW  = 45.f;
constexpr float kBadgeH  = 16.f;
constexpr float kIconSize = 20.f;

/// Tổng chiều rộng 2 progress bar
constexpr float kBarTotalW = 180.f;
constexpr float kBarH      =   9.f;

/// Kích thước capsule info
constexpr float kCapW = 130.f;
constexpr float kCapH =  18.f;

/// Bone ID đầu player trong GTA SA
constexpr UINT kBoneHead = 8;

/// Offset lên trên vị trí đầu để nametag không đè lên model
constexpr float kHeadOffset = 0.28f;

// ---------------------------------------------------------------------------
// D3D resources (chỉ tạo 1 lần, dùng lại qua các frame)
// ---------------------------------------------------------------------------

static ID3DXFont*   s_FontName  = NULL; ///< Font tên player (18px Bold)
static ID3DXFont*   s_FontInfo  = NULL; ///< Font ID/ping  (11px Normal)
static ID3DXFont*   s_FontBadge = NULL; ///< Font badge    (10px Bold)
static ID3DXSprite* s_Sprite    = NULL; ///< Sprite renderer dùng cho icon texture
static bool         s_Ready     = false;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief Vẽ một hàng badge (ADMIN hoặc VIP).
 * @param label    Text hiển thị ("ADMIN" / "VIP")
 * @param color    Màu nền badge
 */
static void DrawBadge(IDirect3DDevice9* dev,
    float x, float y, const char* label, D3DCOLOR color)
{
    D3DHelper::DrawFilledRect(dev, x, y, kBadgeW, kBadgeH, color);
    D3DHelper::DrawBorderRect(dev, x, y, kBadgeW, kBadgeH, 1.f, kColorStroke);
    RECT r = { (LONG)x, (LONG)y, (LONG)(x + kBadgeW), (LONG)(y + kBadgeH) };
    s_FontBadge->DrawTextA(NULL, label, -1, &r,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE, D3DCOLOR_ARGB(255,255,255,255));
}

/**
 * @brief Vẽ icon texture tại vị trí (x,y). Nếu chưa load xong → vẽ placeholder đen.
 * @param url URL của icon (rỗng → bỏ qua)
 */
static void DrawIcon(IDirect3DDevice9* dev,
    float x, float y, const std::string& url)
{
    LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, url);
    if (tex) {
        // Scale texture về kích thước kIconSize x kIconSize
        D3DXMATRIX mat;
        constexpr float kSrcSize = 32.f; // giả sử texture gốc 32x32
        float scale = kIconSize / kSrcSize;
        D3DXMatrixTransformation2D(&mat,
            NULL, 0.f, &D3DXVECTOR2(scale, scale),
            NULL, 0.f, &D3DXVECTOR2(x, y));
        s_Sprite->Begin(D3DXSPRITE_ALPHABLEND);
        s_Sprite->SetTransform(&mat);
        s_Sprite->Draw(tex, NULL, NULL, NULL, D3DCOLOR_ARGB(255,255,255,255));
        s_Sprite->End();
    } else {
        // Placeholder khi chưa tải xong
        D3DHelper::DrawFilledRect(dev, x, y, kIconSize, kIconSize,
            D3DCOLOR_ARGB(80, 0, 0, 0));
    }
    D3DHelper::DrawBorderRect(dev, x, y, kIconSize, kIconSize, 1.f,
        D3DCOLOR_ARGB(120, 0, 0, 0));
}

// ---------------------------------------------------------------------------
// Public render: một player
// ---------------------------------------------------------------------------

/**
 * @brief Render nametag của một player.
 *
 * @param dev         D3D9 device
 * @param centerX     Tọa độ X trung tâm (từ WorldToScreen)
 * @param topY        Tọa độ Y đỉnh trên (từ WorldToScreen)
 * @param name        Tên player
 * @param id          PlayerID
 * @param ping        Ping (ms)
 * @param hp          Máu [0-100]
 * @param armour      Giáp [0-100]
 * @param role        Dữ liệu role/badge từ server
 */
static void RenderOne(IDirect3DDevice9* dev,
    float centerX, float topY,
    const char* name, int id, int ping,
    float hp, float armour,
    const PlayerRoleData& role)
{
    float cy = topY;

    // ── Hàng 1: Badges + Icon ───────────────────────────────────────────────
    const bool showBadgeRow = role.isAdmin || role.isVIP || !role.iconUrl.empty();
    if (showBadgeRow) {
        const float rowW = kBadgeW + kGap + kIconSize + kGap + kBadgeW;
        float bx = centerX - rowW * 0.5f;

        if (role.isAdmin)
            DrawBadge(dev, bx, cy, "ADMIN", kColorAdmin);

        DrawIcon(dev, bx + kBadgeW + kGap, cy, role.iconUrl);

        if (role.isVIP)
            DrawBadge(dev, bx + kBadgeW + kGap + kIconSize + kGap, cy, "VIP", kColorVIP);

        cy += kBadgeH + kGap;
    }

    // ── Hàng 2: Tên (cyan + stroke) ─────────────────────────────────────────
    SIZE nameSz = D3DHelper::MeasureText(s_FontName, name);
    {
        float nx = centerX - nameSz.cx * 0.5f;
        RECT r = { (LONG)nx, (LONG)cy, (LONG)(nx + nameSz.cx + 2), (LONG)(cy + nameSz.cy) };
        D3DHelper::DrawTextStroke(s_FontName, name, r, DT_LEFT | DT_NOCLIP,
            kColorName, kColorStroke);
        cy += static_cast<float>(nameSz.cy) + kGap;
    }

    // ── Hàng 3: Progress bars (HP | Armour) ─────────────────────────────────
    {
        const float singleW = (kBarTotalW - kGap) * 0.5f;
        float bx = centerX - kBarTotalW * 0.5f;

        D3DHelper::DrawProgressBar(dev, bx,             cy, singleW, kBarH,
            hp, kColorBarBg, kColorHpFill);
        D3DHelper::DrawProgressBar(dev, bx + singleW + kGap, cy, singleW, kBarH,
            armour, kColorBarBg, kColorArmourFill);

        cy += kBarH + kGap;
    }

    // ── Hàng 4: Capsule info (ID | Ping) ────────────────────────────────────
    {
        char info[64];
        sprintf_s(info, "ID: %d   |   %dms", id, ping);

        float cx2 = centerX - kCapW * 0.5f;
        D3DHelper::DrawFilledRect(dev, cx2, cy, kCapW, kCapH, kColorBarBg);
        D3DHelper::DrawBorderRect(dev, cx2, cy, kCapW, kCapH, 1.f, kColorCapsuleBorder);
        RECT r = { (LONG)cx2, (LONG)cy, (LONG)(cx2 + kCapW), (LONG)(cy + kCapH) };
        s_FontInfo->DrawTextA(NULL, info, -1, &r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, kColorInfoText);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Nametag::Init(IDirect3DDevice9* dev) {
    if (s_Ready) return;
    D3DXCreateFontA(dev, 18, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontName);
    D3DXCreateFontA(dev, 11, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfo);
    D3DXCreateFontA(dev, 10, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontBadge);
    D3DXCreateSprite(dev, &s_Sprite);
    s_Ready = true;
}

void Nametag::Release() {
    if (s_FontName)  { s_FontName->Release();  s_FontName  = NULL; }
    if (s_FontInfo)  { s_FontInfo->Release();  s_FontInfo  = NULL; }
    if (s_FontBadge) { s_FontBadge->Release(); s_FontBadge = NULL; }
    if (s_Sprite)    { s_Sprite->Release();    s_Sprite    = NULL; }
    TextureCache::ReleaseAll();
    s_Ready = false;
}

void Nametag::RenderAll(IDirect3DDevice9* dev) {
    if (!s_Ready) return;

    CNetGame*    pNet  = RefNetGame();   if (!pNet)  return;
    CPlayerPool* pPool = pNet->GetPlayerPool(); if (!pPool) return;

    // Flush pending textures (phải chạy trên render thread)
    TextureCache::FlushPending(dev);

    // Lấy viewport để kiểm tra bounds
    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);

    const ID localId = pPool->m_nLocalPlayerId;

    for (int i = 0; i < kMaxPlayers; i++) {
        if (i == localId)             continue;
        if (!pPool->IsConnected(i))   continue;

        CRemotePlayer* pPlayer = pPool->GetPlayer(i);
        if (!pPlayer || !pPlayer->m_pPed) continue;

        // Vị trí đầu player trong thế giới
        sampapi::CVector headPos;
        pPlayer->m_pPed->GetBonePosition(kBoneHead, &headPos);
        headPos.z += kHeadOffset;

        float sx, sy;
        if (!W2S::ToScreen(headPos, sx, sy)) continue;

        // Kiểm tra trong viewport
        if (sx < 0.f || sx > (float)vp.Width)  continue;
        if (sy < 0.f || sy > (float)vp.Height) continue;

        const char* name   = pPool->GetName(i);
        int         ping   = pPool->GetPing(i);
        float       hp     = max(0.f, min(100.f, pPlayer->m_fReportedHealth));
        float       armour = max(0.f, min(100.f, pPlayer->m_fReportedArmour));

        RenderOne(dev, sx, sy,
            name ? name : "?", i, ping, hp, armour, g_Players[i]);
    }
}
