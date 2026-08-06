#include "OVLogger.h"

#include <array>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ov
{
Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}

bool Logger::Initialize(const std::filesystem::path& directory, const std::string& fileName, const std::string& threadName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    path_ = directory / fileName;
    threadName_ = threadName;
    RotateIfNeeded();
    stream_.open(path_, std::ios::app);
    return stream_.is_open();
}

void Logger::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open()) stream_.close();
}

void Logger::RotateIfNeeded()
{
    std::error_code error;
    if (!path_.empty() && std::filesystem::exists(path_, error) && std::filesystem::file_size(path_, error) >= maxBytes_)
    {
        const auto backup = path_.string() + ".1";
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(path_, backup, error);
    }
}

void Logger::Write(LogLevel level, const char* subsystem, const char* format, ...)
{
    std::array<char, 2048> message{};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message.data(), message.size(), format, args);
    va_end(args);

    const auto now = std::chrono::system_clock::now();
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    static constexpr const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    std::ostringstream line;
    line << '[' << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.'
         << std::setfill('0') << std::setw(3) << milliseconds.count() << "][$";
    std::string formatted = line.str();
    formatted.pop_back();
    formatted += threadName_ + "][" + (subsystem ? subsystem : "Core") + "][" + names[static_cast<int>(level)] + "] " + message.data() + '\n';

    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open())
    {
        stream_ << formatted;
        stream_.flush();
    }
#ifdef _WIN32
    ::OutputDebugStringA(formatted.c_str());
#endif
}
}
