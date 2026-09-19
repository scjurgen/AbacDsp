#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Graph/CompiledGraph.h"
#include "Graph/GraphCompiler.h"
#include "Graph/Lua/LuaGraphLoader.h"
#include "Graph/Node.h"
#include "Graph/NodeRegistry.h"
#include "Graph/Nodes/TapeDelayNode.h"

/**
 * @brief Minimal "Tape Vibrato" graph: one WobbleDelay per channel inside an UpDownSampler,
 * both identically seeded so they share the same tape-like wow and flutter, 100% wet.
 *
 * The audio chain is built by loading chorus.md's own "First graph: tape vibrato" Lua
 * text through the graph toolbox (LuaGraphLoader -> GraphValidator -> GraphCompiler),
 * not by driving the delays directly - the seed cutover of the planned
 * Lua tape-modulation toolbox.
 */
template <size_t BlockSize>
class PathfinderImpl final : public EffectBase
{
  public:
    static constexpr size_t kBufferSize{8192};
    static constexpr float kBaseDelayMs{8.f};
    // Mirror TapeDelayNode.h's own config defaults - kGraphScript below doesn't
    // override them, so these stay accurate documentation of the fixed config.
    static constexpr float kSafetyMarginSamples{250.f};
    static constexpr float kFlutterRateFloorHz{0.6f};
    using TapeNode = AbacDsp::Graph::Nodes::TapeDelayNode<BlockSize>;
    using Delay = typename TapeNode::Delay;
    static constexpr auto kSharedSeed{TapeNode::kSharedSeed};

    explicit PathfinderImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_graph(buildGraph(sampleRate))
    {
        m_tapeNode = m_graph.findNode(kTapeNodeId);
        assert(m_tapeNode != nullptr);
        m_visualWavedata.resize(6000);
        applyMacros();
    }

    void setDepth(const float percent)
    {
        m_depth = percent * 0.01f;
        applyMacros();
    }

    void setSpeed(const float hz)
    {
        m_speedHz = hz;
        applyMacros();
    }

    void setAggressivity(const float percent)
    {
        m_aggressivity = percent * 0.01f;
        applyMacros();
    }

    // 0% is tape-oriented (below nominal transport speed), 50% is nominal, 100% is a
    // clean, host-rate-equivalent transport - see chorus.md's Character mapping.
    void setCharacter(const float percent)
    {
        const auto fraction = percent * 0.01f;
        const auto ratio = std::exp2((fraction - 0.5f) * 2.f * kCharacterOctaves);
        m_tapeNode->setParameter(kParamTransportRatio, ratio);
    }

    void processBlock(const AbacDsp::AudioBuffer<2, BlockSize>& in, AbacDsp::AudioBuffer<2, BlockSize>& out)
    {
        std::array<float, BlockSize> inL{};
        std::array<float, BlockSize> inR{};
        for (size_t i = 0; i < BlockSize; ++i)
        {
            inL[i] = in(i, 0);
            inR[i] = in(i, 1);
        }

        std::array<float, BlockSize> outL{};
        std::array<float, BlockSize> outR{};
        std::array<const float*, 2> graphInputs{inL.data(), inR.data()};
        std::array<float*, 2> graphOutputs{outL.data(), outR.data()};
        m_graph.process(graphInputs, graphOutputs, BlockSize);

        for (size_t i = 0; i < BlockSize; ++i)
        {
            out(i, 0) = outL[i];
            out(i, 1) = outR[i];
            m_visualWavedata[m_currentSample] = out(i, 0) + out(i, 1);
            m_currentSample = (m_currentSample + 1) % m_visualWavedata.size();
        }
    }

    const std::vector<float>& visualizeWaveData()
    {
        m_preparedWavedata = m_visualWavedata;
        return m_preparedWavedata;
    }

  private:
    static constexpr float kCharacterOctaves{1.f};
    // Wow/flutter depth ranges reused from organicchorus's Classic config; flutter's is
    // the gentler one, per chorus.md's "wow depth primarily, flutter depth more gently".
    static constexpr float kWowDepthAtZero{0.15f};
    static constexpr float kWowDepthAtOne{0.45f};
    static constexpr float kFlutterDepthAtZero{0.1f};
    static constexpr float kFlutterDepthAtOne{0.5f};
    // OU Aggressivity's wow-variance/wow-drift scale, validated up to this value against
    // kSafetyMarginSamples via explore/pathfinder_vibrato.cpp.
    static constexpr float kMaxWowVariance{0.6f};
    static constexpr float kMaxWowDrift{0.6f};

    // Mirror TapeDelayNode.h's registerTapeDelayNode() parameter order.
    static constexpr size_t kParamTransportRatio{0};
    static constexpr size_t kParamWowDepth{1};
    static constexpr size_t kParamWowRate{2};
    static constexpr size_t kParamWowVariance{3};
    static constexpr size_t kParamWowDrift{4};
    static constexpr size_t kParamFlutterDepth{5};
    static constexpr size_t kParamFlutterRate{6};

    static constexpr std::string_view kTapeNodeId{"tape"};

    // chorus.md's own "First graph: tape vibrato" example, embedded verbatim.
    // clang-format off
    static constexpr std::string_view kGraphScript = R"lua(
return {
  version = 1,
  name = "Tape Vibrato",

  io = {
    inputs  = { "inL", "inR" },
    outputs = { "outL", "outR" },
  },

  nodes = {
    {
      id = "tape",
      type = "TapeDelay",
      config = {
        channels = 2,
        writeRateHz = 4800,
        maxDelayMs = 30,
        interpolation = "cubic",
        modulationMode = "sharedStereo",
      },
      params = {
        baseDelayMs = 8.0,
        transportRatio = 1.0,

        wowDepth = 0.25,
        wowRate = 0.30,
        wowVariance = 0.10,
        wowDrift = 0.00,

        flutterDepth = 0.02,
        flutterRate = 4.0,
      },
    },
  },

  edges = {
    { from = "inL", to = "tape.inL" },
    { from = "inR", to = "tape.inR" },
    { from = "tape.outL", to = "outL" },
    { from = "tape.outR", to = "outR" },
  },

  macros = {
    {
      id = "depth",
      label = "Depth",
      default = 0.35,
      targets = {
        { to = "tape.wowDepth", map = "vibratoWowDepth" },
        { to = "tape.flutterDepth", map = "vibratoFlutterDepth" },
      },
    },
    {
      id = "speed",
      label = "Speed",
      default = 0.35,
      targets = {
        { to = "tape.wowRate", map = "vibratoWowRate" },
        { to = "tape.flutterRate", map = "vibratoFlutterRate" },
      },
    },
    {
      id = "aggressivity",
      label = "OU Aggressivity",
      default = 0.15,
      targets = {
        { to = "tape.wowVariance", map = "vibratoWowVariance" },
        { to = "tape.wowDrift", map = "vibratoWowDrift" },
      },
    },
    {
      id = "character",
      label = "Character",
      default = 0.50,
      targets = {
        { to = "tape.transportRatio", map = "tapeToCleanTransportRatio" },
      },
    },
  },
}
)lua";
    // clang-format on

    // A load/validate/compile failure here means kGraphScript itself is broken - a
    // build integration bug, not a runtime condition to recover from.
    [[nodiscard]] static AbacDsp::Graph::CompiledGraph buildGraph(const float sampleRate)
    {
        const AbacDsp::Graph::Lua::LoadResult loaded =
            AbacDsp::Graph::Lua::LuaGraphLoader::loadFromString(kGraphScript);
        assert(loaded.description.has_value());

        AbacDsp::Graph::NodeRegistry registry;
        AbacDsp::Graph::Nodes::registerTapeDelayNode<BlockSize>(registry);

        auto compiled = AbacDsp::Graph::GraphCompiler::compile(*loaded.description, registry, BlockSize, sampleRate);
        assert(compiled.graph.has_value());
        return std::move(*compiled.graph);
    }

    void applyMacros()
    {
        m_tapeNode->setParameter(kParamWowRate, m_speedHz);
        m_tapeNode->setParameter(kParamFlutterRate, std::max(m_speedHz, kFlutterRateFloorHz));
        m_tapeNode->setParameter(kParamWowDepth, kWowDepthAtZero + (kWowDepthAtOne - kWowDepthAtZero) * m_depth);
        m_tapeNode->setParameter(kParamFlutterDepth,
                                 kFlutterDepthAtZero + (kFlutterDepthAtOne - kFlutterDepthAtZero) * m_depth);
        m_tapeNode->setParameter(kParamWowVariance, m_aggressivity * kMaxWowVariance);
        m_tapeNode->setParameter(kParamWowDrift, m_aggressivity * kMaxWowDrift);
    }

    AbacDsp::Graph::CompiledGraph m_graph;
    AbacDsp::Graph::Node* m_tapeNode{nullptr};
    float m_depth{0.35f};
    float m_speedHz{0.8f};
    float m_aggressivity{0.15f};

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{0};
};
