#include "OVBassApi.h"

#include "shared/OVLogger.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ov::client
{
namespace
{
constexpr std::uint32_t BASS_SAMPLE_MONO = 2;
constexpr std::uint32_t BASS_STREAM_AUTOFREE = 0x40000;
constexpr std::uint32_t BASS_ATTRIBUTE_VOLUME = 2;
constexpr std::uint32_t BASS_UNICODE = 0x80000000;
}

OVBassApi::OVBassApi() = default;
OVBassApi::~OVBassApi() { Unload(); }

bool OVBassApi::Load(const std::string& directory)
{
#ifdef _WIN32
    if (module_) return true;
    const std::string path = directory.empty() ? "bass.dll" : directory + "\\bass.dll";
    module_ = LoadLibraryA(path.c_str());
    if (!module_) { lastError_ = "bass.dll was not found"; return false; }
    auto load = [this](auto& target, const char* name) { target = reinterpret_cast<std::decay_t<decltype(target)>>(GetProcAddress(static_cast<HMODULE>(module_), name)); return target != nullptr; };
    if (!load(init_, "BASS_Init") || !load(recordInit_, "BASS_RecordInit") || !load(recordFree_, "BASS_RecordFree") ||
        !load(recordStart_, "BASS_RecordStart") || !load(streamCreate_, "BASS_StreamCreate") || !load(channelPlay_, "BASS_ChannelPlay") ||
        !load(channelStop_, "BASS_ChannelStop") || !load(channelFree_, "BASS_StreamFree") || !load(channelSetAttribute_, "BASS_ChannelSetAttribute") ||
        !load(channelGetData_, "BASS_ChannelGetData"))
    {
        lastError_ = "bass.dll is missing a required export";
        Unload();
        return false;
    }
    OV_LOG_INFO("Audio", "BASS runtime loaded");
    return true;
#else
    (void)directory;
    lastError_ = "BASS is a Windows runtime dependency";
    return false;
#endif
}

void OVBassApi::Unload()
{
#ifdef _WIN32
    if (module_) FreeLibrary(static_cast<HMODULE>(module_));
#endif
    module_ = nullptr;
    init_ = nullptr; recordInit_ = nullptr; recordFree_ = nullptr; recordStart_ = nullptr; streamCreate_ = nullptr;
    channelPlay_ = nullptr; channelStop_ = nullptr; channelFree_ = nullptr; channelSetAttribute_ = nullptr; channelGetData_ = nullptr;
}

bool OVBassApi::InitOutput(std::uint32_t frequency) { return init_ && init_(-1, frequency, 0, nullptr, nullptr) != 0; }
bool OVBassApi::InitRecord(int device) { return recordInit_ && recordInit_(device) != 0; }
void OVBassApi::FreeRecord() { if (recordFree_) recordFree_(); }
BassHandle OVBassApi::StartRecord(std::uint32_t frequency, std::uint32_t channels, BassRecordProc callback, void* user) { return recordStart_ ? recordStart_(frequency, channels | BASS_SAMPLE_MONO, 0, callback, user) : 0; }
BassHandle OVBassApi::CreateStream(std::uint32_t frequency, std::uint32_t channels, BassStreamProc callback, void* user) { return streamCreate_ ? streamCreate_(frequency, channels, BASS_STREAM_AUTOFREE, callback, user) : 0; }
bool OVBassApi::ChannelPlay(BassHandle channel) { return channelPlay_ && channelPlay_(channel, 0) != 0; }
bool OVBassApi::ChannelStop(BassHandle channel) { return channelStop_ && channelStop_(channel) != 0; }
bool OVBassApi::ChannelFree(BassHandle channel) { return channelFree_ && channelFree_(channel) != 0; }
bool OVBassApi::ChannelSetVolume(BassHandle channel, float volume) { return channelSetAttribute_ && channelSetAttribute_(channel, BASS_ATTRIBUTE_VOLUME, volume) != 0; }
BassDword OVBassApi::ChannelGetData(BassHandle channel, void* buffer, BassDword length) { return channelGetData_ ? channelGetData_(channel, buffer, length) : 0; }
}
