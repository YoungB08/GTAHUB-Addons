#include "OVCrashSafety.h"

#include "OVLogger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace ov
{
namespace
{
std::filesystem::path g_directory;
#ifdef _WIN32
LPTOP_LEVEL_EXCEPTION_FILTER g_previous{};

std::filesystem::path DumpPath()
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{}; localtime_s(&local, &now);
    std::ostringstream name; name << "crash_" << std::put_time(&local, "%Y%m%d_%H%M%S") << ".dmp";
    return g_directory / name.str();
}

LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exception)
{
    std::error_code error;
    std::filesystem::create_directories(g_directory, error);
    const auto dumpPath = DumpPath();
    HANDLE file = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(), exception, FALSE};
        const auto dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dumpType, &info, nullptr, nullptr);
        CloseHandle(file);
    }
    std::ofstream log(g_directory / "crash.log", std::ios::app);
    if (log)
    {
        void* frames[32]{};
        const USHORT count = CaptureStackBackTrace(0, 32, frames, nullptr);
        log << "Unhandled exception code=0x" << std::hex << exception->ExceptionRecord->ExceptionCode << std::dec << "\n";
        log << "Stack frames: " << count << "\n";
        for (USHORT index = 0; index < count; ++index) log << "  " << frames[index] << "\n";
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot != INVALID_HANDLE_VALUE)
        {
            MODULEENTRY32W module{sizeof(module)};
            if (Module32FirstW(snapshot, &module)) do { log << "Module: " << std::filesystem::path(module.szModule).string() << "\n"; } while (Module32NextW(snapshot, &module));
            CloseHandle(snapshot);
        }
    }
    OV_LOG_ERROR("Crash", "Unhandled exception; dump written to %s", dumpPath.string().c_str());
    return g_previous ? g_previous(exception) : EXCEPTION_EXECUTE_HANDLER;
}
#endif
}

bool CrashSafety::Install(std::filesystem::path directory)
{
    g_directory = std::move(directory);
#ifdef _WIN32
    g_previous = SetUnhandledExceptionFilter(&UnhandledExceptionFilter);
    return true;
#else
    return false;
#endif
}
void CrashSafety::Uninstall()
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(g_previous);
    g_previous = nullptr;
#endif
    g_directory.clear();
}
}
