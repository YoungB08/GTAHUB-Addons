#include "OVOpusDecoder.h"

#include "shared/OVConstants.h"

#include <opus.h>

namespace ov::client
{
OVOpusDecoder::OVOpusDecoder() : pcm_(FRAME_SAMPLES) {}
OVOpusDecoder::~OVOpusDecoder() { if (decoder_) opus_decoder_destroy(decoder_); }
bool OVOpusDecoder::Initialize()
{
    int error = OPUS_OK;
    decoder_ = opus_decoder_create(static_cast<opus_int32>(SAMPLE_RATE), 1, &error);
    return decoder_ != nullptr && error == OPUS_OK;
}
void OVOpusDecoder::Reset() { if (decoder_) opus_decoder_ctl(decoder_, OPUS_RESET_STATE); }
std::vector<std::int16_t> OVOpusDecoder::Decode(const std::uint8_t* encoded, std::size_t size, bool plc)
{
    if (!decoder_ || (!plc && (!encoded || size == 0))) return {};
    const int decoded = opus_decode(decoder_, plc ? nullptr : encoded, plc ? 0 : static_cast<opus_int32>(size), pcm_.data(), static_cast<int>(pcm_.size()), 0);
    if (decoded <= 0) return {};
    return std::vector<std::int16_t>(pcm_.begin(), pcm_.begin() + decoded);
}
}
