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
    primaryFileName_ = fileName;
    threadName_ = threadName;
    RotateIfNeeded(path_);
    stream_.open(path_, std::ios::app);
    return stream_.is_open();
}

void Logger::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_.is_open()) stream_.close();
    for (auto& [_, stream] : subsystemStreams_) if (stream.is_open()) stream.close();
    subsystemStreams_.clear();
}

void Logger::RotateIfNeeded(const std::filesystem::path& path, std::ofstream* openStream)
{
    std::error_code error;
    if (!path.empty() && std::filesystem::exists(path, error) && std::filesystem::file_size(path, error) >= maxBytes_)
    {
        if (openStream && openStream->is_open()) openStream->close();
        const auto backup = path.string() + ".1";
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(path, backup, error);
        if (openStream) openStream->open(path, std::ios::app);
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
    RotateIfNeeded(path_, &stream_);
    std::ofstream* output = &stream_;
    const std::string subsystemName = subsystem ? subsystem : "Core";
    std::string routedFile;
    if (subsystemName == "Audio") routedFile = "audio.log";
    else if (subsystemName == "Network") routedFile = "network.log";
    else if (subsystemName == "Server") routedFile = "server.log";
    if (!routedFile.empty() && routedFile != primaryFileName_)
    {
        auto it = subsystemStreams_.find(routedFile);
        if (it == subsystemStreams_.end())
        {
            const auto targetPath = path_.parent_path() / routedFile;
            std::ofstream target(targetPath, std::ios::app);
            it = subsystemStreams_.emplace(routedFile, std::move(target)).first;
        }
        RotateIfNeeded(path_.parent_path() / routedFile, &it->second);
        output = &it->second;
    }
    if (output->is_open())
    {
        *output << formatted;
        output->flush();
    }
#ifdef _WIN32
    ::OutputDebugStringA(formatted.c_str());
#endif
}
}
