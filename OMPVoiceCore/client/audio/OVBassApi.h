#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ov::client
{
#ifdef _WIN32
#define OV_BASS_CALL __stdcall
#else
#define OV_BASS_CALL
#endif
using BassHandle = std::uint32_t;
using BassWord = std::uint32_t;
using BassDword = std::uint32_t;
using BassRecordProc = int (OV_BASS_CALL*)(BassHandle, const void*, BassDword, void*);
using BassStreamProc = BassDword (OV_BASS_CALL*)(BassHandle, void*, BassDword, void*);

class OVBassApi final
{
public:
    OVBassApi();
    ~OVBassApi();
    bool Load(const std::string& directory = {});
    void Unload();
    bool InitOutput(std::uint32_t frequency);
    bool InitRecord(int device);
    void FreeRecord();
    BassHandle StartRecord(std::uint32_t frequency, std::uint32_t channels, BassRecordProc callback, void* user);
    BassHandle CreateStream(std::uint32_t frequency, std::uint32_t channels, BassStreamProc callback, void* user);
    bool ChannelPlay(BassHandle channel);
    bool ChannelStop(BassHandle channel);
    bool ChannelFree(BassHandle channel);
    bool ChannelSetVolume(BassHandle channel, float volume);
    BassDword ChannelGetData(BassHandle channel, void* buffer, BassDword length);
    [[nodiscard]] std::vector<std::string> OutputDevices() const;
    [[nodiscard]] std::vector<std::string> RecordDevices() const;
    [[nodiscard]] bool IsLoaded() const noexcept { return module_ != nullptr; }
    [[nodiscard]] bool IsFxLoaded() const noexcept { return fxModule_ != nullptr; }
    [[nodiscard]] const std::string& LastError() const noexcept { return lastError_; }

private:
    void* module_{};
    void* fxModule_{};
    std::string lastError_;
    using InitFn = int (OV_BASS_CALL*)(int, std::uint32_t, std::uint32_t, void*, void*);
    using RecordInitFn = int (OV_BASS_CALL*)(int);
    using RecordFreeFn = int (OV_BASS_CALL*)();
    using RecordStartFn = BassHandle (OV_BASS_CALL*)(std::uint32_t, std::uint32_t, std::uint32_t, BassRecordProc, void*);
    using StreamCreateFn = BassHandle (OV_BASS_CALL*)(std::uint32_t, std::uint32_t, std::uint32_t, BassStreamProc, void*);
    using ChannelPlayFn = int (OV_BASS_CALL*)(BassHandle, int);
    using ChannelStopFn = int (OV_BASS_CALL*)(BassHandle);
    using ChannelFreeFn = int (OV_BASS_CALL*)(BassHandle);
    using ChannelSetAttributeFn = int (OV_BASS_CALL*)(BassHandle, std::uint32_t, float);
    using ChannelGetDataFn = BassDword (OV_BASS_CALL*)(BassHandle, void*, BassDword);
    struct DeviceInfo { const char* name; const char* driver; std::uint32_t flags; };
    using DeviceInfoFn = int (OV_BASS_CALL*)(std::uint32_t, DeviceInfo*);
    InitFn init_{};
    RecordInitFn recordInit_{};
    RecordFreeFn recordFree_{};
    RecordStartFn recordStart_{};
    StreamCreateFn streamCreate_{};
    ChannelPlayFn channelPlay_{};
    ChannelStopFn channelStop_{};
    ChannelFreeFn channelFree_{};
    ChannelSetAttributeFn channelSetAttribute_{};
    ChannelGetDataFn channelGetData_{};
    DeviceInfoFn getDeviceInfo_{};
    DeviceInfoFn recordGetDeviceInfo_{};
};
}
