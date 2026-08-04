#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace HUB::Chat::Input {

bool Install(HWND window);
void Uninstall();

void Open();
void Close();
bool IsOpen();

const std::wstring& Text();
const std::wstring& Composition();
size_t Caret();
bool HasSelection();
size_t SelectionStart();
size_t SelectionEnd();
DWORD LastActivityTick();
uint64_t Generation();
void UpdateImePosition(LONG x, LONG y);

void ClearAfterSubmit();
void AddHistory(const std::wstring& text);

} // namespace HUB::Chat::Input
