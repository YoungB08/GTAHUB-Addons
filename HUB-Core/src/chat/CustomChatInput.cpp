#include "pch.h"
#include "CustomChatInput.h"

#include "CustomChat.h"
#include "Logger.h"
#include "SettingsPanel.h"

#include <imm.h>
#include <sampapi/0.3.DL-1/CGame.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <deque>
#include <limits>
#include <string>

#pragma comment(lib, "imm32.lib")

namespace {

constexpr size_t kNoSelection = (std::numeric_limits<size_t>::max)();
constexpr size_t kMaxHistory = 50;
constexpr size_t kMaxCodeUnits = 512;
constexpr UINT kVietnameseCodePage = 1258;

HWND g_Window = nullptr;
WNDPROC g_OriginalWndProc = nullptr;
HWND g_InputWindow = nullptr;
WNDPROC g_OriginalInputWndProc = nullptr;
std::wstring g_Text;
std::wstring g_Composition;
size_t g_Caret = 0;
size_t g_SelectionAnchor = kNoSelection;
std::deque<std::wstring> g_History;
size_t g_HistoryIndex = 0;
std::wstring g_HistoryDraft;
DWORD g_LastActivityTick = 0;
uint64_t g_Generation = 0;
POINT g_ImePosition{-1, -1};
bool g_Open = false;
bool g_ControlsLocked = false;
bool g_WindowUsesUnicodeMessages = false;
bool g_SyncingInputWindow = false;
DWORD g_LastSubmitKeyTick = 0;

constexpr DWORD kSubmitKeySuppressionMs = 500;

void SyncInputWindowFromModel();

bool ConsumeRecentSubmitKey(UINT message, WPARAM wParam) {
    if (GetTickCount() - g_LastSubmitKeyTick >= kSubmitKeySuppressionMs) return false;
    if (wParam != VK_RETURN && !(message == WM_CHAR && wParam == L'\r')) return false;
    return message == WM_KEYDOWN || message == WM_SYSKEYDOWN ||
        message == WM_KEYUP || message == WM_SYSKEYUP || message == WM_CHAR;
}

void LockGameControls() {
    sampapi::v03dl::CGame* game = GetRefGame();
    if (!game || g_ControlsLocked) return;
    game->SetCursorMode(sampapi::v03dl::CURSOR_LOCKKEYS_NOCURSOR, FALSE);
    g_ControlsLocked = true;
}

void UnlockGameControls() {
    sampapi::v03dl::CGame* game = GetRefGame();
    if (game && g_ControlsLocked) game->SetCursorMode(sampapi::v03dl::CURSOR_NONE, TRUE);
    g_ControlsLocked = false;
}

bool IsHighSurrogate(wchar_t value) {
    return value >= 0xD800 && value <= 0xDBFF;
}

bool IsLowSurrogate(wchar_t value) {
    return value >= 0xDC00 && value <= 0xDFFF;
}

size_t PreviousBoundary(size_t position) {
    if (position == 0) return 0;
    --position;
    if (position > 0 && IsLowSurrogate(g_Text[position]) && IsHighSurrogate(g_Text[position - 1])) --position;
    return position;
}

size_t NextBoundary(size_t position) {
    if (position >= g_Text.size()) return g_Text.size();
    if (IsHighSurrogate(g_Text[position]) && position + 1 < g_Text.size() && IsLowSurrogate(g_Text[position + 1])) {
        return position + 2;
    }
    return position + 1;
}

void MarkActivity() {
    g_LastActivityTick = GetTickCount();
    ++g_Generation;
}

void ClearSelection() {
    g_SelectionAnchor = kNoSelection;
}

bool HasSelectionInternal() {
    return g_SelectionAnchor != kNoSelection && g_SelectionAnchor != g_Caret;
}

size_t SelectionStartInternal() {
    return HasSelectionInternal() ? (std::min)(g_SelectionAnchor, g_Caret) : g_Caret;
}

size_t SelectionEndInternal() {
    return HasSelectionInternal() ? (std::max)(g_SelectionAnchor, g_Caret) : g_Caret;
}

void BeginSelection(bool selecting) {
    if (selecting) {
        if (g_SelectionAnchor == kNoSelection) g_SelectionAnchor = g_Caret;
    } else {
        ClearSelection();
    }
}

void DeleteSelection() {
    if (!HasSelectionInternal()) return;
    const size_t start = SelectionStartInternal();
    g_Text.erase(start, SelectionEndInternal() - start);
    g_Caret = start;
    ClearSelection();
}

void InsertText(const std::wstring& value) {
    if (value.empty()) return;
    DeleteSelection();

    std::wstring sanitized;
    sanitized.reserve(value.size());
    for (wchar_t character : value) {
        if (character != L'\r' && character != L'\n' && character != L'\0') sanitized.push_back(character);
    }
    if (sanitized.empty()) return;
    if (g_Text.size() + sanitized.size() > kMaxCodeUnits) {
        sanitized.resize(kMaxCodeUnits - g_Text.size());
        if (!sanitized.empty() && IsHighSurrogate(sanitized.back())) sanitized.pop_back();
    }
    g_Text.insert(g_Caret, sanitized);
    g_Caret += sanitized.size();
    MarkActivity();
}

std::wstring NormalizeNfc(const wchar_t* text, size_t length) {
    if (!text || length == 0 || length > static_cast<size_t>((std::numeric_limits<int>::max)())) return {};

    const int sourceLength = static_cast<int>(length);
    const int normalizedLength = NormalizeString(NormalizationC, text, sourceLength, nullptr, 0);
    if (normalizedLength <= 0) return {};

    std::wstring result(static_cast<size_t>(normalizedLength), L'\0');
    const int written = NormalizeString(NormalizationC, text, sourceLength,
        result.data(), normalizedLength);
    if (written <= 0) return {};
    result.resize(static_cast<size_t>(written));
    return result;
}

size_t NormalizedPrefixLength(size_t position) {
    position = (std::min)(position, g_Text.size());
    if (position == 0) return 0;
    const std::wstring normalized = NormalizeNfc(g_Text.data(), position);
    return normalized.empty() ? position : normalized.size();
}

void NormalizeInputBuffer() {
    if (g_Text.empty()) return;

    const size_t normalizedCaret = NormalizedPrefixLength(g_Caret);
    const size_t normalizedAnchor = g_SelectionAnchor == kNoSelection
        ? kNoSelection
        : NormalizedPrefixLength(g_SelectionAnchor);
    std::wstring normalized = NormalizeNfc(g_Text.data(), g_Text.size());
    if (normalized.empty()) return;

    g_Text = std::move(normalized);
    g_Caret = (std::min)(normalizedCaret, g_Text.size());
    if (normalizedAnchor != kNoSelection) {
        g_SelectionAnchor = (std::min)(normalizedAnchor, g_Text.size());
    }
}

void InsertAnsiCharacter(WPARAM value) {
    if (value > 0xFF) {
        InsertText(std::wstring(1, static_cast<wchar_t>(value)));
        return;
    }

    const char encoded = static_cast<char>(static_cast<unsigned char>(value));
    wchar_t decoded[2]{};
    const int count = MultiByteToWideChar(kVietnameseCodePage, 0,
        &encoded, 1, decoded, static_cast<int>(std::size(decoded)));
    if (count > 0) {
        InsertText(std::wstring(decoded, static_cast<size_t>(count)));
        NormalizeInputBuffer();
    }
}

LRESULT CallOriginalWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_OriginalWndProc) {
        return g_WindowUsesUnicodeMessages
            ? DefWindowProcW(window, message, wParam, lParam)
            : DefWindowProcA(window, message, wParam, lParam);
    }
    return g_WindowUsesUnicodeMessages
        ? CallWindowProcW(g_OriginalWndProc, window, message, wParam, lParam)
        : CallWindowProcA(g_OriginalWndProc, window, message, wParam, lParam);
}

void SyncModelFromInputWindow() {
    if (!g_InputWindow || g_SyncingInputWindow) return;

    const int length = GetWindowTextLengthW(g_InputWindow);
    if (length < 0 || length > static_cast<int>(kMaxCodeUnits)) return;

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(g_InputWindow, text.data(), length + 1);
    if (copied < 0) return;
    text.resize(static_cast<size_t>(copied));

    DWORD selectionStart = 0;
    DWORD selectionEnd = 0;
    SendMessageW(g_InputWindow, EM_GETSEL,
        reinterpret_cast<WPARAM>(&selectionStart), reinterpret_cast<LPARAM>(&selectionEnd));
    const size_t caret = (std::min)(static_cast<size_t>(selectionEnd), text.size());
    const size_t anchor = selectionStart == selectionEnd
        ? kNoSelection
        : (std::min)(static_cast<size_t>(selectionStart), text.size());

    if (g_Text == text && g_Caret == caret && g_SelectionAnchor == anchor) return;
    g_Text = std::move(text);
    g_Caret = caret;
    g_SelectionAnchor = anchor;
    MarkActivity();
}

void SyncInputWindowFromModel() {
    if (!g_InputWindow || g_SyncingInputWindow) return;

    g_SyncingInputWindow = true;
    SetWindowTextW(g_InputWindow, g_Text.c_str());
    const size_t selectionStart = HasSelectionInternal() ? SelectionStartInternal() : g_Caret;
    const size_t selectionEnd = HasSelectionInternal() ? SelectionEndInternal() : g_Caret;
    SendMessageW(g_InputWindow, EM_SETSEL,
        static_cast<WPARAM>(selectionStart), static_cast<LPARAM>(selectionEnd));
    g_SyncingInputWindow = false;
}

LRESULT CallOriginalInputWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return g_OriginalInputWndProc
        ? CallWindowProcW(g_OriginalInputWndProc, window, message, wParam, lParam)
        : DefWindowProcW(window, message, wParam, lParam);
}

bool IsWordSpace(wchar_t character) {
    return iswspace(character) != 0;
}

size_t PreviousWordBoundary(size_t position) {
    while (position > 0 && IsWordSpace(g_Text[PreviousBoundary(position)])) position = PreviousBoundary(position);
    while (position > 0 && !IsWordSpace(g_Text[PreviousBoundary(position)])) position = PreviousBoundary(position);
    return position;
}

size_t NextWordBoundary(size_t position) {
    while (position < g_Text.size() && IsWordSpace(g_Text[position])) position = NextBoundary(position);
    while (position < g_Text.size() && !IsWordSpace(g_Text[position])) position = NextBoundary(position);
    return position;
}

void MoveCaret(size_t position, bool selecting) {
    BeginSelection(selecting);
    g_Caret = (std::min)(position, g_Text.size());
    MarkActivity();
}

void CopySelection() {
    if (!HasSelectionInternal() || !OpenClipboard(g_Window)) return;

    const std::wstring value = g_Text.substr(SelectionStartInternal(), SelectionEndInternal() - SelectionStartInternal());
    const SIZE_T bytes = (value.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory) {
        void* destination = GlobalLock(memory);
        if (destination) {
            memcpy(destination, value.c_str(), bytes);
            GlobalUnlock(memory);
            EmptyClipboard();
            if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

void PasteClipboard() {
    if (!OpenClipboard(g_Window)) return;
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (handle) {
        const wchar_t* value = static_cast<const wchar_t*>(GlobalLock(handle));
        if (value) {
            InsertText(value);
            GlobalUnlock(handle);
        }
    }
    CloseClipboard();
}

void SelectAll() {
    g_SelectionAnchor = 0;
    g_Caret = g_Text.size();
    MarkActivity();
}

void RecallHistory(int direction) {
    if (g_History.empty()) return;
    if (g_HistoryIndex == g_History.size()) g_HistoryDraft = g_Text;

    if (direction < 0 && g_HistoryIndex > 0) {
        --g_HistoryIndex;
        g_Text = g_History[g_HistoryIndex];
    } else if (direction > 0 && g_HistoryIndex < g_History.size()) {
        ++g_HistoryIndex;
        g_Text = g_HistoryIndex < g_History.size() ? g_History[g_HistoryIndex] : g_HistoryDraft;
    }
    g_Caret = g_Text.size();
    ClearSelection();
    MarkActivity();
    SyncInputWindowFromModel();
}

void ReadImeString(LPARAM lParam) {
    HIMC context = ImmGetContext(g_Window);
    if (!context) return;

    if ((lParam & GCS_RESULTSTR) != 0) {
        const LONG bytes = ImmGetCompositionStringW(context, GCS_RESULTSTR, nullptr, 0);
        if (bytes > 0) {
            std::wstring result(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
            ImmGetCompositionStringW(context, GCS_RESULTSTR, result.data(), bytes);
            InsertText(result);
        }
        g_Composition.clear();
    } else if ((lParam & GCS_COMPSTR) != 0) {
        const LONG bytes = ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
        if (bytes > 0) {
            g_Composition.assign(static_cast<size_t>(bytes / sizeof(wchar_t)), L'\0');
            ImmGetCompositionStringW(context, GCS_COMPSTR, g_Composition.data(), bytes);
        } else {
            g_Composition.clear();
        }
    }
    ImmReleaseContext(g_Window, context);
    MarkActivity();
}

bool HandleKeyDown(WPARAM key) {
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (control) {
        switch (key) {
            case 'A': SelectAll(); return true;
            case 'C': CopySelection(); return true;
            case 'V': PasteClipboard(); return true;
            case 'X': CopySelection(); DeleteSelection(); MarkActivity(); return true;
            default: break;
        }
    }
    if (shift && key == VK_INSERT) {
        PasteClipboard();
        return true;
    }

    switch (key) {
        case VK_RETURN:
            HUB::Chat::CustomChat::SubmitInput();
            return true;
        case VK_ESCAPE:
            HUB::Chat::CustomChat::RequestCloseInput();
            return true;
        case VK_LEFT:
            MoveCaret(control ? PreviousWordBoundary(g_Caret) : PreviousBoundary(g_Caret), shift);
            return true;
        case VK_RIGHT:
            MoveCaret(control ? NextWordBoundary(g_Caret) : NextBoundary(g_Caret), shift);
            return true;
        case VK_HOME:
            MoveCaret(0, shift);
            return true;
        case VK_END:
            MoveCaret(g_Text.size(), shift);
            return true;
        case VK_UP:
            RecallHistory(-1);
            return true;
        case VK_DOWN:
            RecallHistory(1);
            return true;
        case VK_BACK:
            if (HasSelectionInternal()) {
                DeleteSelection();
            } else {
                const size_t start = control ? PreviousWordBoundary(g_Caret) : PreviousBoundary(g_Caret);
                g_Text.erase(start, g_Caret - start);
                g_Caret = start;
            }
            MarkActivity();
            return true;
        case VK_DELETE:
            if (HasSelectionInternal()) {
                DeleteSelection();
            } else {
                const size_t end = control ? NextWordBoundary(g_Caret) : NextBoundary(g_Caret);
                g_Text.erase(g_Caret, end - g_Caret);
            }
            MarkActivity();
            return true;
        default:
            return false;
    }
}

LRESULT CALLBACK InputWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ConsumeRecentSubmitKey(message, wParam)) return 0;
    if (!g_Open) return CallOriginalInputWindowProc(window, message, wParam, lParam);

    switch (message) {
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_RETURN:
                    g_LastSubmitKeyTick = GetTickCount();
                    HUB::Chat::CustomChat::SubmitInput();
                    return 0;
                case VK_ESCAPE:
                    HUB::Chat::CustomChat::RequestCloseInput();
                    return 0;
                case VK_UP:
                    RecallHistory(-1);
                    return 0;
                case VK_DOWN:
                    RecallHistory(1);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_MOUSEWHEEL:
            HUB::Chat::CustomChat::Scroll(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        case WM_IME_STARTCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            break;
        case WM_IME_ENDCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            break;
        default:
            break;
    }

    const LRESULT result = CallOriginalInputWindowProc(window, message, wParam, lParam);
    switch (message) {
        case WM_CHAR:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_PASTE:
        case WM_CUT:
        case WM_CLEAR:
        case WM_UNDO:
        case WM_SETTEXT:
        case WM_IME_COMPOSITION:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            SyncModelFromInputWindow();
            break;
        default:
            break;
    }
    return result;
}

bool CreateInputWindow(HWND parent) {
    g_InputWindow = CreateWindowExW(WS_EX_TRANSPARENT, L"EDIT", L"",
        WS_CHILD | ES_LEFT | ES_AUTOHSCROLL,
        -32000, -32000, 1, 1, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_InputWindow) {
        Logger::Error("Custom Chat failed to create Unicode input window. error=%lu", GetLastError());
        return false;
    }

    SendMessageW(g_InputWindow, EM_SETLIMITTEXT, static_cast<WPARAM>(kMaxCodeUnits), 0);
    SetLastError(0);
    const LONG_PTR original = SetWindowLongPtrW(g_InputWindow, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&InputWindowProc));
    if (original == 0 && GetLastError() != 0) {
        Logger::Error("Custom Chat failed to subclass Unicode input window. error=%lu", GetLastError());
        DestroyWindow(g_InputWindow);
        g_InputWindow = nullptr;
        return false;
    }
    g_OriginalInputWndProc = reinterpret_cast<WNDPROC>(original);
    return true;
}

void DestroyInputWindow() {
    if (g_InputWindow && IsWindow(g_InputWindow)) {
        if (g_OriginalInputWndProc) {
            const WNDPROC current = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(g_InputWindow, GWLP_WNDPROC));
            if (current == &InputWindowProc) {
                SetWindowLongPtrW(g_InputWindow, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(g_OriginalInputWndProc));
            }
        }
        DestroyWindow(g_InputWindow);
    }
    g_InputWindow = nullptr;
    g_OriginalInputWndProc = nullptr;
    g_SyncingInputWindow = false;
}

LRESULT CALLBACK CustomWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_OriginalWndProc) return CallOriginalWindowProc(window, message, wParam, lParam);
    if (ConsumeRecentSubmitKey(message, wParam)) return 0;

    if (HUB::Chat::SettingsPanel::IsOpen()) {
        if (HUB::Chat::SettingsPanel::HandleMessage(window, message, wParam, lParam)) {
            return 0;
        }
    }

    if (!g_Open) return CallOriginalWindowProc(window, message, wParam, lParam);

    if (message == WM_COMMAND && reinterpret_cast<HWND>(lParam) == g_InputWindow) {
        if (HIWORD(wParam) == EN_UPDATE || HIWORD(wParam) == EN_CHANGE) {
            SyncModelFromInputWindow();
        }
        return 0;
    }

    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam == VK_RETURN) {
                g_LastSubmitKeyTick = GetTickCount();
                HUB::Chat::CustomChat::SubmitInput();
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                HUB::Chat::CustomChat::RequestCloseInput();
                return 0;
            }
            if (HandleKeyDown(wParam)) return 0;
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
            if (wParam >= 0x20 && wParam != 0x7F && (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
                if (g_WindowUsesUnicodeMessages || wParam > 0xFF) {
                    InsertText(std::wstring(1, static_cast<wchar_t>(wParam)));
                } else {
                    InsertAnsiCharacter(wParam);
                }
                SyncInputWindowFromModel();
            }
            return 0;

        case WM_UNICHAR:
            if (wParam == UNICODE_NOCHAR) return TRUE;
            if (wParam <= 0xFFFF) {
                InsertText(std::wstring(1, static_cast<wchar_t>(wParam)));
            } else if (wParam <= 0x10FFFF) {
                const unsigned value = static_cast<unsigned>(wParam) - 0x10000;
                std::wstring pair;
                pair.push_back(static_cast<wchar_t>(0xD800 + (value >> 10)));
                pair.push_back(static_cast<wchar_t>(0xDC00 + (value & 0x3FF)));
                InsertText(pair);
            }
            SyncInputWindowFromModel();
            return 0;

        case WM_IME_STARTCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            return 0;

        case WM_IME_COMPOSITION:
            ReadImeString(lParam);
            SyncInputWindowFromModel();
            return 0;

        case WM_IME_ENDCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            return 0;

        case WM_IME_CHAR:
            if (wParam != 0) {
                InsertText(std::wstring(1, static_cast<wchar_t>(wParam)));
                SyncInputWindowFromModel();
            }
            return 0;

        case WM_MOUSEWHEEL:
            HUB::Chat::CustomChat::Scroll(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;

        default:
            break;
    }
    return CallOriginalWindowProc(window, message, wParam, lParam);
}

} // namespace

namespace HUB::Chat::Input {

bool Install(HWND window) {
    if (!window) return false;
    if (g_Window == window && g_OriginalWndProc && g_InputWindow) return true;
    Uninstall();

    g_WindowUsesUnicodeMessages = IsWindowUnicode(window) != FALSE;
    SetLastError(0);
    const LONG_PTR original = g_WindowUsesUnicodeMessages
        ? SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&CustomWndProc))
        : SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&CustomWndProc));
    if (original == 0 && GetLastError() != 0) {
        Logger::Error("Custom Chat failed to subclass game WndProc. error=%lu", GetLastError());
        g_WindowUsesUnicodeMessages = false;
        return false;
    }
    g_Window = window;
    g_OriginalWndProc = reinterpret_cast<WNDPROC>(original);
    if (!CreateInputWindow(window)) {
        Uninstall();
        return false;
    }
    Logger::Input("Custom Chat WndProc installed. hwnd=%p mode=%s input=%p mode=UTF-16",
        window, g_WindowUsesUnicodeMessages ? "UTF-16" : "Windows-1258", g_InputWindow);
    return true;
}

void Uninstall() {
    UnlockGameControls();
    DestroyInputWindow();
    if (g_Window && g_OriginalWndProc) {
        const WNDPROC current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(g_Window, GWLP_WNDPROC));
        if (current == &CustomWndProc) {
            if (g_WindowUsesUnicodeMessages) {
                SetWindowLongPtrW(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_OriginalWndProc));
            } else {
                SetWindowLongPtrA(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_OriginalWndProc));
            }
        }
    }
    g_Window = nullptr;
    g_OriginalWndProc = nullptr;
    g_WindowUsesUnicodeMessages = false;
    g_Open = false;
    g_Composition.clear();
}

void Open() {
    LockGameControls();
    g_Open = true;
    g_Text.clear();
    g_Composition.clear();
    g_Caret = 0;
    ClearSelection();
    g_HistoryIndex = g_History.size();
    g_HistoryDraft.clear();
    SyncInputWindowFromModel();
    if (g_InputWindow) {
        ShowWindow(g_InputWindow, SW_SHOWNA);
        SetFocus(g_InputWindow);
    }
    MarkActivity();
    Logger::Input("Custom Chat input opened.");
}

void Close() {
    g_Open = false;
    if (g_InputWindow) ShowWindow(g_InputWindow, SW_HIDE);
    if (g_Window) SetFocus(g_Window);
    UnlockGameControls();
    g_Composition.clear();
    ClearSelection();
    MarkActivity();
    Logger::Input("Custom Chat input closed.");
}

bool IsOpen() { return g_Open; }
const std::wstring& Text() { return g_Text; }
const std::wstring& Composition() { return g_Composition; }
size_t Caret() { return g_Caret; }
bool HasSelection() { return HasSelectionInternal(); }
size_t SelectionStart() { return SelectionStartInternal(); }
size_t SelectionEnd() { return SelectionEndInternal(); }
DWORD LastActivityTick() { return g_LastActivityTick; }
uint64_t Generation() { return g_Generation; }

void UpdateImePosition(LONG x, LONG y) {
    if (!g_Open || !g_Window || (g_ImePosition.x == x && g_ImePosition.y == y)) return;
    HWND inputWindow = g_InputWindow ? g_InputWindow : g_Window;
    HIMC context = ImmGetContext(inputWindow);
    if (!context) return;

    POINT position{x, y};
    ClientToScreen(g_Window, &position);
    ScreenToClient(inputWindow, &position);

    COMPOSITIONFORM compositionForm{};
    compositionForm.dwStyle = CFS_POINT;
    compositionForm.ptCurrentPos = position;
    ImmSetCompositionWindow(context, &compositionForm);

    CANDIDATEFORM candidateForm{};
    candidateForm.dwIndex = 0;
    candidateForm.dwStyle = CFS_CANDIDATEPOS;
    candidateForm.ptCurrentPos = position;
    ImmSetCandidateWindow(context, &candidateForm);
    ImmReleaseContext(inputWindow, context);
    g_ImePosition = {x, y};
}

void ClearAfterSubmit() {
    g_Text.clear();
    g_Composition.clear();
    g_Caret = 0;
    ClearSelection();
    SyncInputWindowFromModel();
    MarkActivity();
}

void AddHistory(const std::wstring& text) {
    if (text.empty()) return;
    if (g_History.empty() || g_History.back() != text) g_History.push_back(text);
    if (g_History.size() > kMaxHistory) g_History.pop_front();
    g_HistoryIndex = g_History.size();
}

} // namespace HUB::Chat::Input
