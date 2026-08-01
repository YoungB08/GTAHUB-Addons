/**
 * @file Nametag.cpp
 * @brief Render nametag động cho tất cả player mỗi frame bằng Direct3D 9 Engine.
 * Hỗ trợ tối đa 10 Multi-Slot (Tự phân dòng khi quá dài), Rainbow Effects, Custom Nametag Color và Undercover Mode.
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
#include <vector>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

using namespace sampapi::v03dl;

// ---------------------------------------------------------------------------
// Layout constants
// ---------------------------------------------------------------------------

constexpr D3DCOLOR kColorNameDefault    = D3DCOLOR_ARGB(255, 255, 255, 255);
constexpr D3DCOLOR kColorNameStroke     = D3DCOLOR_ARGB(255,   0,   0,   0);
constexpr D3DCOLOR kColorTagTextDefault = D3DCOLOR_ARGB(255, 255, 255, 255);
constexpr D3DCOLOR kColorTagStroke      = D3DCOLOR_ARGB(255,   0,   0,   0);
constexpr D3DCOLOR kColorHpFill         = D3DCOLOR_ARGB(255, 210,  30,  30);
constexpr D3DCOLOR kColorArmourFill     = D3DCOLOR_ARGB(255, 180, 180, 180);
constexpr D3DCOLOR kColorBarBg          = D3DCOLOR_ARGB(200,   0,   0,   0);

constexpr float kGap       = 6.f;  ///< Khoảng cách dọc giữa các hàng (px)
constexpr float kTagPadX   = 8.f;  ///< Padding ngang trong tag badge
constexpr float kTagH      = 18.f; ///< Chiều cao cố định của tag badge
constexpr float kTagGap    = 5.f;  ///< Khoảng cách ngang giữa các tag
constexpr float kBarW      = 160.f;///< Chiều rộng progress bar
constexpr float kBarH      = 9.f;  ///< Chiều cao progress bar

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

static float DrawSingleTag(IDirect3DDevice9* dev, float x, float y, float scale, const RoleSlotClientData& slot) {
    ID3DXFont* font = (scale < 0.75f) ? s_FontTagSmall : s_FontTag;
    SIZE sz = D3DHelper::MeasureText(font, slot.text.c_str());

    float tagPadX = kTagPadX * scale;
    float tagH    = kTagH * scale;
    float tagW    = static_cast<float>(sz.cx) + tagPadX * 2.f;
    float radius  = 4.f * scale;

    D3DCOLOR badgeColor = slot.isRainbow ? GetRainbowD3DColor(slot.currentHue) : (slot.color != 0 ? slot.color : D3DCOLOR_ARGB(255, 50, 50, 50));
    D3DCOLOR bgColor    = slot.bgColor != 0 ? slot.bgColor : badgeColor;

    D3DHelper::DrawRoundedFilledRect(dev, x, y, tagW, tagH, radius, bgColor);
    D3DHelper::DrawRoundedBorderRect(dev, x, y, tagW, tagH, radius, 1.f, kColorTagStroke);

    RECT r = { (LONG)x, (LONG)y, (LONG)(x + tagW), (LONG)(y + tagH) };
    if (slot.stroke) {
        D3DHelper::DrawTextStroke(font, slot.text.c_str(), r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE,
            kColorTagTextDefault, kColorTagStroke);
    } else {
        font->DrawTextA(NULL, slot.text.c_str(), -1, &r,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, kColorTagTextDefault);
    }

    return tagW;
}

static void DrawIconTexture(IDirect3DDevice9* dev, float x, float y, float targetW, float targetH, const std::string& pathOrUrl) {
    LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, pathOrUrl);
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

struct RenderSlotItem {
    float width = 0.f;
    float height = 0.f;
    int slotIndex = 0;
};

/**
 * @brief Vẽ tối đa 10 Role Badges, tự động xuống dòng (Tầng / Multi-row) khi độ rộng vượt quá maxRowW.
 */
static float DrawMultiSlotRow(IDirect3DDevice9* dev, float centerX, float y, float scale, const PlayerNametag& pn) {
    std::vector<RenderSlotItem> items;

    ID3DXFont* font = (scale < 0.75f) ? s_FontTagSmall : s_FontTag;
    float tagPadX = kTagPadX * scale;
    float tagH    = 20.f * scale;
    float badgeH  = 26.f * scale;
    float tagGap  = kTagGap * scale;
    float rowGap  = kGap * scale;
    float maxRowW = 240.f * scale; // Chiều rộng tối đa mỗi tầng trước khi phân dòng

    for (int s = 0; s < kMaxRoleSlots; ++s) {
        const auto& slot = pn.slots[s];
        if (!slot.active) continue;

        RenderSlotItem item;
        item.slotIndex = s;
        item.height = tagH;

        if (!slot.imagePath.empty()) {
            LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(dev, slot.imagePath);
            if (tex) {
                D3DSURFACE_DESC desc;
                tex->GetLevelDesc(0, &desc);
                if (desc.Width > 0 && desc.Height > 0) {
                    float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
                    item.width = badgeH * aspect;
                    item.height = badgeH;
                }
            }
        }

        if (item.width <= 0.f && !slot.text.empty()) {
            SIZE sz = D3DHelper::MeasureText(font, slot.text.c_str());
            item.width = static_cast<float>(sz.cx) + tagPadX * 2.f;
            item.height = tagH;
        }

        if (item.width > 0.f) {
            items.push_back(item);
        }
    }

    if (items.empty()) return 0.f;

    // Tự động phân dòng (Tầng / Rows) khi tổng width vượt quá maxRowW
    std::vector<std::vector<RenderSlotItem>> rows;
    std::vector<RenderSlotItem> currentRow;
    float currentW = 0.f;

    for (const auto& item : items) {
        float testW = currentW + (currentRow.empty() ? 0.f : tagGap) + item.width;
        if (!currentRow.empty() && testW > maxRowW) {
            rows.push_back(currentRow);
            currentRow.clear();
            currentW = 0.f;
        }
        if (!currentRow.empty()) currentW += tagGap;
        currentW += item.width;
        currentRow.push_back(item);
    }
    if (!currentRow.empty()) {
        rows.push_back(currentRow);
    }

    // Render từng tầng từ trên xuống
    float curY = y;
    float totalHeight = 0.f;

    for (size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        float rowW = 0.f;
        float maxH = tagH;

        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) rowW += tagGap;
            rowW += row[i].width;
            if (row[i].height > maxH) maxH = row[i].height;
        }

        float curX = centerX - rowW * 0.5f;

        for (const auto& item : row) {
            const auto& slot = pn.slots[item.slotIndex];
            if (item.height == badgeH && !slot.imagePath.empty()) {
                DrawIconTexture(dev, curX, curY, item.width, badgeH, slot.imagePath);
            } else if (!slot.text.empty()) {
                float tagY = curY + (maxH > tagH ? (maxH - tagH) * 0.5f : 0.f);
                DrawSingleTag(dev, curX, tagY, scale, slot);
            }
            curX += item.width + tagGap;
        }

        curY += maxH + rowGap;
        totalHeight += maxH + (r + 1 < rows.size() ? rowGap : 0.f);
    }

    return totalHeight;
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
    if (!pn.visible) return;

    float cy   = topY;
    float gap  = kGap * scale;
    float barW = kBarW * scale;
    float barH = kBarH * scale;

    ID3DXFont* fontName = (scale < 0.75f) ? s_FontNameSmall : s_FontName;

    // ── Hàng 1: Multi-Slot Role Badges (Up to 10 Slots - Auto Multi-Row) ──────
    float rowH = DrawMultiSlotRow(dev, centerX, cy, scale, pn);
    if (rowH > 0.f) cy += rowH + gap;

    // ── Hàng 2: Tên & ID ───────────────────────────────────────────────────
    char nameWithId[128];
    sprintf_s(nameWithId, "%s (%d)", name, id);

    D3DCOLOR nameColor = kColorNameDefault;
    if (pn.isNametagRainbow) {
        nameColor = GetRainbowD3DColor(pn.nametagRainbowHue);
    } else if (pn.hasCustomNametagColor) {
        nameColor = pn.nametagColor;
    }

    SIZE nameSz = D3DHelper::MeasureText(fontName, nameWithId);
    {
        float nx = centerX - nameSz.cx * 0.5f;
        RECT r = { (LONG)nx, (LONG)cy, (LONG)(nx + nameSz.cx + 2), (LONG)(cy + nameSz.cy) };
        D3DHelper::DrawTextStroke(fontName, nameWithId, r, DT_LEFT | DT_NOCLIP, nameColor, kColorNameStroke);
        cy += static_cast<float>(nameSz.cy) + gap;
    }

    // ── Hàng 3: Progress Bar Máu ───────────────────────────────────────────
    {
        float bx = centerX - barW * 0.5f;
        float barRadius = 3.f * scale;
        D3DHelper::DrawProgressBar(dev, bx, cy, barW, barH, barRadius, hp, kColorBarBg, kColorHpFill);
        cy += barH + gap;
    }

    // ── Hàng 4: Progress Bar Giáp ──────────────────────────────────────────
    if (armour > 0.f) {
        float bx = centerX - barW * 0.5f;
        float barRadius = 3.f * scale;
        D3DHelper::DrawProgressBar(dev, bx, cy, barW, barH, barRadius, armour, kColorBarBg, kColorArmourFill);
        cy += barH + gap;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Nametag::Init(IDirect3DDevice9* dev) {
    if (s_Ready && s_FontName && s_FontInfo && s_FontTag && s_Sprite) return;

    Release();

    HRESULT h1 = D3DXCreateFontA(dev, 18, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontName);
    HRESULT h2 = D3DXCreateFontA(dev, 14, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfo);
    HRESULT h3 = D3DXCreateFontA(dev, 10, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontTag);
    
    HRESULT h1s = D3DXCreateFontA(dev, 13, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontNameSmall);
    HRESULT h2s = D3DXCreateFontA(dev, 11, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontInfoSmall);
    HRESULT h3s = D3DXCreateFontA(dev,  8, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &s_FontTagSmall);

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

    CLocalPlayer* pLocalPlayer = pPool->GetLocalPlayer();
    if (!pLocalPlayer || !pLocalPlayer->m_bIsActive) return;

    TextureCache::FlushPending(dev);

    const sampapi::ID localId = pPool->m_nLocalPlayerId;
    const float maxDrawDist = RoleConfig::GetDrawDistance();

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

    for (int i = 0; i < kMaxPlayers; ++i) {
        auto& pn = g_Players[i];
        if (pn.isNametagRainbow) {
            pn.nametagRainbowHue = std::fmod(pn.nametagRainbowHue + 3.0f, 360.0f);
        }
        for (int s = 0; s < kMaxRoleSlots; ++s) {
            if (pn.slots[s].active && pn.slots[s].isRainbow) {
                pn.slots[s].currentHue = std::fmod(pn.slots[s].currentHue + 3.0f, 360.0f);
            }
        }
    }

    for (sampapi::ID i = 0; i < kMaxPlayers; ++i) {
        if (i == localId) continue;
        if (!pPool->m_bNotEmpty[i]) continue;

        CRemotePlayer* pRemote = pPool->GetPlayer(i);
        if (!pRemote || !pRemote->m_pPed) continue;

        sampapi::v03dl::CPed* pPed = pRemote->m_pPed;
        if (!pPed->m_pGamePed) continue;

        sampapi::CVector bonePos{0.f, 0.f, 0.f};
        pPed->GetBonePosition(8, &bonePos);
        sampapi::CVector worldPos = (bonePos.z > 0.f) ? bonePos : pRemote->m_onfootTargetPosition;
        worldPos.z += 0.55f;

        float dx = worldPos.x - localPos.x;
        float dy = worldPos.y - localPos.y;
        float dz = worldPos.z - localPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > maxDrawDist) continue;

        float screenX = 0.f, screenY = 0.f;
        if (!W2S::ToScreen(worldPos, screenX, screenY)) continue;

        float scale = 1.f - (dist / maxDrawDist) * 0.45f;
        if (scale < 0.55f) scale = 0.55f;

        const char* name = pPool->GetName(i);
        if (!name || name[0] == '\0') name = "Player";

        int ping = pPool->GetPing(i);
        float hp = pPed->GetHealth();
        float armour = pPed->GetArmour();

        RenderOne(dev, screenX, screenY, scale, name, i, ping, hp, armour, g_Players[i]);
    }
}
