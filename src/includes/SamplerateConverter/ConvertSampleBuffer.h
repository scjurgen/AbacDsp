#pragma once

#include "SrPushConverter.h"
#include "Sinc69.h"

namespace AbacDsp
{

class ConvertSampleBuffer
{
  public:
    template <size_t NumChannels>
    static void convert(const float ratio, const std::vector<float>& in, std::vector<float>& out)
    {
        SrPushConverter<NumChannels> pc{std::make_shared<SincFilter>(sinc69)};
        out.resize(1000 + static_cast<size_t>(static_cast<float>(in.size()) * ratio));
        const auto generated = pc.fetchBlock(ratio, in.data(), in.size() / 2, out.data(), out.size());
        out.resize(generated * 2);
    }
};

}