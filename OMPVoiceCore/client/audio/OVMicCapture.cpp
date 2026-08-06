#include "OVMicCapture.h"

#include "shared/OVConstants.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ov::client
{
OVMicCapture::OVMicCapture(OVBassApi& bass) : bass_(bass) {}
OVMicCapture::~OVMicCapture() { Stop(); }
bool OVMicCapture::Initialize(int device) { return bass_.InitRecord(device); }
bool OVMicCapture::Start()
{
    if (recordHandle_ != 0) return true;
    recordHandle_ = bass_.StartRecord(SAMPLE_RATE, 1, &OVMicCapture::OnRecord, this);
    if (recordHandle_ != 0 && highPassEnabled_) highPassFx_ = bass_.AddHighPassFilter(recordHandle_, 120.0F);
    return recordHandle_ != 0;
}
void OVMicCapture::Stop()
{
    if (recordHandle_ != 0 && highPassFx_ != 0) bass_.RemoveFx(recordHandle_, highPassFx_);
    highPassFx_ = 0;
    if (recordHandle_ != 0) bass_.ChannelStop(recordHandle_);
    recordHandle_ = 0;
    bass_.FreeRecord();
}
void OVMicCapture::EnableHighPass(bool enable)
{
    highPassEnabled_ = enable;
    if (recordHandle_ == 0) return;
    if (!enable && highPassFx_ != 0)
    {
        bass_.RemoveFx(recordHandle_, highPassFx_);
        highPassFx_ = 0;
    }
    else if (enable && highPassFx_ == 0) highPassFx_ = bass_.AddHighPassFilter(recordHandle_, 120.0F);
}
std::optional<std::vector<std::int16_t>> OVMicCapture::PopFrame() { return frames_.TryPop(); }
std::vector<float> OVMicCapture::Waveform() const
{
    std::lock_guard<std::mutex> lock(waveformMutex_);
    return std::vector<float>(waveform_.begin(), waveform_.end());
}
float OVMicCapture::MeasureCallbackMicros(std::size_t iterations)
{
    OVBassApi bass;
    OVMicCapture capture(bass);
    std::array<std::int16_t, FRAME_SAMPLES> samples{};
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration)
        OnRecord(0, samples.data(), static_cast<BassDword>(samples.size() * sizeof(samples[0])), &capture);
    return static_cast<float>(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / static_cast<double>(iterations));
}
void OVMicCapture::InjectSamplesForTesting(const std::vector<std::int16_t>& samples)
{
    if (!samples.empty()) OnRecord(0, samples.data(), static_cast<BassDword>(samples.size() * sizeof(samples[0])), this);
}

int OV_BASS_CALL OVMicCapture::OnRecord(BassHandle, const void* buffer, BassDword length, void* user)
{
    auto* capture = static_cast<OVMicCapture*>(user);
    if (!capture || !buffer || length < sizeof(std::int16_t)) return 1;
    const auto* samples = static_cast<const std::int16_t*>(buffer);
    const std::size_t count = length / sizeof(std::int16_t);
    std::vector<std::int16_t> frame(samples, samples + count);
    double sum = 0.0;
    int peak = 0;
    for (const auto sample : frame) { sum += static_cast<double>(sample) * sample; peak = std::max(peak, std::abs(static_cast<int>(sample))); }
    capture->rms_.store(frame.empty() ? 0.0F : static_cast<float>(std::sqrt(sum / frame.size()) / 32768.0), std::memory_order_relaxed);
    capture->peak_.store(static_cast<float>(peak) / 32768.0F, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(capture->waveformMutex_);
        capture->waveform_.fill(0.0F);
        const std::size_t step = std::max<std::size_t>(1, frame.size() / capture->waveform_.size());
        for (std::size_t index = 0; index < capture->waveform_.size() && index * step < frame.size(); ++index)
        {
            const std::size_t end = std::min(frame.size(), (index + 1) * step);
            float value = 0.0F;
            for (std::size_t sample = index * step; sample < end; ++sample)
                value = std::max(value, std::abs(static_cast<float>(frame[sample])) / 32768.0F);
            capture->waveform_[index] = value;
        }
    }
    capture->frames_.Push(std::move(frame));
    return 1;
}
}
