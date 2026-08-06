#include "OVOpusEncoder.h"

#include "shared/OVConstants.h"

#include <opus.h>

namespace ov::client
{
OVOpusEncoder::OVOpusEncoder() : buffer_(MAX_ENCODED_FRAME) {}
OVOpusEncoder::~OVOpusEncoder() { Shutdown(); }
bool OVOpusEncoder::Initialize()
{
    int error = OPUS_OK;
    encoder_ = opus_encoder_create(static_cast<opus_int32>(SAMPLE_RATE), 1, OPUS_APPLICATION_VOIP, &error);
    if (!encoder_ || error != OPUS_OK) { encoder_ = nullptr; return false; }
    opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(24000));
    opus_encoder_ctl(encoder_, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(encoder_, OPUS_SET_DTX(1));
    opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));
    return true;
}
void OVOpusEncoder::Shutdown() { if (encoder_) opus_encoder_destroy(encoder_); encoder_ = nullptr; }
std::vector<std::uint8_t> OVOpusEncoder::Encode(const std::int16_t* pcm, std::size_t samples)
{
    if (!encoder_ || !pcm || samples != FRAME_SAMPLES) return {};
    const int encoded = opus_encode(encoder_, pcm, static_cast<int>(samples), buffer_.data(), static_cast<opus_int32>(buffer_.size()));
    if (encoded <= 0) return {};
    return std::vector<std::uint8_t>(buffer_.begin(), buffer_.begin() + encoded);
}
}
