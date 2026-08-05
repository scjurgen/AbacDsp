#pragma once

#include "SrPushConverter.h"
#include "Filters/Sinc/sinc_4.h"

namespace AbacDsp
{

/// @ingroup srconverter
/// @brief One-shot offline resample of an interleaved buffer.
/// Allocates and builds a fresh kernel per call, so it belongs in tests and file conversion,
/// never on an audio path where SrPushConverter should be held and reused.
class ConvertSampleBuffer
{
  public:
    template <size_t NumChannels>
    static void convert(const float ratio, const std::vector<float>& in, std::vector<float>& out)
    {
        SrPushConverter<NumChannels> pc{std::make_shared<SincFilter>(sinc4)};
        out.resize(1000 + static_cast<size_t>(static_cast<float>(in.size()) * ratio));
        const auto generated = pc.fetchBlock(ratio, in.data(), in.size() / 2, out.data(), out.size());
        out.resize(generated * 2);
    }
};

}