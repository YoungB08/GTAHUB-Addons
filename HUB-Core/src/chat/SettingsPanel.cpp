#include "pch.h"
#include "SettingsPanel.h"

#include "ChatManager.h"
#include "ChatSettings.h"
#include "ChatTextRenderer.h"
#include "D3DHelper.h"
#include "Logger.h"

#include <sampapi/0.3.DL-1/CGame.h>
#include <sampapi/0.3.DL-1/CInput.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <string>

using namespace sampapi::v03dl;

namespace HUB::Chat::SettingsPanel {

namespace {

bool g_Open = false;
int g_ActiveTab = 0; // 0 = Layout, 1 = Behavior, 2 = Colors
bool g_DraggingSlider = false;
int g_DraggedSliderId = -1;
bool g_MousePressed = false;
bool g_MouseReleased = false;
bool g_LastPolledMouseDown = false;
DWORD g_LastEscTick = 0;

constexpr float kBackgroundSliderOffset = 22.0f;
constexpr float kTextSliderOffset = 172.0f;
constexpr float kColorSliderSpacing = 30.0f;
constexpr float kThemeTitleOffset = 266.0f;
constexpr float kThemeButtonOffset = 288.0f;

void DrawTextSimple(IDirect3DDevice9* dev, const std::wstring& text, float x, float y, float w, float h, D3DCOLOR color, DWORD fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) {
    if (text.empty() || !dev) return;
    HUB::Chat::TextRenderer::Draw(text, x, y, x + w, h, color, fmt);
}

void RenderSlider(IDirect3DDevice9* dev, const std::wstring& label, float minVal, float maxVal, float curVal, float x, float y, float w, bool isFloat, wchar_t* valStr, D3DCOLOR themeColor = D3DCOLOR_ARGB(255, 64, 150, 255)) {
    DrawTextSimple(dev, label, x, y, 160.0f, 22.0f, D3DCOLOR_ARGB(255, 230, 230, 240));

    const float trackX = x + 165.0f;
    const float trackY = y + 7.0f;
    const float trackW = w - 245.0f;
    const float trackH = 8.0f;

    // Track Background
    D3DHelper::DrawRoundedFilledRect(dev, trackX, trackY, trackW, trackH, 4.0f, D3DCOLOR_ARGB(255, 35, 38, 50));
    
    // Fill
    float ratio = (curVal - minVal) / (maxVal - minVal);
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    const float fillW = (std::max)(6.0f, trackW * ratio);
    D3DHelper::DrawRoundedFilledRect(dev, trackX, trackY, fillW, trackH, 4.0f, themeColor);

    // Handle Knob
    const float knobX = trackX + fillW - 7.0f;
    const float knobY = trackY - 4.0f;
    D3DHelper::DrawRoundedFilledRect(dev, knobX, knobY, 14.0f, 16.0f, 4.0f, D3DCOLOR_ARGB(255, 255, 255, 255));
    D3DHelper::DrawRoundedBorderRect(dev, knobX, knobY, 14.0f, 16.0f, 4.0f, 1.0f, themeColor);

    // Value text
    DrawTextSimple(dev, valStr, trackX + trackW + 15.0f, y, 60.0f, 22.0f, themeColor, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void RenderToggle(IDirect3DDevice9* dev, const std::wstring& label, bool enabled, float x, float y) {
    DrawTextSimple(dev, label, x, y, 220.0f, 24.0f, D3DCOLOR_ARGB(255, 230, 230, 240));

    const float switchX = x + 240.0f;
    const float switchY = y + 2.0f;
    const float switchW = 48.0f;
    const float switchH = 20.0f;

    D3DCOLOR bg = enabled ? D3DCOLOR_ARGB(255, 46, 204, 113) : D3DCOLOR_ARGB(255, 60, 64, 78);
    D3DHelper::DrawRoundedFilledRect(dev, switchX, switchY, switchW, switchH, 10.0f, bg);

    const float knobX = switchX + (enabled ? 28.0f : 2.0f);
    D3DHelper::DrawRoundedFilledRect(dev, knobX, switchY + 2.0f, 16.0f, 16.0f, 8.0f, D3DCOLOR_ARGB(255, 255, 255, 255));

    DrawTextSimple(dev, enabled ? L"ON" : L"OFF", switchX + switchW + 12.0f, y, 40.0f, 24.0f, enabled ? D3DCOLOR_ARGB(255, 46, 204, 113) : D3DCOLOR_ARGB(255, 140, 145, 160));
}

void UpdateSliderValue(int id, float ratio) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    ChatSettings cfg = HUB::Chat::GetSettingsStore().Get();

    switch (id) {
        case 0: cfg.scale = 0.8f + ratio * (1.5f - 0.8f); break;
        case 1: cfg.width = static_cast<int>(std::lround(320.0f + ratio * (900.0f - 320.0f))); break;
        case 2: cfg.height = static_cast<int>(std::lround(180.0f + ratio * (600.0f - 180.0f))); break;
        case 3: cfg.fontSize = static_cast<int>(std::lround(10.0f + ratio * (24.0f - 10.0f))); break;
        case 4: cfg.lineHeight = static_cast<int>(std::lround(14.0f + ratio * (36.0f - 14.0f))); break;
        case 5: cfg.marginLeft = static_cast<int>(std::lround(0.0f + ratio * 200.0f)); break;
        case 6: cfg.marginTop = static_cast<int>(std::lround(0.0f + ratio * 200.0f)); break;
        case 7: cfg.opacity = static_cast<int>(std::lround(ratio * 255.0f)); break;
        case 8: cfg.idleOpacity = static_cast<int>(std::lround(ratio * 255.0f)); break;
        case 9: cfg.fadeDelayMs = static_cast<int>(std::lround(1000.0f + ratio * (30000.0f - 1000.0f))); break;

        // Color Pickers
        case 10: { // Background Red
            uint8_t a = (cfg.backgroundColor >> 24) & 0xFF;
            uint8_t g = (cfg.backgroundColor >> 8) & 0xFF;
            uint8_t b = cfg.backgroundColor & 0xFF;
            uint8_t r = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.backgroundColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 11: { // Background Green
            uint8_t a = (cfg.backgroundColor >> 24) & 0xFF;
            uint8_t r = (cfg.backgroundColor >> 16) & 0xFF;
            uint8_t b = cfg.backgroundColor & 0xFF;
            uint8_t g = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.backgroundColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 12: { // Background Blue
            uint8_t a = (cfg.backgroundColor >> 24) & 0xFF;
            uint8_t r = (cfg.backgroundColor >> 16) & 0xFF;
            uint8_t g = (cfg.backgroundColor >> 8) & 0xFF;
            uint8_t b = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.backgroundColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 13: { // Background Alpha / Opacity
            uint8_t r = (cfg.backgroundColor >> 16) & 0xFF;
            uint8_t g = (cfg.backgroundColor >> 8) & 0xFF;
            uint8_t b = cfg.backgroundColor & 0xFF;
            uint8_t a = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.backgroundColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 14: { // Text Red
            uint8_t a = (cfg.textColor >> 24) & 0xFF;
            uint8_t g = (cfg.textColor >> 8) & 0xFF;
            uint8_t b = cfg.textColor & 0xFF;
            uint8_t r = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.textColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 15: { // Text Green
            uint8_t a = (cfg.textColor >> 24) & 0xFF;
            uint8_t r = (cfg.textColor >> 16) & 0xFF;
            uint8_t b = cfg.textColor & 0xFF;
            uint8_t g = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.textColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        case 16: { // Text Blue
            uint8_t a = (cfg.textColor >> 24) & 0xFF;
            uint8_t r = (cfg.textColor >> 16) & 0xFF;
            uint8_t g = (cfg.textColor >> 8) & 0xFF;
            uint8_t b = static_cast<uint8_t>(std::lround(ratio * 255.0f));
            cfg.textColor = D3DCOLOR_ARGB(a, r, g, b);
            break;
        }
        default: break;
    }
    HUB::Chat::GetSettingsStore().Save(cfg);
}

} // namespace

void Initialize() {
    g_Open = false;
    g_ActiveTab = 0;
    g_DraggingSlider = false;
    g_DraggedSliderId = -1;
    g_MousePressed = false;
    g_MouseReleased = false;
    g_LastPolledMouseDown = false;
    g_LastEscTick = 0;
    Logger::Info("SettingsPanel initialized.");
}

void Open() {
    if (g_Open) return;
    g_Open = true;
    g_DraggingSlider = false;
    g_DraggedSliderId = -1;
    g_MousePressed = false;
    g_MouseReleased = false;
    g_LastPolledMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    CGame* game = GetRefGame();
    if (game) {
        game->SetCursorMode(CURSOR_LOCKCAMANDCONTROL, FALSE);
    }
    Logger::Info("SettingsPanel opened.");
}

void Close() {
    if (!g_Open) return;
    g_Open = false;
    g_DraggingSlider = false;
    g_DraggedSliderId = -1;
    g_MousePressed = false;
    g_MouseReleased = false;
    CGame* game = GetRefGame();
    HWND gameWindow = game ? static_cast<HWND>(game->GetWindowHandle()) : nullptr;
    if (gameWindow && GetCapture() == gameWindow) ReleaseCapture();
    if (game) {
        game->SetCursorMode(CURSOR_NONE, TRUE);
    }
    Logger::Info("SettingsPanel closed.");
}

void Toggle() {
    if (g_Open) Close();
    else Open();
}

bool IsOpen() {
    return g_Open;
}

void Render(IDirect3DDevice9* device) {
    if (!g_Open || !device) return;

    D3DVIEWPORT9 vp{};
    if (FAILED(device->GetViewport(&vp))) return;

    const float screenW = static_cast<float>(vp.Width);
    const float screenH = static_cast<float>(vp.Height);

    HWND hwnd = GetRefGame() ? static_cast<HWND>(GetRefGame()->GetWindowHandle()) : nullptr;
    const DWORD now = GetTickCount();
    if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 && now - g_LastEscTick > 300) {
        g_LastEscTick = now;
        Close();
        return;
    }

    const bool polledMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool isDown = polledMouseDown;
    const bool justPressed = g_MousePressed || (polledMouseDown && !g_LastPolledMouseDown);
    const bool justReleased = g_MouseReleased || (!polledMouseDown && g_LastPolledMouseDown);
    g_LastPolledMouseDown = polledMouseDown;
    g_MousePressed = false;
    g_MouseReleased = false;

    POINT clientPt{};
    GetCursorPos(&clientPt);
    if (hwnd) ScreenToClient(hwnd, &clientPt);

    RECT rc{};
    if (hwnd) GetClientRect(hwnd, &rc);
    const float clientW = static_cast<float>(rc.right - rc.left);
    const float clientH = static_cast<float>(rc.bottom - rc.top);

    float ptX = static_cast<float>(clientPt.x);
    float ptY = static_cast<float>(clientPt.y);

    if (clientW > 0.0f && clientH > 0.0f && screenW > 0.0f && screenH > 0.0f) {
        ptX = ptX * (screenW / clientW);
        ptY = ptY * (screenH / clientH);
    }

    // Center Panel Dimensions
    const float panelW = 580.0f;
    const float panelH = 430.0f;
    const float panelX = (screenW - panelW) * 0.5f;
    const float panelY = (screenH - panelH) * 0.5f;
    const float startY = panelY + 94.0f;
    const float startX = panelX + 24.0f;

    // Handle Mouse Slider Dragging
    if (justReleased || !isDown) {
        g_DraggingSlider = false;
        g_DraggedSliderId = -1;
    }

    if (g_DraggingSlider && g_DraggedSliderId >= 0 && isDown) {
        const float trackX = startX + 165.0f;
        const float trackW = (panelW - 48.0f) - 245.0f;
        const float ratio = (ptX - trackX) / trackW;
        UpdateSliderValue(g_DraggedSliderId, ratio);
    }

    // Handle Mouse Clicks
    if (justPressed) {
        // Close button [X]
        const float closeX = panelX + panelW - 38.0f;
        const float closeY = panelY + 8.0f;
        if (ptX >= closeX && ptX <= closeX + 28.0f && ptY >= closeY && ptY <= closeY + 24.0f) {
            Close();
            return;
        }

        // Tabs
        for (int i = 0; i < 3; ++i) {
            const float tabX = panelX + 16.0f + i * 115.0f;
            const float tabY = panelY + 50.0f;
            if (ptX >= tabX && ptX <= tabX + 105.0f && ptY >= tabY && ptY <= tabY + 28.0f) {
                g_ActiveTab = i;
                break;
            }
        }

        ChatSettings cfg = HUB::Chat::GetSettingsStore().Get();

        if (g_ActiveTab == 0) { // Layout Sliders
            const float trackX = startX + 165.0f;
            const float trackW = (panelW - 48.0f) - 245.0f;
            for (int i = 0; i < 7; ++i) {
                const float sY = startY + i * 36.0f;
                if (ptX >= trackX - 15.0f && ptX <= trackX + trackW + 15.0f && ptY >= sY - 5.0f && ptY <= sY + 26.0f) {
                    g_DraggingSlider = true;
                    g_DraggedSliderId = i;
                    const float ratio = (ptX - trackX) / trackW;
                    UpdateSliderValue(i, ratio);
                    break;
                }
            }
        }
        else if (g_ActiveTab == 1) { // Behavior Controls
            const float trackX = startX + 165.0f;
            const float trackW = (panelW - 48.0f) - 245.0f;

            for (int i = 0; i < 3; ++i) {
                const float sY = startY + i * 36.0f;
                if (ptX >= trackX - 15.0f && ptX <= trackX + trackW + 15.0f && ptY >= sY - 5.0f && ptY <= sY + 26.0f) {
                    g_DraggingSlider = true;
                    g_DraggedSliderId = 7 + i;
                    const float ratio = (ptX - trackX) / trackW;
                    UpdateSliderValue(7 + i, ratio);
                    break;
                }
            }

            const float switchX = startX + 240.0f;
            if (ptX >= switchX && ptX <= switchX + 80.0f) {
                if (ptY >= startY + 118.0f && ptY <= startY + 142.0f) {
                    cfg.timestamps = !cfg.timestamps;
                    HUB::Chat::GetSettingsStore().Save(cfg);
                }
                else if (ptY >= startY + 154.0f && ptY <= startY + 178.0f) {
                    cfg.smoothScroll = !cfg.smoothScroll;
                    HUB::Chat::GetSettingsStore().Save(cfg);
                }
                else if (ptY >= startY + 190.0f && ptY <= startY + 214.0f) {
                    cfg.fadeEnabled = !cfg.fadeEnabled;
                    HUB::Chat::GetSettingsStore().Save(cfg);
                }
            }
        }
        else if (g_ActiveTab == 2) { // Colors Tab (Color Pickers + Presets)
            const float trackX = startX + 165.0f;
            const float trackW = (panelW - 48.0f) - 245.0f;

            // Background Color Sliders (10..13)
            for (int i = 0; i < 4; ++i) {
                const float sY = startY + kBackgroundSliderOffset + i * kColorSliderSpacing;
                if (ptX >= trackX - 15.0f && ptX <= trackX + trackW + 15.0f && ptY >= sY - 5.0f && ptY <= sY + 24.0f) {
                    g_DraggingSlider = true;
                    g_DraggedSliderId = 10 + i;
                    const float ratio = (ptX - trackX) / trackW;
                    UpdateSliderValue(10 + i, ratio);
                    break;
                }
            }

            // Text Color Sliders (14..16)
            for (int i = 0; i < 3; ++i) {
                const float sY = startY + kTextSliderOffset + i * kColorSliderSpacing;
                if (ptX >= trackX - 15.0f && ptX <= trackX + trackW + 15.0f && ptY >= sY - 5.0f && ptY <= sY + 24.0f) {
                    g_DraggingSlider = true;
                    g_DraggedSliderId = 14 + i;
                    const float ratio = (ptX - trackX) / trackW;
                    UpdateSliderValue(14 + i, ratio);
                    break;
                }
            }

            // Quick Theme Presets
            const struct { D3DCOLOR bg; D3DCOLOR text; D3DCOLOR ts; } themes[] = {
                { D3DCOLOR_ARGB(208, 18, 18, 20), D3DCOLOR_ARGB(255, 255, 255, 255), D3DCOLOR_ARGB(255, 170, 170, 170) },
                { D3DCOLOR_ARGB(220, 10, 20, 35), D3DCOLOR_ARGB(255, 240, 248, 255), D3DCOLOR_ARGB(255, 100, 180, 255) },
                { D3DCOLOR_ARGB(220, 25, 15, 35), D3DCOLOR_ARGB(255, 245, 240, 255), D3DCOLOR_ARGB(255, 180, 120, 255) },
                { D3DCOLOR_ARGB(120, 0, 0, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DCOLOR_ARGB(255, 200, 200, 200) },
                { D3DCOLOR_ARGB(230, 20, 20, 20), D3DCOLOR_ARGB(255, 255, 215, 0),   D3DCOLOR_ARGB(255, 190, 190, 190) }
            };

            for (int i = 0; i < 5; ++i) {
                const float btnX = startX + i * 105.0f;
                const float btnY = startY + kThemeButtonOffset;
                if (ptX >= btnX && ptX <= btnX + 98.0f && ptY >= btnY && ptY <= btnY + 36.0f) {
                    cfg.backgroundColor = themes[i].bg;
                    cfg.textColor = themes[i].text;
                    cfg.timestampColor = themes[i].ts;
                    HUB::Chat::GetSettingsStore().Save(cfg);
                    break;
                }
            }
        }
    }

    // 1. Sleek Modern Translucent Glassmorphism Panel
    D3DHelper::DrawRoundedFilledRect(device, panelX, panelY, panelW, panelH, 12.0f, D3DCOLOR_ARGB(240, 16, 18, 24));
    D3DHelper::DrawRoundedBorderRect(device, panelX, panelY, panelW, panelH, 12.0f, 1.5f, D3DCOLOR_ARGB(255, 64, 150, 255));

    // 2. Header Bar
    D3DHelper::DrawRoundedFilledRect(device, panelX + 2.0f, panelY + 2.0f, panelW - 4.0f, 40.0f, 10.0f, D3DCOLOR_ARGB(250, 24, 28, 38));
    DrawTextSimple(device, L"HUB CORE - CHAT CONFIGURATION", panelX + 16.0f, panelY + 10.0f, 340.0f, 20.0f, D3DCOLOR_ARGB(255, 255, 255, 255));

    // Close Button [X]
    const float closeX = panelX + panelW - 38.0f;
    const float closeY = panelY + 8.0f;
    D3DHelper::DrawRoundedFilledRect(device, closeX, closeY, 28.0f, 24.0f, 6.0f, D3DCOLOR_ARGB(255, 219, 68, 85));
    DrawTextSimple(device, L"X", closeX, closeY, 28.0f, 24.0f, D3DCOLOR_ARGB(255, 255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 3. Category Tabs
    const wchar_t* tabs[] = { L"Layout", L"Behavior", L"Colors" };
    for (int i = 0; i < 3; ++i) {
        const float tabX = panelX + 16.0f + i * 115.0f;
        const float tabY = panelY + 50.0f;
        const bool active = (g_ActiveTab == i);
        D3DCOLOR tabBg = active ? D3DCOLOR_ARGB(255, 64, 150, 255) : D3DCOLOR_ARGB(220, 32, 36, 48);
        D3DHelper::DrawRoundedFilledRect(device, tabX, tabY, 105.0f, 28.0f, 6.0f, tabBg);
        DrawTextSimple(device, tabs[i], tabX, tabY, 105.0f, 28.0f, D3DCOLOR_ARGB(255, 255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const ChatSettings& cfg = HUB::Chat::GetSettingsStore().Get();
    wchar_t buf[32]{};

    // 4. Render Active Tab Controls
    if (g_ActiveTab == 0) { // Layout
        swprintf_s(buf, L"%.2f", cfg.scale);
        RenderSlider(device, L"UI Scale:", 0.8f, 1.5f, cfg.scale, startX, startY, panelW - 48.0f, true, buf);

        swprintf_s(buf, L"%d px", cfg.width);
        RenderSlider(device, L"Width:", 320.0f, 900.0f, static_cast<float>(cfg.width), startX, startY + 36.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d px", cfg.height);
        RenderSlider(device, L"Height:", 180.0f, 600.0f, static_cast<float>(cfg.height), startX, startY + 72.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d pt", cfg.fontSize);
        RenderSlider(device, L"Font Size:", 10.0f, 24.0f, static_cast<float>(cfg.fontSize), startX, startY + 108.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d px", cfg.lineHeight);
        RenderSlider(device, L"Line Height:", 14.0f, 36.0f, static_cast<float>(cfg.lineHeight), startX, startY + 144.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d px", cfg.marginLeft);
        RenderSlider(device, L"Margin Left:", 0.0f, 200.0f, static_cast<float>(cfg.marginLeft), startX, startY + 180.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d px", cfg.marginTop);
        RenderSlider(device, L"Margin Top:", 0.0f, 200.0f, static_cast<float>(cfg.marginTop), startX, startY + 216.0f, panelW - 48.0f, false, buf);
    }
    else if (g_ActiveTab == 1) { // Behavior
        swprintf_s(buf, L"%d / 255", cfg.opacity);
        RenderSlider(device, L"Active Opacity:", 0.0f, 255.0f, static_cast<float>(cfg.opacity), startX, startY, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d / 255", cfg.idleOpacity);
        RenderSlider(device, L"Idle Opacity:", 0.0f, 255.0f, static_cast<float>(cfg.idleOpacity), startX, startY + 36.0f, panelW - 48.0f, false, buf);

        swprintf_s(buf, L"%d s", cfg.fadeDelayMs / 1000);
        RenderSlider(device, L"Fade Delay:", 1000.0f, 30000.0f, static_cast<float>(cfg.fadeDelayMs), startX, startY + 72.0f, panelW - 48.0f, false, buf);

        RenderToggle(device, L"Show Timestamps [00:15]:", cfg.timestamps, startX, startY + 118.0f);
        RenderToggle(device, L"Smooth 60FPS Scroll:", cfg.smoothScroll, startX, startY + 154.0f);
        RenderToggle(device, L"Auto Fade Out Idle Chat:", cfg.fadeEnabled, startX, startY + 190.0f);
    }
    else if (g_ActiveTab == 2) { // Colors Tab (RGBA Background & Text Color Pickers)
        const uint8_t bgA = (cfg.backgroundColor >> 24) & 0xFF;
        const uint8_t bgR = (cfg.backgroundColor >> 16) & 0xFF;
        const uint8_t bgG = (cfg.backgroundColor >> 8) & 0xFF;
        const uint8_t bgB = cfg.backgroundColor & 0xFF;

        const uint8_t txtR = (cfg.textColor >> 16) & 0xFF;
        const uint8_t txtG = (cfg.textColor >> 8) & 0xFF;
        const uint8_t txtB = cfg.textColor & 0xFF;

        // Title 1: Background Color
        DrawTextSimple(device, L"Background Color (RGBA Picker):", startX, startY - 5.0f, 260.0f, 20.0f, D3DCOLOR_ARGB(255, 230, 230, 240));
        
        // Live Color Swatch Preview
        D3DHelper::DrawRoundedFilledRect(device, startX + 300.0f, startY - 5.0f, 80.0f, 22.0f, 4.0f, cfg.backgroundColor);
        D3DHelper::DrawRoundedBorderRect(device, startX + 300.0f, startY - 5.0f, 80.0f, 22.0f, 4.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255));
        DrawTextSimple(device, L"Preview", startX + 300.0f, startY - 5.0f, 80.0f, 22.0f, D3DCOLOR_ARGB(255, 255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        swprintf_s(buf, L"%d", bgR);
        RenderSlider(device, L"Background Red:", 0.0f, 255.0f, static_cast<float>(bgR), startX, startY + kBackgroundSliderOffset, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 231, 76, 60));

        swprintf_s(buf, L"%d", bgG);
        RenderSlider(device, L"Background Green:", 0.0f, 255.0f, static_cast<float>(bgG), startX, startY + kBackgroundSliderOffset + kColorSliderSpacing, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 46, 204, 113));

        swprintf_s(buf, L"%d", bgB);
        RenderSlider(device, L"Background Blue:", 0.0f, 255.0f, static_cast<float>(bgB), startX, startY + kBackgroundSliderOffset + kColorSliderSpacing * 2.0f, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 52, 152, 219));

        swprintf_s(buf, L"%d", bgA);
        RenderSlider(device, L"Background Alpha:", 0.0f, 255.0f, static_cast<float>(bgA), startX, startY + kBackgroundSliderOffset + kColorSliderSpacing * 3.0f, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 190, 195, 205));

        // Title 2: Text Color
        DrawTextSimple(device, L"Text Color (RGB Picker):", startX, startY + 146.0f, 260.0f, 20.0f, D3DCOLOR_ARGB(255, 230, 230, 240));
        
        // Text Color Swatch
        D3DHelper::DrawRoundedFilledRect(device, startX + 300.0f, startY + 146.0f, 80.0f, 22.0f, 4.0f, cfg.textColor);
        D3DHelper::DrawRoundedBorderRect(device, startX + 300.0f, startY + 146.0f, 80.0f, 22.0f, 4.0f, 1.0f, D3DCOLOR_ARGB(255, 255, 255, 255));
        DrawTextSimple(device, L"Text", startX + 300.0f, startY + 146.0f, 80.0f, 22.0f, D3DCOLOR_ARGB(255, 20, 20, 20), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        swprintf_s(buf, L"%d", txtR);
        RenderSlider(device, L"Text Red:", 0.0f, 255.0f, static_cast<float>(txtR), startX, startY + kTextSliderOffset, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 231, 76, 60));

        swprintf_s(buf, L"%d", txtG);
        RenderSlider(device, L"Text Green:", 0.0f, 255.0f, static_cast<float>(txtG), startX, startY + kTextSliderOffset + kColorSliderSpacing, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 46, 204, 113));

        swprintf_s(buf, L"%d", txtB);
        RenderSlider(device, L"Text Blue:", 0.0f, 255.0f, static_cast<float>(txtB), startX, startY + kTextSliderOffset + kColorSliderSpacing * 2.0f, panelW - 48.0f, false, buf, D3DCOLOR_ARGB(255, 52, 152, 219));

        // Quick Preset Themes
        DrawTextSimple(device, L"Quick Preset Themes:", startX, startY + kThemeTitleOffset, 200.0f, 20.0f, D3DCOLOR_ARGB(255, 230, 230, 240));

        const struct { const wchar_t* name; D3DCOLOR bg; D3DCOLOR text; D3DCOLOR ts; } themes[] = {
            { L"Default", D3DCOLOR_ARGB(208, 18, 18, 20), D3DCOLOR_ARGB(255, 255, 255, 255), D3DCOLOR_ARGB(255, 170, 170, 170) },
            { L"Midnight", D3DCOLOR_ARGB(220, 10, 20, 35), D3DCOLOR_ARGB(255, 240, 248, 255), D3DCOLOR_ARGB(255, 100, 180, 255) },
            { L"Purple",   D3DCOLOR_ARGB(220, 25, 15, 35), D3DCOLOR_ARGB(255, 245, 240, 255), D3DCOLOR_ARGB(255, 180, 120, 255) },
            { L"Clear",    D3DCOLOR_ARGB(120, 0, 0, 0), D3DCOLOR_ARGB(255, 255, 255, 255), D3DCOLOR_ARGB(255, 200, 200, 200) },
            { L"Gold",     D3DCOLOR_ARGB(230, 20, 20, 20), D3DCOLOR_ARGB(255, 255, 215, 0),   D3DCOLOR_ARGB(255, 190, 190, 190) }
        };

        for (int i = 0; i < 5; ++i) {
            const float btnX = startX + i * 105.0f;
            const float btnY = startY + kThemeButtonOffset;
            D3DHelper::DrawRoundedFilledRect(device, btnX, btnY, 98.0f, 30.0f, 6.0f, themes[i].bg);
            D3DHelper::DrawRoundedBorderRect(device, btnX, btnY, 98.0f, 30.0f, 6.0f, 1.0f, D3DCOLOR_ARGB(255, 80, 85, 100));
            DrawTextSimple(device, themes[i].name, btnX, btnY, 98.0f, 30.0f, themes[i].text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

bool HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM) {
    if (!g_Open) return false;

    switch (message) {
        case WM_MOUSEMOVE:
            return true;
        case WM_LBUTTONDOWN:
            g_MousePressed = true;
            g_MouseReleased = false;
            SetCapture(hwnd);
            return true;
        case WM_LBUTTONUP:
            g_MouseReleased = true;
            if (GetCapture() == hwnd) ReleaseCapture();
            return true;
        case WM_CAPTURECHANGED:
        case WM_CANCELMODE:
            g_MouseReleased = true;
            return true;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam == VK_ESCAPE) Close();
            return true;
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_UNICHAR:
        case WM_MOUSEWHEEL:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return true;
        default:
            return false;
    }
}

} // namespace HUB::Chat::SettingsPanel
