#include "pch.h"
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>

namespace {
std::mutex g_LogMutex;

const char* CategoryToString(Logger::Category category) {
    switch (category) {
        case Logger::Category::Info:  return "[INFO]";
        case Logger::Category::Warn:  return "[WARN]";
        case Logger::Category::Error: return "[ERROR]";
        case Logger::Category::Hook:  return "[HOOK]";
        case Logger::Category::Chat:  return "[CHAT]";
        case Logger::Category::Input: return "[INPUT]";
        case Logger::Category::CEF:   return "[CEF]";
        case Logger::Category::D3D:   return "[D3D]";
        default: return "[LOG]";
    }
}
} // namespace

namespace Logger {

void ClearLog() {
    std::lock_guard<std::mutex> guard(g_LogMutex);
    std::ofstream stream("HUB-Core.log", std::ios::out | std::ios::trunc);
}

void Log(Category category, const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm timeInfo{};
    localtime_s(&timeInfo, &timeT);

    char formattedLine[2560];
    snprintf(formattedLine, sizeof(formattedLine), "[%02d:%02d:%02d.%03d] %-7s %s\n",
        timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec, static_cast<int>(ms.count()),
        CategoryToString(category), buffer);

    std::lock_guard<std::mutex> guard(g_LogMutex);
    std::ofstream stream("HUB-Core.log", std::ios::out | std::ios::app);
    if (stream.is_open()) {
        stream << formattedLine;
    }
    OutputDebugStringA(formattedLine);
}

} // namespace Logger
