#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <cstdarg>
#include <cstdio>

namespace HUBRole {

/**
 * @brief Hệ thống Ghi Log chuyên dụng cho open:mp Server Component.
 * Ghi log ra thư mục `Hub-Plugin/HUB-ServerRole.log` (Tự động tạo folder/file nếu chưa có).
 */
class Logger {
private:
    inline static std::mutex s_logMutex;
    inline static const std::string s_logDir = "Hub-Plugin";
    inline static const std::string s_logFile = "Hub-Plugin/HUB-ServerRole.log";

public:
    static void Init() {
        std::lock_guard<std::mutex> lock(s_logMutex);
        try {
            std::filesystem::create_directories(s_logDir);
        } catch (...) {}
    }

    static void Log(const char* level, const char* format, ...) {
        std::lock_guard<std::mutex> lock(s_logMutex);
        try {
            std::filesystem::create_directories(s_logDir);
        } catch (...) {}

        std::ofstream logStream(s_logFile, std::ios::app);
        if (!logStream.is_open()) return;

        // Lấy thời gian hiện tại
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_buf, &in_time_t);
#else
        localtime_r(&in_time_t, &tm_buf);
#endif

        char timeStr[32];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

        // Format message
        char messageBuffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);

        logStream << "[" << timeStr << "] [" << level << "] " << messageBuffer << std::endl;
    }

    static void Info(const char* format, ...) {
        char messageBuffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
        Log("INFO", "%s", messageBuffer);
    }

    static void Warn(const char* format, ...) {
        char messageBuffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
        Log("WARN", "%s", messageBuffer);
    }

    static void Error(const char* format, ...) {
        char messageBuffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(messageBuffer, sizeof(messageBuffer), format, args);
        va_end(args);
        Log("ERROR", "%s", messageBuffer);
    }
};

} // namespace HUBRole
