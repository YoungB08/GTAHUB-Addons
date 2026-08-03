#include "pch.h"
#include "CustomChatInput.h"

#include "CustomChat.h"
#include "Logger.h"

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

HWND g_Window = nullptr;
WNDPROC g_OriginalWndProc = nullptr;
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

LRESULT CALLBACK CustomWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (!g_OriginalWndProc) return DefWindowProcW(window, message, wParam, lParam);
    if (!g_Open) return CallWindowProcW(g_OriginalWndProc, window, message, wParam, lParam);

    switch (message) {
        case WM_KEYDOWN:
            if (HandleKeyDown(wParam)) return 0;
            break;
        case WM_CHAR:
            if (wParam >= 0x20 && wParam != 0x7F && (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
                InsertText(std::wstring(1, static_cast<wchar_t>(wParam)));
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
            return 0;
        case WM_IME_STARTCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            return 0;
        case WM_IME_COMPOSITION:
            ReadImeString(lParam);
            return 0;
        case WM_IME_ENDCOMPOSITION:
            g_Composition.clear();
            MarkActivity();
            return 0;
        case WM_IME_CHAR:
            return 0;
        case WM_MOUSEWHEEL:
            HUB::Chat::CustomChat::Scroll(GET_WHEEL_DELTA_WPARAM(wParam));
            return 0;
        default:
            break;
    }
    return CallWindowProcW(g_OriginalWndProc, window, message, wParam, lParam);
}

} // namespace

namespace HUB::Chat::Input {

bool Install(HWND window) {
    if (!window) return false;
    if (g_Window == window && g_OriginalWndProc) return true;
    Uninstall();

    SetLastError(0);
    const LONG_PTR original = SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&CustomWndProc));
    if (original == 0 && GetLastError() != 0) {
        Logger::Error("Custom Chat failed to subclass game WndProc. error=%lu", GetLastError());
        return false;
    }
    g_Window = window;
    g_OriginalWndProc = reinterpret_cast<WNDPROC>(original);
    Logger::Input("Custom Chat WndProc installed. hwnd=%p", window);
    return true;
}

void Uninstall() {
    UnlockGameControls();
    if (g_Window && g_OriginalWndProc) {
        const WNDPROC current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(g_Window, GWLP_WNDPROC));
        if (current == &CustomWndProc) {
            SetWindowLongPtrW(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_OriginalWndProc));
        }
    }
    g_Window = nullptr;
    g_OriginalWndProc = nullptr;
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
    MarkActivity();
    Logger::Input("Custom Chat input opened.");
}

void Close() {
    g_Open = false;
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
    HIMC context = ImmGetContext(g_Window);
    if (!context) return;

    COMPOSITIONFORM compositionForm{};
    compositionForm.dwStyle = CFS_POINT;
    compositionForm.ptCurrentPos = {x, y};
    ImmSetCompositionWindow(context, &compositionForm);

    CANDIDATEFORM candidateForm{};
    candidateForm.dwIndex = 0;
    candidateForm.dwStyle = CFS_CANDIDATEPOS;
    candidateForm.ptCurrentPos = {x, y};
    ImmSetCandidateWindow(context, &candidateForm);
    ImmReleaseContext(g_Window, context);
    g_ImePosition = {x, y};
}

void ClearAfterSubmit() {
    g_Text.clear();
    g_Composition.clear();
    g_Caret = 0;
    ClearSelection();
    MarkActivity();
}

void AddHistory(const std::wstring& text) {
    if (text.empty()) return;
    if (g_History.empty() || g_History.back() != text) g_History.push_back(text);
    if (g_History.size() > kMaxHistory) g_History.pop_front();
    g_HistoryIndex = g_History.size();
}

} // namespace HUB::Chat::Input
