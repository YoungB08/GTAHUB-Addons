#include "pch.h"
#include "CustomChat.h"

#include "ChatManager.h"
#include "ChatSettings.h"
#include "ChatTextRenderer.h"
#include "CustomChatInput.h"
#include "D3DHelper.h"
#include "HookManager.h"
#include "Logger.h"
#include "PlayerData.h"
#include "SettingsPanel.h"
#include "TextureCache.h"

#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <string>
#include <vector>

#pragma comment(lib, "d3d9.lib")

using Microsoft::WRL::ComPtr;

namespace {

struct TextSpan {
    std::wstring text;
    std::string imagePath;
    D3DCOLOR color = 0xFFFFFFFF;
    float width = 0.0f;
    bool isRainbow = false;
    uint32_t rainbowSpeedMs = 500;
    bool isImage = false;
};

struct RenderLine {
    std::wstring timestamp;
    std::wstring prefix;
    std::vector<TextSpan> spans;
    D3DCOLOR prefixColor = 0xFFFFFFFF;
    float timestampWidth = 0.0f;
    float prefixWidth = 0.0f;
};

struct ParsedText {
    std::wstring text;
    std::vector<D3DCOLOR> colors;
};

struct InputRenderCache {
    uint64_t generation = (std::numeric_limits<uint64_t>::max)();
    float availableWidth = 0.0f;
    std::wstring visibleText;
    float caretOffset = 0.0f;
    float selectionOffset = 0.0f;
    float selectionWidth = 0.0f;
    bool hasSelection = false;
};

ComPtr<IDirect3DStateBlock9> g_StateBlock;
IDirect3DDevice9* g_Device = nullptr;
std::vector<RenderLine> g_Lines;
uint64_t g_MessageGeneration = (std::numeric_limits<uint64_t>::max)();
uint64_t g_SettingsGeneration = 0;
int g_CachedContentWidth = 0;
int g_FontHeight = 0;
int g_ScrollLines = 0;
float g_VisualScroll = 0.0f;
float g_VisualAlpha = -1.0f;
DWORD g_LastMessageTick = 0;
DWORD g_LastAnimationTick = 0;
bool g_SettingsInitialized = false;
bool g_DeviceLost = false;
std::atomic<bool> g_Ready{false};
InputRenderCache g_InputCache;

void MarkUnavailable() {
    HookManager::SetNativeChatSuppressed(false);
    g_Ready.store(false, std::memory_order_release);
}

struct ReadinessGuard {
    ~ReadinessGuard() {
        if (!committed) MarkUnavailable();
    }

    bool committed = false;
};

struct StateGuard {
    explicit StateGuard(IDirect3DStateBlock9* block) : block_(block) {
        captured_ = block_ && SUCCEEDED(block_->Capture());
    }
    ~StateGuard() {
        if (captured_) block_->Apply();
    }
    IDirect3DStateBlock9* block_ = nullptr;
    bool captured_ = false;
};

D3DCOLOR WithAlpha(D3DCOLOR color, int alpha) {
    const int original = static_cast<int>((color >> 24) & 0xFF);
    const int combined = std::clamp(original * alpha / 255, 0, 255);
    return (color & 0x00FFFFFF) | (static_cast<D3DCOLOR>(combined) << 24);
}

float MeasureWidth(const std::wstring& text) {
    return HUB::Chat::TextRenderer::MeasureWidth(text);
}

int HexValue(wchar_t character) {
    if (character >= L'0' && character <= L'9') return character - L'0';
    if (character >= L'a' && character <= L'f') return character - L'a' + 10;
    if (character >= L'A' && character <= L'F') return character - L'A' + 10;
    return -1;
}

bool ParseColorTag(const std::wstring& text, size_t position, D3DCOLOR& color, size_t& tagLength) {
    if (position >= text.size() || text[position] != L'{') return false;
    for (const size_t digitCount : {size_t{6}, size_t{8}}) {
        const size_t closing = position + digitCount + 1;
        if (closing >= text.size() || text[closing] != L'}') continue;
        uint32_t value = 0;
        bool valid = true;
        for (size_t index = 0; index < digitCount; ++index) {
            const int digit = HexValue(text[position + index + 1]);
            if (digit < 0) {
                valid = false;
                break;
            }
            value = (value << 4) | static_cast<uint32_t>(digit);
        }
        if (!valid) continue;
        color = digitCount == 6 ? 0xFF000000u | value : value;
        tagLength = digitCount + 2;
        return true;
    }
    return false;
}

ParsedText ParseStyledText(const std::wstring& source, D3DCOLOR defaultColor) {
    ParsedText parsed;
    parsed.text.reserve(source.size());
    parsed.colors.reserve(source.size());
    D3DCOLOR currentColor = defaultColor;
    for (size_t position = 0; position < source.size();) {
        size_t tagLength = 0;
        D3DCOLOR taggedColor = currentColor;
        if (ParseColorTag(source, position, taggedColor, tagLength)) {
            currentColor = taggedColor;
            position += tagLength;
            continue;
        }
        parsed.text.push_back(source[position]);
        parsed.colors.push_back(currentColor);
        ++position;
    }
    return parsed;
}

std::vector<TextSpan> BuildSpans(const ParsedText& parsed, size_t start, size_t length) {
    std::vector<TextSpan> spans;
    const size_t end = (std::min)(parsed.text.size(), start + length);
    for (size_t position = start; position < end;) {
        const D3DCOLOR color = parsed.colors[position];
        size_t spanEnd = position + 1;
        while (spanEnd < end && parsed.colors[spanEnd] == color) ++spanEnd;
        TextSpan span;
        span.text = parsed.text.substr(position, spanEnd - position);
        span.color = color;
        span.width = MeasureWidth(span.text);
        spans.push_back(std::move(span));
        position = spanEnd;
    }
    return spans;
}

size_t PreviousBoundary(const std::wstring& text, size_t position) {
    if (position == 0) return 0;
    --position;
    if (position > 0 && text[position] >= 0xDC00 && text[position] <= 0xDFFF &&
        text[position - 1] >= 0xD800 && text[position - 1] <= 0xDBFF) {
        --position;
    }
    return position;
}

size_t FindLineEnd(const std::wstring& text, size_t start, float maximumWidth) {
    if (start >= text.size()) return start;
    size_t low = start + 1;
    size_t high = text.size();
    size_t best = start;
    while (low <= high) {
        const size_t middle = low + (high - low) / 2;
        if (MeasureWidth(text.substr(start, middle - start)) <= maximumWidth) {
            best = middle;
            low = middle + 1;
        } else {
            if (middle == 0) break;
            high = middle - 1;
        }
    }
    if (best == start) {
        best = start + 1;
        if (text[start] >= 0xD800 && text[start] <= 0xDBFF && best < text.size() &&
            text[best] >= 0xDC00 && text[best] <= 0xDFFF) {
            ++best;
        }
    } else if (best < text.size() && text[best] >= 0xDC00 && text[best] <= 0xDFFF &&
        text[best - 1] >= 0xD800 && text[best - 1] <= 0xDBFF) {
        --best;
    }
    if (best < text.size()) {
        size_t wordBreak = best;
        while (wordBreak > start && !iswspace(text[wordBreak - 1])) wordBreak = PreviousBoundary(text, wordBreak);
        if (wordBreak > start) best = wordBreak;
    }
    return best;
}

std::wstring FormatTimestamp(const SYSTEMTIME& timestamp) {
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"[%02u:%02u] ", timestamp.wHour, timestamp.wMinute);
    return buffer;
}

void PushRenderLine(std::wstring timestamp, std::wstring prefix, std::vector<TextSpan> spans,
    D3DCOLOR prefixColor) {
    RenderLine line;
    line.timestamp = std::move(timestamp);
    line.prefix = std::move(prefix);
    line.spans = std::move(spans);
    line.prefixColor = prefixColor;
    line.timestampWidth = MeasureWidth(line.timestamp);
    line.prefixWidth = MeasureWidth(line.prefix);
    g_Lines.push_back(std::move(line));
}

struct RoleBadgeInfo {
    std::wstring text;
    std::string imagePath;
    D3DCOLOR color = 0xFFFFFFFF;
    bool isRainbow = false;
    uint32_t rainbowSpeedMs = 500;
};

std::vector<RoleBadgeInfo> GetPlayerRoles(int playerId) {
    std::vector<RoleBadgeInfo> roles;
    if (playerId < 0 || playerId >= kMaxPlayers) return roles;

    std::lock_guard<std::mutex> guard(g_PlayerDataMutex);
    const PlayerNametag& nametag = g_Players[playerId];
    for (const auto& slot : nametag.slots) {
        if (slot.active) {
            RoleBadgeInfo info;
            info.text = HUB::Chat::ChatManager::DecodeSampText(slot.text.c_str());
            info.imagePath = slot.imagePath;
            info.color = slot.color != 0 ? slot.color : D3DCOLOR_ARGB(255, 220, 220, 220);
            info.isRainbow = slot.isRainbow;
            info.rainbowSpeedMs = slot.rainbowSpeedMs;
            if (!info.text.empty() || !info.imagePath.empty()) {
                roles.push_back(std::move(info));
            }
        }
    }
    return roles;
}

std::vector<TextSpan> BuildPrefixSpans(const std::wstring& rawPrefix, int playerId, D3DCOLOR defaultColor) {
    std::vector<TextSpan> spans;
    if (rawPrefix.empty()) return spans;

    const auto roles = GetPlayerRoles(playerId);
    if (roles.empty()) {
        TextSpan span;
        span.text = rawPrefix + L" ";
        span.color = defaultColor;
        span.width = MeasureWidth(span.text);
        spans.push_back(std::move(span));
        return spans;
    }

    std::wstring name = rawPrefix;
    bool hasColon = false;
    if (!name.empty() && name.back() == L':') {
        hasColon = true;
        name.pop_back();
    }

    TextSpan nameSpan;
    nameSpan.text = name;
    nameSpan.color = defaultColor;
    nameSpan.width = MeasureWidth(nameSpan.text);
    spans.push_back(std::move(nameSpan));

    for (const auto& role : roles) {
        bool hasImage = !role.imagePath.empty() && TextureCache::HasImageResource(role.imagePath);
        if (hasImage) {
            TextSpan imgSpan;
            imgSpan.imagePath = role.imagePath;
            imgSpan.isImage = true;
            imgSpan.color = role.color;
            imgSpan.isRainbow = role.isRainbow;
            imgSpan.rainbowSpeedMs = role.rainbowSpeedMs;
            imgSpan.width = 22.0f;
            spans.push_back(std::move(imgSpan));
        } else if (!role.text.empty()) {
            TextSpan roleSpan;
            roleSpan.text = L" [" + role.text + L"]";
            roleSpan.color = role.color;
            roleSpan.isRainbow = role.isRainbow;
            roleSpan.rainbowSpeedMs = role.rainbowSpeedMs;
            roleSpan.width = MeasureWidth(roleSpan.text);
            spans.push_back(std::move(roleSpan));
        }
    }

    TextSpan colonSpan;
    colonSpan.text = hasColon ? L": " : L" ";
    colonSpan.color = defaultColor;
    colonSpan.width = MeasureWidth(colonSpan.text);
    spans.push_back(std::move(colonSpan));

    return spans;
}

void AppendWrappedMessage(const HUB::Chat::ChatMessage& message, int contentWidth,
    const HUB::Chat::ChatSettings& settings) {
    const std::wstring timestamp = settings.timestamps ? FormatTimestamp(message.timestamp) : std::wstring{};
    const std::vector<TextSpan> prefixSpans = BuildPrefixSpans(message.prefix, message.playerId, message.prefixColor);

    float prefixWidth = 0.0f;
    for (const auto& ps : prefixSpans) prefixWidth += ps.width;

    const float timestampWidth = MeasureWidth(timestamp);
    const float firstOverhead = timestampWidth + prefixWidth;
    const float continuationIndent = timestampWidth > 0.0f ? timestampWidth : 20.0f;
    const ParsedText parsed = ParseStyledText(message.text, message.textColor);
    size_t position = 0;
    bool firstLine = true;

    if (parsed.text.empty()) {
        PushRenderLine(timestamp, L"", prefixSpans, message.prefixColor);
        return;
    }

    while (position < parsed.text.size()) {
        while (position < parsed.text.size() && iswspace(parsed.text[position])) ++position;
        if (position >= parsed.text.size()) break;

        const float lineIndent = firstLine ? firstOverhead : continuationIndent;
        float available = static_cast<float>(contentWidth) - lineIndent;
        if (available < 40.0f && firstLine) {
            PushRenderLine(timestamp, L"", prefixSpans, message.prefixColor);
            firstLine = false;
            continue;
        }
        available = (std::max)(available, 40.0f);
        const size_t end = FindLineEnd(parsed.text, position, available);
        size_t visibleEnd = end;
        while (visibleEnd > position && iswspace(parsed.text[visibleEnd - 1])) --visibleEnd;

        std::vector<TextSpan> lineSpans;
        if (firstLine) {
            lineSpans = prefixSpans;
        } else if (continuationIndent > 0.0f) {
            TextSpan indentSpan;
            indentSpan.width = continuationIndent;
            lineSpans.push_back(std::move(indentSpan));
        }
        const std::vector<TextSpan> msgSpans = BuildSpans(parsed, position, visibleEnd - position);
        lineSpans.insert(lineSpans.end(), msgSpans.begin(), msgSpans.end());

        PushRenderLine(firstLine ? timestamp : L"", L"", lineSpans, message.prefixColor);
        position = end;
        firstLine = false;
    }
}

bool CreateResources(IDirect3DDevice9* device, const HUB::Chat::ChatSettings& settings) {
    if (!device) return false;
    const int requiredFontHeight = (std::max)(10, static_cast<int>(std::lround(settings.fontSize * settings.scale)));
    if (g_Device == device && g_StateBlock && g_FontHeight == requiredFontHeight && !g_DeviceLost &&
        HUB::Chat::TextRenderer::Initialize(device, requiredFontHeight)) return true;

    MarkUnavailable();
    g_DeviceLost = false;
    g_StateBlock.Reset();
    HUB::Chat::TextRenderer::Shutdown();

    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf()))) {
        Logger::Error("Failed to create the D3D9 state block for Custom Chat.");
        return false;
    }

    if (!HUB::Chat::TextRenderer::Initialize(device, requiredFontHeight)) {
        Logger::Error("Failed to initialize font resources for Custom Chat.");
        g_StateBlock.Reset();
        return false;
    }

    g_Device = device;
    g_FontHeight = requiredFontHeight;
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

void RebuildLinesIfNeeded(int contentWidth, const HUB::Chat::ChatSettings& settings) {
    HUB::Chat::ChatManager& manager = HUB::Chat::ChatManager::Get();
    const uint64_t generation = manager.Generation();
    if (generation == g_MessageGeneration && contentWidth == g_CachedContentWidth &&
        g_SettingsGeneration == HUB::Chat::GetSettingsStore().Generation()) {
        return;
    }

    const size_t oldLineCount = g_Lines.size();
    const HUB::Chat::MessageSnapshot snapshot = manager.Snapshot(1000);
    g_Lines.clear();
    g_Lines.reserve(snapshot.messages.size() * 2);
    for (const HUB::Chat::ChatMessage& message : snapshot.messages) {
        AppendWrappedMessage(message, contentWidth, settings);
    }

    if (g_ScrollLines > 0 && g_Lines.size() > oldLineCount) {
        const int addedLines = static_cast<int>(g_Lines.size() - oldLineCount);
        g_ScrollLines += addedLines;
        g_VisualScroll += static_cast<float>(addedLines);
    }
    if (generation != g_MessageGeneration) g_LastMessageTick = GetTickCount();
    g_MessageGeneration = snapshot.generation;
    g_CachedContentWidth = contentWidth;
    g_SettingsGeneration = HUB::Chat::GetSettingsStore().Generation();
}

void DrawText(const std::wstring& text, float x, float y, float right, float height, D3DCOLOR color,
    DWORD format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, bool shadow = true) {
    if (text.empty() || !g_Device) return;

    if (shadow) {
        const D3DCOLOR shadowColor = WithAlpha(0xFF000000, static_cast<int>(color >> 24));
        HUB::Chat::TextRenderer::Draw(text, x + 1.0f, y + 1.0f, right + 1.0f, height,
            shadowColor, format);
    }
    HUB::Chat::TextRenderer::Draw(text, x, y, right, height, color, format);
}

void DrawTexturedRect(IDirect3DDevice9* dev, IDirect3DTexture9* texture,
    float x, float y, float w, float h, D3DCOLOR color) {
    if (!dev || !texture) return;

    struct TexturedVertex2D {
        float x, y, z, rhw;
        D3DCOLOR color;
        float u, v;
    };
    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

    TexturedVertex2D vertices[4] = {
        { x,     y + h, 0.0f, 1.0f, color, 0.0f, 1.0f },
        { x,     y,     0.0f, 1.0f, color, 0.0f, 0.0f },
        { x + w, y + h, 0.0f, 1.0f, color, 1.0f, 1.0f },
        { x + w, y,     0.0f, 1.0f, color, 1.0f, 0.0f },
    };

    dev->SetTexture(0, texture);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev->SetFVF(kFvf);
    dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(TexturedVertex2D));
    dev->SetTexture(0, nullptr);

    // Restore standard D3D9 texture stage state
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
}

void DrawMessages(float x, float y, float width, float height, int alpha,
    const HUB::Chat::ChatSettings& settings) {
    const int lineHeight = (std::max)(14, static_cast<int>(std::lround(settings.lineHeight * settings.scale)));
    const int visibleCount = (std::max)(1, static_cast<int>(height) / lineHeight);
    const int maximumScroll = (std::max)(0, static_cast<int>(g_Lines.size()) - visibleCount);
    g_ScrollLines = std::clamp(g_ScrollLines, 0, maximumScroll);

    g_VisualScroll = std::clamp(g_VisualScroll, 0.0f, static_cast<float>(maximumScroll));
    const int baseScroll = static_cast<int>(std::floor(g_VisualScroll));
    const float scrollFraction = g_VisualScroll - baseScroll;
    const int end = (std::max)(0, static_cast<int>(g_Lines.size()) - baseScroll);
    const int extraLine = scrollFraction > 0.001f ? 1 : 0;
    const int start = (std::max)(0, end - visibleCount - extraLine);
    float lineY = y + height - static_cast<float>((end - start) * lineHeight) +
        scrollFraction * lineHeight;

    TextureCache::FlushPending(g_Device);

    for (int index = start; index < end; ++index) {
        const RenderLine& line = g_Lines[static_cast<size_t>(index)];
        float cursorX = x;
        if (!line.timestamp.empty()) {
            DrawText(line.timestamp, cursorX, lineY, x + width, static_cast<float>(lineHeight),
                WithAlpha(settings.timestampColor, alpha));
            cursorX += line.timestampWidth;
        }
        if (!line.prefix.empty()) {
            DrawText(line.prefix, cursorX, lineY, x + width, static_cast<float>(lineHeight),
                WithAlpha(line.prefixColor, alpha));
            cursorX += line.prefixWidth;
        }
        for (const TextSpan& span : line.spans) {
            D3DCOLOR drawColor = span.color;
            if (span.isRainbow) {
                const DWORD now = GetTickCount();
                const float speed = span.rainbowSpeedMs > 0 ? static_cast<float>(span.rainbowSpeedMs) : 500.0f;
                const float hue = std::fmod((now % static_cast<uint32_t>(speed)) / speed * 360.0f, 360.0f);
                drawColor = GetRainbowD3DColor(hue, static_cast<uint8_t>(alpha));
            } else {
                drawColor = WithAlpha(span.color, alpha);
            }

            if (span.isImage && !span.imagePath.empty()) {
                IDirect3DTexture9* texture = TextureCache::GetOrLoad(g_Device, span.imagePath);
                if (texture) {
                    float iconH = static_cast<float>(lineHeight - 2);
                    float iconW = iconH;
                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(texture->GetLevelDesc(0, &desc)) && desc.Height > 0) {
                        iconW = iconH * (static_cast<float>(desc.Width) / static_cast<float>(desc.Height));
                    }
                    DrawTexturedRect(g_Device, texture, cursorX + 2.0f, lineY + 1.0f, iconW, iconH, WithAlpha(0xFFFFFFFF, alpha));
                    cursorX += iconW + 4.0f;
                } else {
                    cursorX += span.width;
                }
            } else {
                DrawText(span.text, cursorX, lineY, x + width, static_cast<float>(lineHeight), drawColor);
                cursorX += span.width;
            }
        }
        lineY += static_cast<float>(lineHeight);
    }
}

void DrawScrollbar(float x, float y, float width, float height, int alpha,
    const HUB::Chat::ChatSettings& settings) {
    const int lineHeight = (std::max)(14, static_cast<int>(std::lround(settings.lineHeight * settings.scale)));
    const int visibleCount = (std::max)(1, static_cast<int>(height) / lineHeight);
    const int maximumScroll = (std::max)(0, static_cast<int>(g_Lines.size()) - visibleCount);
    if (maximumScroll == 0) return;

    const float thumbHeight = (std::max)(24.0f, height * visibleCount / static_cast<float>(g_Lines.size()));
    const float ratio = 1.0f - g_VisualScroll / static_cast<float>(maximumScroll);
    const float thumbY = y + (height - thumbHeight) * ratio;
    D3DHelper::DrawRoundedFilledRect(g_Device, x + width + 4.0f, thumbY, 6.0f,
        thumbHeight, 3.0f, WithAlpha(0xFF8E8E8E, alpha));
}

size_t FindInputViewStart(const std::wstring& text, size_t caret, float maximumWidth) {
    size_t start = 0;
    while (start < caret && MeasureWidth(text.substr(start, caret - start)) > maximumWidth) {
        size_t next = start + 1;
        if (text[start] >= 0xD800 && text[start] <= 0xDBFF && next < text.size() &&
            text[next] >= 0xDC00 && text[next] <= 0xDFFF) {
            ++next;
        }
        start = next;
    }
    return start;
}

void RebuildInputCache(float availableWidth) {
    const uint64_t generation = HUB::Chat::Input::Generation();
    if (g_InputCache.generation == generation &&
        std::fabs(g_InputCache.availableWidth - availableWidth) < 0.5f) {
        return;
    }

    const std::wstring& text = HUB::Chat::Input::Text();
    const size_t caret = (std::min)(HUB::Chat::Input::Caret(), text.size());
    const size_t viewStart = FindInputViewStart(text, caret, availableWidth - 8.0f);
    g_InputCache.visibleText = text.substr(viewStart);
    g_InputCache.caretOffset = MeasureWidth(text.substr(viewStart, caret - viewStart));
    g_InputCache.hasSelection = false;
    g_InputCache.selectionOffset = 0.0f;
    g_InputCache.selectionWidth = 0.0f;

    if (HUB::Chat::Input::HasSelection()) {
        const size_t rawStart = (std::min)(HUB::Chat::Input::SelectionStart(), text.size());
        const size_t rawEnd = (std::min)(HUB::Chat::Input::SelectionEnd(), text.size());
        const size_t selectionStart = (std::max)(viewStart, rawStart);
        const size_t selectionEnd = (std::max)(selectionStart, rawEnd);
        if (selectionStart < selectionEnd) {
            g_InputCache.selectionOffset = MeasureWidth(text.substr(viewStart, selectionStart - viewStart));
            g_InputCache.selectionWidth = MeasureWidth(text.substr(selectionStart, selectionEnd - selectionStart));
            g_InputCache.hasSelection = true;
        }
    }
    g_InputCache.generation = generation;
    g_InputCache.availableWidth = availableWidth;
}

void DrawComposer(float x, float y, float width, float height, int alpha,
    const HUB::Chat::ChatSettings& settings) {
    D3DHelper::DrawRoundedFilledRect(g_Device, x, y, width, height, 5.0f * settings.scale,
        WithAlpha(settings.composerColor, alpha));
    D3DHelper::DrawRoundedBorderRect(g_Device, x, y, width, height, 5.0f * settings.scale,
        1.0f, WithAlpha(settings.borderColor, alpha));

    const float padding = 10.0f * settings.scale;
    const float textX = x + padding;
    const float textRight = x + width - padding;
    const float availableWidth = textRight - textX;
    HUB::Chat::Input::UpdateImePosition(static_cast<LONG>(textX), static_cast<LONG>(y + height));
    const std::wstring& composition = HUB::Chat::Input::Composition();
    RebuildInputCache(availableWidth);

    if (g_InputCache.visibleText.empty() && composition.empty()) {
        DrawText(L"Nh\u1eadp tin nh\u1eafn...", textX, y, textRight, height,
            WithAlpha(settings.placeholderColor, alpha),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
        return;
    }

    if (g_InputCache.hasSelection) {
        D3DHelper::DrawFilledRect(g_Device, textX + g_InputCache.selectionOffset,
            y + 7.0f * settings.scale,
            (std::min)(g_InputCache.selectionWidth, availableWidth - g_InputCache.selectionOffset),
            height - 14.0f * settings.scale, WithAlpha(settings.selectionColor, alpha));
    }

    DrawText(g_InputCache.visibleText, textX, y, textRight, height, WithAlpha(settings.textColor, alpha),
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
    const float caretX = textX + g_InputCache.caretOffset;
    if (!composition.empty()) {
        DrawText(composition, caretX, y, textRight, height, WithAlpha(0xFF55AAFF, alpha),
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, false);
    }
    if (((GetTickCount() / 500) & 1) == 0) {
        D3DHelper::DrawFilledRect(g_Device, caretX, y + 8.0f * settings.scale,
            2.0f, height - 16.0f * settings.scale, WithAlpha(settings.textColor, alpha));
    }
}

} // namespace

namespace HUB::Chat::CustomChat {

void Render(IDirect3DDevice9* device) {
    ReadinessGuard readiness;
    if (!HookManager::IsInstalled() || !device || device->TestCooperativeLevel() != D3D_OK) return;
    HookManager::SyncSampChat();
    if (!g_SettingsInitialized) {
        HUB::Chat::GetSettingsStore().Initialize();
        g_SettingsInitialized = true;
        g_LastMessageTick = GetTickCount();
        g_LastAnimationTick = g_LastMessageTick;
    }
    HUB::Chat::GetSettingsStore().ReloadIfChanged();
    const ChatSettings& settings = HUB::Chat::GetSettingsStore().Get();
    if (!CreateResources(device, settings)) return;

    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (FAILED(device->GetCreationParameters(&creation)) || !Input::Install(creation.hFocusWindow)) {
        return;
    }

    D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetViewport(&viewport))) return;

    const float scale = settings.scale;
    const float width = (std::min)(settings.width * scale, static_cast<float>(viewport.Width) - 20.0f);
    const float height = (std::min)(settings.height * scale, static_cast<float>(viewport.Height) - 20.0f);
    const float x = (std::max)(0.0f, settings.marginLeft * scale);
    const float y = std::clamp(settings.marginTop * scale, 0.0f,
        (std::max)(0.0f, static_cast<float>(viewport.Height) - height));
    const float padding = settings.padding * scale;
    const float composerHeight = settings.composerHeight * scale;
    const float messageHeight = height - composerHeight - padding * 2.0f;
    const float contentWidth = width - padding * 2.0f - 10.0f;
    RebuildLinesIfNeeded(static_cast<int>(contentWidth), settings);

    const DWORD now = GetTickCount();
    const float deltaSeconds = (std::min)(0.1f, (now - g_LastAnimationTick) / 1000.0f);
    g_LastAnimationTick = now;
    const DWORD activity = (std::max)(g_LastMessageTick, Input::LastActivityTick());
    const bool idle = !Input::IsOpen() && settings.fadeEnabled &&
        now - activity >= static_cast<DWORD>(settings.fadeDelayMs);
    const float targetAlpha = static_cast<float>(idle ? settings.idleOpacity : settings.opacity);
    if (g_VisualAlpha < 0.0f) g_VisualAlpha = targetAlpha;
    const float fadeBlend = (std::min)(1.0f, deltaSeconds / 0.2f);
    g_VisualAlpha += (targetAlpha - g_VisualAlpha) * fadeBlend;
    const int alpha = static_cast<int>(std::lround(g_VisualAlpha));

    const int lineHeight = (std::max)(14,
        static_cast<int>(std::lround(settings.lineHeight * settings.scale)));
    const int visibleLines = (std::max)(1, static_cast<int>(messageHeight) / lineHeight);
    const int maximumScroll = (std::max)(0, static_cast<int>(g_Lines.size()) - visibleLines);
    g_ScrollLines = std::clamp(g_ScrollLines, 0, maximumScroll);
    if (settings.smoothScroll) {
        const float scrollBlend = (std::min)(1.0f, deltaSeconds * 12.0f);
        g_VisualScroll += (g_ScrollLines - g_VisualScroll) * scrollBlend;
    } else {
        g_VisualScroll = static_cast<float>(g_ScrollLines);
    }

    if (!g_StateBlock && FAILED(device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf()))) return;
    StateGuard guard(g_StateBlock.Get());
    SetupRenderState(device);

    DrawMessages(x + padding, y + padding, contentWidth, messageHeight, alpha, settings);
    DrawScrollbar(x + padding, y + padding, contentWidth, messageHeight, alpha, settings);

    if (Input::IsOpen()) {
        const float composerY = y + height - composerHeight;
        DrawComposer(x, composerY, width, composerHeight, settings.opacity, settings);
    }

    SettingsPanel::Render(device);

    if (!HookManager::SetNativeChatSuppressed(true)) return;
    g_Ready.store(true, std::memory_order_release);
    readiness.committed = true;
}

void OnLostDevice() {
    MarkUnavailable();
    if (g_DeviceLost) return;
    g_DeviceLost = true;
    g_StateBlock.Reset();
    HUB::Chat::TextRenderer::OnLostDevice();
}

void OnResetDevice(IDirect3DDevice9* device) {
    g_Device = device;
    if (device) device->CreateStateBlock(D3DSBT_ALL, g_StateBlock.GetAddressOf());
    g_DeviceLost = false;
    g_MessageGeneration = (std::numeric_limits<uint64_t>::max)();
}

void Shutdown() {
    MarkUnavailable();
    Input::Uninstall();
    g_StateBlock.Reset();
    HUB::Chat::TextRenderer::Shutdown();
    g_Device = nullptr;
    g_DeviceLost = false;
    g_Lines.clear();
    g_InputCache = {};
    g_VisualScroll = 0.0f;
    g_VisualAlpha = -1.0f;
}

bool IsReady() {
    return g_Ready.load(std::memory_order_acquire);
}

void OpenInput() {
    if (IsReady()) Input::Open();
}

void CloseInput() {
    if (Input::IsOpen()) Input::Close();
}

void SubmitInput() {
    if (!Input::IsOpen()) return;
    std::wstring text = Input::Text();

    const size_t firstNotSpace = text.find_first_not_of(L" \t\r\n");
    if (firstNotSpace == std::wstring::npos) {
        RequestCloseInput();
        return;
    }
    const size_t lastNotSpace = text.find_last_not_of(L" \t\r\n");
    std::wstring trimmed = text.substr(firstNotSpace, lastNotSpace - firstNotSpace + 1);

    if (_wcsicmp(trimmed.c_str(), L"/settings") == 0 || _wcsicmp(trimmed.c_str(), L"/setting") == 0) {
        Input::AddHistory(text);
        Input::ClearAfterSubmit();
        RequestCloseInput();
        SettingsPanel::Open();
        return;
    }

    const std::string fullText = ChatManager::EncodeUtf8(text, (std::numeric_limits<size_t>::max)());
    const std::string encoded = fullText.size() <= 128 ? fullText : ChatManager::EncodeUtf8(text, 128);
    if (encoded.empty()) {
        Logger::Error("Custom Chat input could not be encoded as UTF-8.");
        return;
    }
    if (fullText.size() > encoded.size()) {
        Logger::Log(Logger::Category::Warn,
            "Custom Chat input exceeded 128 bytes and was truncated on a UTF-8 boundary.");
    }
    Input::AddHistory(text);
    Input::ClearAfterSubmit();
    HookManager::CloseChatInput();
    if (!HookManager::SendChatText(encoded.c_str())) {
        Logger::Error("Custom Chat failed to dispatch chat input.");
        return;
    }
}

void RequestCloseInput() {
    if (!Input::IsOpen()) return;
    HookManager::CloseChatInput();
}

void Scroll(short wheelDelta) {
    if (wheelDelta == 0) return;
    const int direction = wheelDelta > 0 ? 1 : -1;
    g_ScrollLines = (std::max)(0, g_ScrollLines + direction * HUB::Chat::GetSettingsStore().Get().wheelLines);
}

} // namespace HUB::Chat::CustomChat
