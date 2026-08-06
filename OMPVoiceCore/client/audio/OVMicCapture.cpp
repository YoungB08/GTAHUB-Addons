#include "OVMicCapture.h"

#include "shared/OVConstants.h"

#include <algorithm>
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
    return recordHandle_ != 0;
}
void OVMicCapture::Stop()
{
    if (recordHandle_ != 0) bass_.ChannelFree(recordHandle_);
    recordHandle_ = 0;
    bass_.FreeRecord();
}
std::optional<std::vector<std::int16_t>> OVMicCapture::PopFrame() { return frames_.TryPop(); }
std::vector<float> OVMicCapture::Waveform() const
{
    std::lock_guard<std::mutex> lock(waveformMutex_);
    return std::vector<float>(waveform_.begin(), waveform_.end());
}

int OV_BASS_CALL OVMicCapture::OnRecord(BassHandle, const void* buffer, BassDword length, void* user)
{
    auto* capture = static_cast<OVMicCapture*>(user);
    if (!capture || !buffer || length < sizeof(std::int16_t)) return 1;
    const auto* samples = static_cast<const std::int16_t*>(buffer);
    const std::size_t count = length / sizeof(std::int16_t);
    std::vector<std::int16_t> frame(samples, samples + count);
    double sum = 0.0;
    std::int16_t peak = 0;
    for (const auto sample : frame) { sum += static_cast<double>(sample) * sample; peak = std::max(peak, static_cast<std::int16_t>(std::abs(sample))); }
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
