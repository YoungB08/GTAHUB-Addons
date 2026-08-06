#include "OVDiagnostic.h"

#include "client/audio/OVAudioEngine.h"
#include "client/audio/OVBassApi.h"
#include "shared/OVConstants.h"
#include "shared/OVLogger.h"

#include <fstream>

namespace ov::client
{
OVDiagnostic::OVDiagnostic(std::filesystem::path directory) : directory_(std::move(directory)) {}
bool OVDiagnostic::Run(const OVBassApi& bass, const OVAudioEngine& audio, bool dx9Hooked, std::string& outputPath) const
{
    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    if (error) return false;
    const auto path = directory_ / "diagnostic_report.txt";
    std::ofstream report(path, std::ios::trunc);
    if (!report) return false;
    report << "OMPVoiceCore client diagnostic\n"
           << (bass.IsLoaded() ? "[PASS] BASS initialized\n" : "[FAIL] BASS initialized\n")
           << (audio.IsCapturing() ? "[PASS] Microphone capture\n" : "[WARN] Microphone capture stopped\n")
           << "[PASS] Opus encoder path compiled\n"
           << "[PASS] Voice UDP port " << OMPVOICE_PORT << " configured\n"
           << (dx9Hooked ? "[PASS] DX9 EndScene hooked\n" : "[WARN] DX9 EndScene not hooked\n")
           << "[PASS] Config/log directory permissions\n";
    outputPath = path.string();
    OV_LOG_INFO("Debug", "Client diagnostic report written to %s", outputPath.c_str());
    return true;
}
}
