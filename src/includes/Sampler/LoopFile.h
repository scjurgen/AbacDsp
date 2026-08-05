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

/// @ingroup sampler
/// @brief What a saved loop cannot state about itself.
/// Sample rate, length and channel count are read back off the WAV; tempo is not recoverable
/// from the audio, so it has to be carried alongside. bars and beats are derived at save time
/// and informational only, so a human reading the file need not redo the arithmetic.
struct LoopMetadata
{
    int version{1};
    float bpm{120.f};
    float bars{0.f};
    float beats{0.f};
};

/// @ingroup sampler
/// @brief The shape nlohmann::json satisfies, taken as a template parameter so this header
/// needs no JSON dependency of its own and the library stays free of one.
template <typename Json>
concept JsonLike = requires(const LoopMetadata& meta, const std::string& text, Json j) {
    { Json(meta) };
    { Json::parse(text) } -> std::same_as<Json>;
    { j.dump() } -> std::convertible_to<std::string>;
    { j.template get<LoopMetadata>() } -> std::same_as<LoopMetadata>;
};

/**
 * @ingroup sampler
 * @brief WAV load and save with LoopMetadata embedded in the iXML chunk.
 *
 * iXML is a standard chunk that other tools ignore gracefully, so a loop
 * written here stays a plain WAV everywhere else while still carrying its
 * tempo. Json supplies the serialisation, with to_json and from_json for
 * LoopMetadata defined by the caller.
 *
 * Blocking file IO throughout: a loading-thread class, never an audio one.
 */
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