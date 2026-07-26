#pragma once

#include <concepts>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "AudioFile/LoadWav.h"
#include "AudioFile/SaveWav.h"

namespace AbacDsp
{

// Belief data about a saved loop the audio itself can't tell us (unlike
// sample rate/length/channels, which are read straight back off the WAV).
struct LoopMetadata
{
    int version{1};
    float bpm{120.f};
};

// Shape nlohmann::json satisfies; keeps this header free of a JSON dependency.
template <typename Json>
concept JsonLike = requires(const LoopMetadata& meta, const std::string& text, Json j) {
    { Json(meta) };
    { Json::parse(text) } -> std::same_as<Json>;
    { j.dump() } -> std::convertible_to<std::string>;
    { j.template get<LoopMetadata>() } -> std::same_as<LoopMetadata>;
};

// WAV load/save with LoopMetadata embedded in the iXML chunk. `Json` supplies
// the (de)serialization, e.g. LoopFile<nlohmann::json>::saveStereoWav(...),
// with `to_json`/`from_json` for LoopMetadata defined by the caller.
template <JsonLike Json>
class LoopFile
{
  public:
    [[nodiscard]] static std::string serializeMetadata(const LoopMetadata& meta)
    {
        return Json(meta).dump();
    }

    [[nodiscard]] static std::optional<LoopMetadata> deserializeMetadata(const std::string& text)
    {
        try
        {
            return Json::parse(text).template get<LoopMetadata>();
        }
        catch (const std::exception& e)
        {
            std::cerr << "LoopFile: failed to parse metadata: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    static void saveStereoWav(const std::string& filename, const std::span<const float> left,
                              const std::span<const float> right, const float sampleRate, const LoopMetadata& meta)
    {
        auto af = AudioUtility::SaveWav::StereoToAudioFile(left, right, sampleRate);
        af.iXMLChunk = serializeMetadata(meta);
        af.save(filename);
    }

    struct LoadResult
    {
        std::vector<float> left;
        std::vector<float> right;
        float sampleRate{0.f};
        std::optional<LoopMetadata> embeddedMetadata;
    };

    [[nodiscard]] static LoadResult loadStereoWav(const std::string& filename)
    {
        LoadResult result;
        auto af = AudioUtility::LoadWav::loadAudioFile(filename);
        if (af.getNumChannels() < 2 || af.getNumSamplesPerChannel() == 0)
        {
            return result;
        }
        result.left = af.samples[0];
        result.right = af.samples[1];
        result.sampleRate = static_cast<float>(af.getSampleRate());
        if (!af.iXMLChunk.empty())
        {
            result.embeddedMetadata = deserializeMetadata(af.iXMLChunk);
        }
        return result;
    }
};

}