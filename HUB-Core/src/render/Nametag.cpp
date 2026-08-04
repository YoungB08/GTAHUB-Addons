#include "pch.h"
#include "Nametag.h"

#include "D3DHelper.h"
#include "PlayerData.h"
#include "RoleConfig.h"
#include "TextureCache.h"
#include "W2S.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>

#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <array>
#include <atomic>
#include <vector>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

using Microsoft::WRL::ComPtr;
using namespace sampapi::v03dl;

namespace {

constexpr uintptr_t kGtaFrameCounterAddress = 0xB7CB4C;
constexpr float kHeadOffsetZ = 0.45f;
constexpr float kGap = 6.0f;
constexpr float kTagGap = 5.0f;
constexpr float kTagPaddingX = 8.0f;
constexpr float kTagHeight = 18.0f;
constexpr float kBarWidth = 160.0f;
constexpr float kBarHeight = 9.0f;
constexpr float kMaxBadgeRowWidth = 240.0f;

constexpr D3DCOLOR kNameColor = D3DCOLOR_ARGB(255, 255, 255, 255);
constexpr D3DCOLOR kStrokeColor = D3DCOLOR_ARGB(255, 0, 0, 0);
constexpr D3DCOLOR kHealthColor = D3DCOLOR_ARGB(255, 210, 30, 30);
constexpr D3DCOLOR kArmourColor = D3DCOLOR_ARGB(255, 180, 180, 180);
constexpr D3DCOLOR kBarBackground = D3DCOLOR_ARGB(200, 0, 0, 0);

ComPtr<ID3DXFont> g_NameFont;
ComPtr<ID3DXFont> g_NameFontSmall;
ComPtr<ID3DXFont> g_TagFont;
ComPtr<ID3DXFont> g_TagFontSmall;
ComPtr<ID3DXSprite> g_Sprite;
ComPtr<IDirect3DStateBlock9> g_StateBlock;
IDirect3DDevice9* g_Device = nullptr;
std::atomic<uint32_t> g_LastRenderedFrame{UINT32_MAX};
DWORD g_LastAnimationTick = 0;
bool g_DeviceLost = false;

bool IsReadable(const void* address, size_t size) {
    if (!address || reinterpret_cast<uintptr_t>(address) < 0x10000 || size == 0) return false;

    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + size;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return end >= start && end <= regionEnd;
}

bool AcquireFrame() {
    const auto* frameCounter = reinterpret_cast<const uint32_t*>(kGtaFrameCounterAddress);
    if (!IsReadable(frameCounter, sizeof(*frameCounter))) return true;

    const uint32_t frame = *frameCounter;
    return g_LastRenderedFrame.exchange(frame, std::memory_order_acq_rel) != frame;
}

void SuppressNativeNametags(CNetGame* netGame, CPlayerPool* playerPool) {
    if (!netGame) return;
    if (netGame->m_pSettings) {
        netGame->m_pSettings->m_bNameTags = false;
        netGame->m_pSettings->m_fNameTagsDrawDist = 0.0f;
    }

    if (!playerPool) return;
    const sampapi::ID localId = playerPool->m_nLocalPlayerId;
    for (int playerId = 0; playerId < kMaxPlayers; ++playerId) {
        if (playerId == localId || !playerPool->IsConnected(static_cast<sampapi::ID>(playerId))) continue;
        CRemotePlayer* remote = playerPool->GetPlayer(static_cast<sampapi::ID>(playerId));
        if (remote) {
            remote->m_bDrawLabels = FALSE;
        }
    }
}

bool CreateResources(IDirect3DDevice9* device) {
    if (!device) return false;
    if (g_Device == device && g_NameFont && g_NameFontSmall && g_TagFont && g_TagFontSmall && g_Sprite) {
        return true;
    }

    Nametag::Shutdown();
    g_Device = device;

    auto createFont = [device](int height, ComPtr<ID3DXFont>& font) {
        return D3DXCreateFontW(device, height, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Arial", font.GetAddressOf());
    };

    if (FAILED(createFont(18, g_NameFont)) || FAILED(createFont(13, g_NameFontSmall)) ||
        FAILED(createFont(10, g_TagFont)) || FAILED(createFont(8, g_TagFontSmall)) ||
        FAILED(D3DXCreateSprite(device, g_Sprite.GetAddressOf()))) {
        Nametag::Shutdown();
        return false;
    }

    device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf());
    g_DeviceLost = false;
    g_LastRenderedFrame.store(UINT32_MAX, std::memory_order_release);
    g_LastAnimationTick = GetTickCount();
    return true;
}

void SetupRenderState(IDirect3DDevice9* device) {
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZENABLE, FALSE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
}

struct StateGuard {
    explicit StateGuard(IDirect3DStateBlock9* stateBlock) : stateBlock_(stateBlock) {
        captured_ = stateBlock_ && SUCCEEDED(stateBlock_->Capture());
    }

    ~StateGuard() {
        if (captured_) stateBlock_->Apply();
    }

    IDirect3DStateBlock9* stateBlock_ = nullptr;
    bool captured_ = false;
};

ID3DXFont* SelectNameFont(float scale) {
    return scale < 0.75f ? g_NameFontSmall.Get() : g_NameFont.Get();
}

ID3DXFont* SelectTagFont(float scale) {
    return scale < 0.75f ? g_TagFontSmall.Get() : g_TagFont.Get();
}

float DrawTextBadge(IDirect3DDevice9* device, float x, float y, float scale,
    const RoleSlotClientData& slot)
{
    ID3DXFont* font = SelectTagFont(scale);
    const std::wstring text = D3DHelper::Utf8ToWide(slot.text);
    const SIZE textSize = D3DHelper::MeasureTextW(font, text.c_str());
    const float height = kTagHeight * scale;
    const float width = static_cast<float>(textSize.cx) + kTagPaddingX * scale * 2.0f;
    const float radius = 4.0f * scale;
    const D3DCOLOR badgeColor = slot.color != 0
        ? slot.color
        : D3DCOLOR_ARGB(255, 50, 50, 50);
    const uint8_t backgroundAlpha = slot.bgColor != 0
        ? static_cast<uint8_t>((slot.bgColor >> 24) & 0xFF)
        : 255;
    const D3DCOLOR background = slot.isRainbow
        ? GetRainbowD3DColor(slot.currentHue, backgroundAlpha)
        : (slot.bgColor != 0 ? slot.bgColor : badgeColor);

    D3DHelper::DrawRoundedFilledRect(device, x, y, width, height, radius, background);
    D3DHelper::DrawRoundedBorderRect(device, x, y, width, height, radius, 1.0f, kStrokeColor);

    RECT rect = {static_cast<LONG>(x), static_cast<LONG>(y),
        static_cast<LONG>(x + width), static_cast<LONG>(y + height)};
    if (slot.stroke) {
        D3DHelper::DrawTextStrokeW(font, text.c_str(), rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, D3DCOLOR_ARGB(255, 255, 255, 255), kStrokeColor);
    } else {
        font->DrawTextW(nullptr, text.c_str(), -1, &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE, D3DCOLOR_ARGB(255, 255, 255, 255));
    }
    return width;
}

struct TexturedVertex2D {
    float x, y, z, rhw;
    D3DCOLOR color;
    float u, v;
};
constexpr DWORD kTextureFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

void DrawIcon(IDirect3DDevice9* device, float x, float y, float width, float height,
    IDirect3DTexture9* texture, D3DCOLOR tint) {
    if (!device || !texture) return;
    TexturedVertex2D v[4] = {
        { x,         y + height, 0.f, 1.f, tint, 0.f, 1.f },
        { x,         y,          0.f, 1.f, tint, 0.f, 0.f },
        { x + width, y + height, 0.f, 1.f, tint, 1.f, 1.f },
        { x + width, y,          0.f, 1.f, tint, 1.f, 0.f },
    };
    device->SetTexture(0, texture);
    device->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetFVF(kTextureFVF);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(TexturedVertex2D));
}

struct BadgeItem {
    int slotIndex = 0;
    float width = 0.0f;
    float height = 0.0f;
    IDirect3DTexture9* texture = nullptr;
};

float DrawBadges(IDirect3DDevice9* device, float centerX, float y, float scale,
    const PlayerNametag& nametag)
{
    std::vector<BadgeItem> items;
    const float textHeight = kTagHeight * scale;
    const float iconHeight = 26.0f * scale;
    ID3DXFont* font = SelectTagFont(scale);

    for (int slotIndex = 0; slotIndex < kMaxRoleSlots; ++slotIndex) {
        const RoleSlotClientData& slot = nametag.slots[slotIndex];
        if (!slot.active) continue;

        BadgeItem item{};
        item.slotIndex = slotIndex;
        if (!slot.imagePath.empty()) {
            item.texture = TextureCache::GetOrLoad(device, slot.imagePath);
            if (item.texture) {
                D3DSURFACE_DESC description{};
                if (SUCCEEDED(item.texture->GetLevelDesc(0, &description)) && description.Height > 0) {
                    item.height = iconHeight;
                    item.width = iconHeight * static_cast<float>(description.Width) / description.Height;
                }
            }
        }

        if (item.width <= 0.0f && !slot.text.empty()) {
            const std::wstring text = D3DHelper::Utf8ToWide(slot.text);
            const SIZE size = D3DHelper::MeasureTextW(font, text.c_str());
            item.width = static_cast<float>(size.cx) + kTagPaddingX * scale * 2.0f;
            item.height = textHeight;
            item.texture = nullptr;
        }

        if (item.width > 0.0f) items.push_back(item);
    }

    if (items.empty()) return 0.0f;

    std::vector<std::vector<BadgeItem>> rows(1);
    float rowWidth = 0.0f;
    for (const BadgeItem& item : items) {
        const float nextWidth = rowWidth + (rows.back().empty() ? 0.0f : kTagGap * scale) + item.width;
        if (!rows.back().empty() && nextWidth > kMaxBadgeRowWidth * scale) {
            rows.emplace_back();
            rowWidth = 0.0f;
        }
        if (!rows.back().empty()) rowWidth += kTagGap * scale;
        rows.back().push_back(item);
        rowWidth += item.width;
    }

    float currentY = y;
    float totalHeight = 0.0f;
    for (size_t rowIndex = 0; rowIndex < rows.size(); ++rowIndex) {
        const auto& row = rows[rowIndex];
        float width = 0.0f;
        float height = textHeight;
        for (size_t itemIndex = 0; itemIndex < row.size(); ++itemIndex) {
            if (itemIndex != 0) width += kTagGap * scale;
            width += row[itemIndex].width;
            height = (std::max)(height, row[itemIndex].height);
        }

        float currentX = centerX - width * 0.5f;
        for (const BadgeItem& item : row) {
            const RoleSlotClientData& slot = nametag.slots[item.slotIndex];
            const float itemY = currentY + (height - item.height) * 0.5f;
            if (item.texture) {
                const D3DCOLOR tint = slot.isRainbow
                    ? GetRainbowD3DColor(slot.currentHue)
                    : D3DCOLOR_ARGB(255, 255, 255, 255);
                DrawIcon(device, currentX, itemY, item.width, item.height, item.texture, tint);
            } else {
                DrawTextBadge(device, currentX, itemY, scale, slot);
            }
            currentX += item.width + kTagGap * scale;
        }

        currentY += height;
        totalHeight += height;
        if (rowIndex + 1 < rows.size()) {
            currentY += kGap * scale;
            totalHeight += kGap * scale;
        }
    }
    return totalHeight;
}

float MeasureBadgesHeight(IDirect3DDevice9* device, float scale, const PlayerNametag& nametag) {
    const float textHeight = kTagHeight * scale;
    const float iconHeight = 26.0f * scale;
    ID3DXFont* font = SelectTagFont(scale);

    std::vector<float> itemWidths;
    std::vector<float> itemHeights;

    for (int slotIndex = 0; slotIndex < kMaxRoleSlots; ++slotIndex) {
        const RoleSlotClientData& slot = nametag.slots[slotIndex];
        if (!slot.active) continue;

        float w = 0.0f;
        float h = textHeight;
        if (!slot.imagePath.empty()) {
            LPDIRECT3DTEXTURE9 tex = TextureCache::GetOrLoad(device, slot.imagePath);
            if (tex) {
                D3DSURFACE_DESC description{};
                if (SUCCEEDED(tex->GetLevelDesc(0, &description)) && description.Height > 0) {
                    h = iconHeight;
                    w = iconHeight * static_cast<float>(description.Width) / description.Height;
                }
            }
        }
        if (w <= 0.0f && !slot.text.empty()) {
            const std::wstring text = D3DHelper::Utf8ToWide(slot.text);
            const SIZE size = D3DHelper::MeasureTextW(font, text.c_str());
            w = static_cast<float>(size.cx) + kTagPaddingX * scale * 2.0f;
            h = textHeight;
        }
        if (w > 0.0f) {
            itemWidths.push_back(w);
            itemHeights.push_back(h);
        }
    }

    if (itemWidths.empty()) return 0.0f;

    float currentW = 0.0f;
    float maxRowH = textHeight;
    float totalH = 0.0f;
    int rowItemCount = 0;

    for (size_t i = 0; i < itemWidths.size(); ++i) {
        float testW = currentW + (rowItemCount > 0 ? kTagGap * scale : 0.0f) + itemWidths[i];
        if (rowItemCount > 0 && testW > kMaxBadgeRowWidth * scale) {
            totalH += maxRowH + kGap * scale;
            currentW = 0.0f;
            maxRowH = textHeight;
            rowItemCount = 0;
        }
        currentW += (rowItemCount > 0 ? kTagGap * scale : 0.0f) + itemWidths[i];
        maxRowH = (std::max)(maxRowH, itemHeights[i]);
        rowItemCount++;
    }
    totalH += maxRowH;
    return totalH;
}

void DrawPlayer(IDirect3DDevice9* device, float centerX, float bottomAnchorY, float scale,
    const char* name, int playerId, float health, float armour, const PlayerNametag& nametag)
{
    if (!nametag.visible) return;

    char label[128] = {};
    sprintf_s(label, "%s (%d)", name, playerId);
    const std::wstring wideLabel = D3DHelper::Utf8ToWide(label);
    ID3DXFont* nameFont = SelectNameFont(scale);
    const SIZE labelSize = D3DHelper::MeasureTextW(nameFont, wideLabel.c_str());

    const float gap = kGap * scale;
    const float barWidth = kBarWidth * scale;
    const float barHeight = kBarHeight * scale;

    const float nameTextHeight = static_cast<float>(labelSize.cy);
    const float healthBarHeight = barHeight;
    const float armourBarHeight = (armour > 0.0f) ? (barHeight + gap) : 0.0f;

    const float badgeHeight = MeasureBadgesHeight(device, scale, nametag);
    const float badgeSectionHeight = (badgeHeight > 0.0f) ? (badgeHeight + gap) : 0.0f;

    const float totalLayoutHeight = badgeSectionHeight + nameTextHeight + gap + healthBarHeight + armourBarHeight;

    float currentY = bottomAnchorY - totalLayoutHeight;

    // 1. Draw Badges
    if (badgeHeight > 0.0f) {
        DrawBadges(device, centerX, currentY, scale, nametag);
        currentY += badgeSectionHeight;
    }

    // 2. Draw Name "Name (ID)"
    const D3DCOLOR color = nametag.isNametagRainbow
        ? GetRainbowD3DColor(nametag.nametagRainbowHue)
        : (nametag.hasCustomNametagColor ? nametag.nametagColor : kNameColor);
    RECT labelRect = {
        static_cast<LONG>(centerX - labelSize.cx * 0.5f),
        static_cast<LONG>(currentY),
        static_cast<LONG>(centerX + labelSize.cx * 0.5f + 2.0f),
        static_cast<LONG>(currentY + labelSize.cy)
    };
    D3DHelper::DrawTextStrokeW(nameFont, wideLabel.c_str(), labelRect, DT_LEFT | DT_NOCLIP, color, kStrokeColor);
    currentY += nameTextHeight + gap;

    // 3. Draw Health Progress Bar
    const float barX = centerX - barWidth * 0.5f;
    D3DHelper::DrawProgressBar(device, barX, currentY, barWidth, barHeight,
        3.0f * scale, health, kBarBackground, kHealthColor);
    currentY += barHeight;

    // 4. Draw Armour Progress Bar (if > 0)
    if (armour > 0.0f) {
        currentY += gap;
        D3DHelper::DrawProgressBar(device, barX, currentY, barWidth, barHeight,
            3.0f * scale, armour, kBarBackground, kArmourColor);
    }
}

void UpdateRainbowAnimations() {
    const DWORD now = GetTickCount();
    const DWORD elapsed = g_LastAnimationTick == 0 ? 0 : now - g_LastAnimationTick;
    g_LastAnimationTick = now;
    if (elapsed == 0) return;

    for (PlayerNametag& nametag : g_Players) {
        if (nametag.isNametagRainbow) {
            const float period = static_cast<float>((std::max)(nametag.nametagRainbowSpeedMs, 50u));
            nametag.nametagRainbowHue = std::fmod(nametag.nametagRainbowHue + 360.0f * elapsed / period, 360.0f);
        }
        for (RoleSlotClientData& slot : nametag.slots) {
            if (!slot.active || !slot.isRainbow) continue;
            const float period = static_cast<float>((std::max)(slot.rainbowSpeedMs, 50u));
            slot.currentHue = std::fmod(slot.currentHue + 360.0f * elapsed / period, 360.0f);
        }
    }
}

} // namespace

void Nametag::RenderAll(IDirect3DDevice9* device) {
    CNetGame* netGame = GetRefNetGame();
    if (!netGame) return;

    CPlayerPool* playerPool = netGame->GetPlayerPool();
    SuppressNativeNametags(netGame, playerPool);
    if (!playerPool || g_DeviceLost || !CreateResources(device)) return;

    CLocalPlayer* localPlayer = playerPool->GetLocalPlayer();
    if (!localPlayer || !localPlayer->m_bIsActive || !localPlayer->m_pPed || !localPlayer->m_pPed->m_pGamePed) return;

    sampapi::CVector localPosition{};
    localPlayer->m_pPed->GetBonePosition(8, &localPosition);

    TextureCache::FlushPending(device);
    if (!g_StateBlock) device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf());
    StateGuard stateGuard(g_StateBlock.Get());
    SetupRenderState(device);

    D3DVIEWPORT9 viewport{};
    device->GetViewport(&viewport);
    const float drawDistance = (std::max)(RoleConfig::GetDrawDistance(), 1.0f);

    std::array<PlayerNametag, kMaxPlayers> nametagSnapshot;
    {
        std::lock_guard<std::mutex> dataGuard(g_PlayerDataMutex);
        UpdateRainbowAnimations();
        nametagSnapshot = g_Players;
    }

    const sampapi::ID localId = playerPool->m_nLocalPlayerId;
    for (int playerId = 0; playerId < kMaxPlayers; ++playerId) {
        if (playerId == localId || !playerPool->IsConnected(static_cast<sampapi::ID>(playerId))) continue;

        CRemotePlayer* remote = playerPool->GetPlayer(static_cast<sampapi::ID>(playerId));
        if (!remote || !remote->m_pPed || !remote->m_pPed->m_pGamePed) continue;

        sampapi::CVector worldPosition{};
        remote->m_pPed->GetBonePosition(8, &worldPosition);
        worldPosition.z += kHeadOffsetZ;

        const float deltaX = worldPosition.x - localPosition.x;
        const float deltaY = worldPosition.y - localPosition.y;
        const float deltaZ = worldPosition.z - localPosition.z;
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
        if (distance > drawDistance) continue;

        float screenX = 0.0f;
        float screenY = 0.0f;
        if (!W2S::ToScreen(worldPosition, screenX, screenY) || screenX < 0.0f || screenY < 0.0f ||
            screenX > viewport.Width || screenY > viewport.Height) {
            continue;
        }

        const float scale = (std::max)(0.55f, 1.0f - distance / drawDistance * 0.45f);
        const char* name = playerPool->GetName(static_cast<sampapi::ID>(playerId));
        if (!name || name[0] == '\0') name = "Player";

        DrawPlayer(device, screenX, screenY, scale, name, playerId,
            remote->m_pPed->GetHealth(), remote->m_pPed->GetArmour(), nametagSnapshot[playerId]);
    }
}

void Nametag::OnLostDevice() {
    if (g_DeviceLost) return;
    g_DeviceLost = true;
    g_StateBlock.Reset();
    if (g_NameFont) g_NameFont->OnLostDevice();
    if (g_NameFontSmall) g_NameFontSmall->OnLostDevice();
    if (g_TagFont) g_TagFont->OnLostDevice();
    if (g_TagFontSmall) g_TagFontSmall->OnLostDevice();
    if (g_Sprite) g_Sprite->OnLostDevice();
}

void Nametag::OnResetDevice(IDirect3DDevice9* device) {
    if (g_NameFont) g_NameFont->OnResetDevice();
    if (g_NameFontSmall) g_NameFontSmall->OnResetDevice();
    if (g_TagFont) g_TagFont->OnResetDevice();
    if (g_TagFontSmall) g_TagFontSmall->OnResetDevice();
    if (g_Sprite) g_Sprite->OnResetDevice();
    g_Device = device;
    if (device) device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf());
    g_LastRenderedFrame.store(UINT32_MAX, std::memory_order_release);
    g_DeviceLost = false;
}

void Nametag::Shutdown() {
    g_StateBlock.Reset();
    g_Sprite.Reset();
    g_TagFontSmall.Reset();
    g_TagFont.Reset();
    g_NameFontSmall.Reset();
    g_NameFont.Reset();
    TextureCache::ReleaseAll();
    g_Device = nullptr;
    g_DeviceLost = false;
    g_LastRenderedFrame.store(UINT32_MAX, std::memory_order_release);
}
