#pragma once

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>
#include <windows.h>

namespace Logger {

enum class Category {
    Info,
    Warn,
    Error,
    Hook,
    Chat,
    Input,
    CEF,
    D3D
};

void ClearLog();
void Log(Category category, const char* format, ...);

inline void Info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::Info, "%s", buffer);
}

inline void Hook(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::Hook, "%s", buffer);
}

inline void Chat(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::Chat, "%s", buffer);
}

inline void Input(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::Input, "%s", buffer);
}

inline void Error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::Error, "%s", buffer);
}

inline void D3DLog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::D3D, "%s", buffer);
}

inline void CEFLog(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    Log(Category::CEF, "%s", buffer);
}

} // namespace Logger
