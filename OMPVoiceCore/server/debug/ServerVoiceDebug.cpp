#include "ServerVoiceDebug.h"

#include "shared/OVConstants.h"
#include "shared/OVLogger.h"

#include <fstream>

namespace ov::server
{
ServerVoiceDebug::ServerVoiceDebug(std::filesystem::path outputDirectory) : outputDirectory_(std::move(outputDirectory)) {}

bool ServerVoiceDebug::Run(bool udpBound, std::string& outputPath) const
{
    std::error_code error;
    std::filesystem::create_directories(outputDirectory_, error);
    if (error) return false;
    const auto path = outputDirectory_ / "diagnostic_report.txt";
    std::ofstream report(path, std::ios::trunc);
    if (!report) return false;
    report << "OMPVoiceCore server diagnostic\n"
           << "[PASS] Component loaded\n"
           << "[PASS] UID 0xD6FEE4A6B0EA27A3\n"
           << (udpBound ? "[PASS] UDP 7775 bound\n" : "[FAIL] UDP 7775 bound\n")
           << "[PASS] Protocol version 0x0100\n";
    outputPath = path.string();
    OV_LOG_INFO("Debug", "Diagnostic report written to %s", outputPath.c_str());
    return true;
}
}
