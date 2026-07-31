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

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>

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

static ID3DXFont*   s_FontName  = NULL;
static ID3DXFont*   s_FontInfo  = NULL;
static ID3DXFont*   s_FontTag   = NULL;
static ID3DXSprite* s_Sprite    = NULL;
static bool         s_Ready     = false;

// ---------------------------------------------------------------------------
// Internal draw helpers
// ---------------------------------------------------------------------------

/**
 * @brief Vẽ 1 tag badge: nền màu từ server + text (+ stroke nếu bật).
 *
 * Chiều rộng badge tự động co giãn theo độ dài text.
 * @param x,y     Góc trên-trái
 * @param tag     Dữ liệu tag từ server
 * @return        Chiều rộng thực của badge đã vẽ
 */
static float DrawTag(IDirect3DDevice9* dev, float x, float y, const RoleTag& tag) {
    // Đo text để tính chiều rộng badge
    SIZE sz = D3DHelper::MeasureText(s_FontTag, tag.text.c_str());
    float tagW = static_cast<float>(sz.cx) + kTagPadX * 2.f;

    // Nền màu từ server
    D3DHelper::DrawFilledRect(dev, x, y, tagW, kTagH, tag.color);
    // Viền đen
    D3DHelper::DrawBorderRect(dev, x, y, tagW, kTagH, 1.f, kColorTagStroke);

    // Text (+ stroke nếu server bật)
    RECT r = { (LONG)x, (LONG)y, (LONG)(x + tagW), (LONG)(y + kTagH) };
    if (tag.stroke) {
        D3DHelper::DrawTextStroke(s_FontTag, tag.text.c_str(), r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            kColorTagText, kColorTagStroke);
    } else {
        s_FontTag->DrawTextA(NULL, tag.text.c_str(), -1, &r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, kColorTagText);
    }

    return tagW;
}

/**
 * @brief Vẽ icon URL texture (hoặc placeholder nếu chưa tải xong).
 * Tự scale về kIconSize x kIconSize.
 */
static void DrawIcon(IDirect3DDevice9* dev, float x, float y, const std::string& url) {
    LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, url);
    if (tex) {
        // Lấy kích thước gốc của texture để scale đúng
        D3DSURFACE_DESC desc;
        tex->GetLevelDesc(0, &desc);
        float scaleX = kIconSize / static_cast<float>(desc.Width);
        float scaleY = kIconSize / static_cast<float>(desc.Height);

        D3DXMATRIX mat;
        D3DXVECTOR2 scale(scaleX, scaleY);
        D3DXVECTOR2 pos(x, y);
        D3DXMatrixTransformation2D(&mat,
            NULL, 0.f, &scale,
            NULL, 0.f, &pos);

        s_Sprite->Begin(D3DXSPRITE_ALPHABLEND);
        s_Sprite->SetTransform(&mat);
        s_Sprite->Draw(tex, NULL, NULL, NULL, D3DCOLOR_ARGB(255, 255, 255, 255));
        s_Sprite->End();
    } else {
        // Placeholder: hình vuông tối
        D3DHelper::DrawFilledRect(dev, x, y, kIconSize, kIconSize,
            D3DCOLOR_ARGB(80, 0, 0, 0));
    }
    D3DHelper::DrawBorderRect(dev, x, y, kIconSize, kIconSize, 1.f,
        D3DCOLOR_ARGB(120, 0, 0, 0));
}

/**
 * @brief Vẽ hàng tag + icon theo layout:
 *
 *   [tag0] [icon] [tag1]      — nếu có cả 2 tag
 *   [icon] [tag0]             — nếu chỉ có 1 tag
 *   [icon]                    — nếu không có tag
 *
 * Căn giữa centerX.
 * @return Chiều cao của hàng (để tăng currentY).
 */
static float DrawTagRow(IDirect3DDevice9* dev, float centerX, float y,
    const PlayerNametag& pn)
{
    // Không có gì cần vẽ
    if (pn.tagCount == 0 && pn.iconUrl.empty()) return 0.f;

    // Tính chiều rộng từng thành phần
    float tag0W = 0.f, tag1W = 0.f;
    if (pn.tagCount >= 1) {
        SIZE s = D3DHelper::MeasureText(s_FontTag, pn.tags[0].text.c_str());
        tag0W = static_cast<float>(s.cx) + kTagPadX * 2.f;
    }
    if (pn.tagCount >= 2) {
        SIZE s = D3DHelper::MeasureText(s_FontTag, pn.tags[1].text.c_str());
        tag1W = static_cast<float>(s.cx) + kTagPadX * 2.f;
    }

    const float iconW = pn.iconUrl.empty() ? 0.f : kIconSize;
    const float iconGap = iconW > 0.f ? kTagGap : 0.f;
    const float tag0Gap = tag0W > 0.f ? kTagGap : 0.f;

    // Tổng chiều rộng để căn giữa
    float totalW = tag0W + (tag0W > 0.f ? tag0Gap : 0.f)
                 + iconW + iconGap
                 + tag1W;

    float x = centerX - totalW * 0.5f;

    // Vẽ tag[0]
    if (pn.tagCount >= 1) {
        DrawTag(dev, x, y, pn.tags[0]);
        x += tag0W + kTagGap;
    }
    // Vẽ icon
    if (!pn.iconUrl.empty()) {
        DrawIcon(dev, x, y + (kTagH - kIconSize) * 0.5f, pn.iconUrl);
        x += iconW + kTagGap;
    }
    // Vẽ tag[1]
    if (pn.tagCount >= 2) {
        DrawTag(dev, x, y, pn.tags[1]);
    }

    return max(kTagH, kIconSize);
}

// ---------------------------------------------------------------------------
// Render 1 player
// ---------------------------------------------------------------------------

static void RenderOne(IDirect3DDevice9* dev,
    float centerX, float topY,
    const char* name, int id, int ping,
    float hp, float armour,
    const PlayerNametag& pn)
{
    float cy = topY;

    // ── Hàng 1: Tags + Icon ─────────────────────────────────────────────────
    float rowH = DrawTagRow(dev, centerX, cy, pn);
    if (rowH > 0.f) cy += rowH + kGap;

    // ── Hàng 2: Tên (cyan + stroke 8 hướng) ─────────────────────────────────
    SIZE nameSz = D3DHelper::MeasureText(s_FontName, name);
    {
        float nx = centerX - nameSz.cx * 0.5f;
        RECT r = { (LONG)nx, (LONG)cy,
                   (LONG)(nx + nameSz.cx + 2), (LONG)(cy + nameSz.cy) };
        D3DHelper::DrawTextStroke(s_FontName, name, r,
            DT_LEFT | DT_NOCLIP, kColorName, kColorNameStroke);
        cy += static_cast<float>(nameSz.cy) + kGap;
    }

    // ── Hàng 3: Progress bars (HP | Armour) ─────────────────────────────────
    {
        float singleW = (kBarTotalW - kGap) * 0.5f;
        float bx = centerX - kBarTotalW * 0.5f;
        D3DHelper::DrawProgressBar(dev, bx,              cy, singleW, kBarH,
            hp,     kColorBarBg, kColorHpFill);
        D3DHelper::DrawProgressBar(dev, bx + singleW + kGap, cy, singleW, kBarH,
            armour, kColorBarBg, kColorArmourFill);
        cy += kBarH + kGap;
    }

    // ── Hàng 4: Capsule ID | Ping ────────────────────────────────────────────
    {
        char info[64];
        sprintf_s(info, "ID: %d   |   %dms", id, ping);
        float cx2 = centerX - kCapW * 0.5f;
        D3DHelper::DrawFilledRect(dev, cx2, cy, kCapW, kCapH, kColorBarBg);
        D3DHelper::DrawBorderRect(dev, cx2, cy, kCapW, kCapH, 1.f, kColorCapsuleBorder);
        RECT r = { (LONG)cx2, (LONG)cy,
                   (LONG)(cx2 + kCapW), (LONG)(cy + kCapH) };
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
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontName);
    D3DXCreateFontA(dev, 11, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfo);
    D3DXCreateFontA(dev, 10, 0, FW_BOLD,   1, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontTag);
    D3DXCreateSprite(dev, &s_Sprite);
    s_Ready = true;
}

void Nametag::Release() {
    if (s_FontName)  { s_FontName->Release();  s_FontName  = NULL; }
    if (s_FontInfo)  { s_FontInfo->Release();  s_FontInfo  = NULL; }
    if (s_FontTag)   { s_FontTag->Release();   s_FontTag   = NULL; }
    if (s_Sprite)    { s_Sprite->Release();    s_Sprite    = NULL; }
    TextureCache::ReleaseAll();
    s_Ready = false;
}

void Nametag::RenderAll(IDirect3DDevice9* dev) {
    if (!s_Ready) return;

    CNetGame*    pNet  = RefNetGame();           if (!pNet)  return;
    CPlayerPool* pPool = pNet->GetPlayerPool();  if (!pPool) return;

    TextureCache::FlushPending(dev); // tạo texture từ file đã download (render thread)

    D3DVIEWPORT9 vp;
    dev->GetViewport(&vp);

    const sampapi::ID localId = pPool->m_nLocalPlayerId;

    for (int i = 0; i < kMaxPlayers; i++) {
        if (i == localId)           continue;
        if (!pPool->IsConnected(i)) continue;

        CRemotePlayer* pPlayer = pPool->GetPlayer(i);
        if (!pPlayer || !pPlayer->m_pPed) continue;

        // Lấy vị trí đầu player → project lên màn hình
        sampapi::CVector headPos;
        pPlayer->m_pPed->GetBonePosition(kBoneHead, &headPos);
        headPos.z += kHeadOffZ;

        float sx, sy;
        if (!W2S::ToScreen(headPos, sx, sy)) continue;
        if (sx < 0.f || sx > (float)vp.Width)  continue;
        if (sy < 0.f || sy > (float)vp.Height) continue;

        const char* name   = pPool->GetName(i);
        float hp     = max(0.f, min(100.f, pPlayer->m_fReportedHealth));
        float armour = max(0.f, min(100.f, pPlayer->m_fReportedArmour));
        int   ping   = pPool->GetPing(i);

        RenderOne(dev, sx, sy, name ? name : "?",
            i, ping, hp, armour, g_Players[i]);
    }
}
