#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ov::client
{
using BassHandle = std::uint32_t;
using BassWord = std::uint32_t;
using BassDword = std::uint32_t;
using BassRecordProc = int (*)(BassHandle, const void*, BassDword, void*);
using BassStreamProc = BassDword (*)(BassHandle, void*, BassDword, void*);

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
    [[nodiscard]] bool IsLoaded() const noexcept { return module_ != nullptr; }
    [[nodiscard]] const std::string& LastError() const noexcept { return lastError_; }

private:
    void* module_{};
    std::string lastError_;
    using InitFn = int (*)(int, std::uint32_t, std::uint32_t, void*, void*);
    using RecordInitFn = int (*)(int);
    using RecordFreeFn = int (*)();
    using RecordStartFn = BassHandle (*)(std::uint32_t, std::uint32_t, std::uint32_t, BassRecordProc, void*);
    using StreamCreateFn = BassHandle (*)(std::uint32_t, std::uint32_t, std::uint32_t, BassStreamProc, void*);
    using ChannelPlayFn = int (*)(BassHandle, int);
    using ChannelStopFn = int (*)(BassHandle);
    using ChannelFreeFn = int (*)(BassHandle);
    using ChannelSetAttributeFn = int (*)(BassHandle, std::uint32_t, float);
    using ChannelGetDataFn = BassDword (*)(BassHandle, void*, BassDword);
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
};
}
