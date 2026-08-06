#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ov
{
enum class LogLevel { Debug, Info, Warning, Error };

class Logger final
{
public:
    static Logger& Instance();
    bool Initialize(const std::filesystem::path& directory, const std::string& fileName, const std::string& threadName);
    void Shutdown();
    void Write(LogLevel level, const char* subsystem, const char* format, ...);
    void SetMaxBytesForTesting(std::size_t bytes) noexcept { maxBytes_ = bytes; }

private:
    void RotateIfNeeded(const std::filesystem::path& path, std::ofstream* openStream = nullptr);
    std::mutex mutex_;
    std::ofstream stream_;
    std::unordered_map<std::string, std::ofstream> subsystemStreams_;
    std::filesystem::path path_;
    std::string primaryFileName_;
    std::string threadName_{"Main"};
    std::size_t maxBytes_{4U * 1024U * 1024U};
};
}

#define OV_LOG_INFO(subsystem, format, ...) ::ov::Logger::Instance().Write(::ov::LogLevel::Info, subsystem, format, ##__VA_ARGS__)
#define OV_LOG_WARN(subsystem, format, ...) ::ov::Logger::Instance().Write(::ov::LogLevel::Warning, subsystem, format, ##__VA_ARGS__)
#define OV_LOG_ERROR(subsystem, format, ...) ::ov::Logger::Instance().Write(::ov::LogLevel::Error, subsystem, format, ##__VA_ARGS__)
#define OV_LOG_DEBUG(subsystem, format, ...) ::ov::Logger::Instance().Write(::ov::LogLevel::Debug, subsystem, format, ##__VA_ARGS__)
