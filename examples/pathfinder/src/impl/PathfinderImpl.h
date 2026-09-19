#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Audio/AudioBuffer.h"
#include "EffectBase.h"
#include "Graph/GraphSwapper.h"
#include "Graph/MacroBank.h"
#include "PathfinderScriptEngine.h"

/**
 * @brief A tape-modulation effect whose signal chain is a user-editable graph script.
 *
 * The graph starts as the tape vibrato preset. setScript() compiles a new script on the caller's
 * thread and swaps it in with a short crossfade; a script that fails leaves the running graph
 * alone and reports its error. The script's macros are its only controls: each is a knob whose
 * value the graph reads itself, so a control callback may run on any thread. collectRetired() frees
 * graphs the audio thread has finished with and belongs on a non-audio thread.
 */
template <size_t BlockSize>
class PathfinderImpl final : public EffectBase
{
  public:
    using Engine = PathfinderScriptEngine<BlockSize>;
    using UiParamSlots = typename Engine::KnobSlots;

    static constexpr float kFadeSeconds{0.03f};

    explicit PathfinderImpl(const float sampleRate)
        : EffectBase(sampleRate)
        , m_engine(m_bank, sampleRate)
        , m_swapper(makeDefaultGraph(), BlockSize, static_cast<size_t>(kFadeSeconds * sampleRate))
    {
        m_visualWavedata.resize(6000);
        applyMacroDefaults(m_defaultMacros);
        m_macros = m_defaultMacros;
    }

    // The script's macros, in declaration order, shown as knobs one to eight; each takes a raw 0 to 1.
    void setLuaParam1(const float value)
    {
        setKnob(0, value);
    }

    void setLuaParam2(const float value)
    {
        setKnob(1, value);
    }

    void setLuaParam3(const float value)
    {
        setKnob(2, value);
    }

    void setLuaParam4(const float value)
    {
        setKnob(3, value);
    }

    void setLuaParam5(const float value)
    {
        setKnob(4, value);
    }

    void setLuaParam6(const float value)
    {
        setKnob(5, value);
    }

    void setLuaParam7(const float value)
    {
        setKnob(6, value);
    }

    void setLuaParam8(const float value)
    {
        setKnob(7, value);
    }

    // Graph scripts have no imports; the resolver the processor offers is not used.
    template <typename Resolver>
    void setImportResolver(Resolver&&)
    {
    }

    bool setScript(const std::string_view text)
    {
        auto result = m_engine.compile(text);
        if (!result.graph)
        {
            m_scriptError = std::move(result.error);
            return false;
        }
        if (!m_swapper.submit(std::move(*result.graph)))
        {
            m_scriptError = "the graph could not replace the running one";
            return false;
        }
        m_scriptError.clear();
        m_scriptWarnings = std::move(result.warnings);
        m_macros = std::move(result.macros);
        static_cast<void>(m_swapper.collectRetired());
        return true;
    }

    [[nodiscard]] bool hasScriptError() const noexcept
    {
        return !m_scriptError.empty();
    }

    [[nodiscard]] const std::string& scriptError() const noexcept
    {
        return m_scriptError;
    }

    [[nodiscard]] const std::string& scriptWarnings() const noexcept
    {
        return m_scriptWarnings;
    }

    [[nodiscard]] static std::string scriptSkeleton()
    {
        return Engine::skeleton();
    }

    [[nodiscard]] UiParamSlots uiParamSlots() const
    {
        return Engine::knobSlots(m_macros);
    }

    // Message thread: frees graphs a finished swap has left behind; true if one was freed.
    bool collectRetired() noexcept
    {
        return m_swapper.collectRetired();
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
        m_swapper.process(graphInputs, graphOutputs, BlockSize);

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
    // A new instance starts on the default script's declared defaults; the host's parameter values
    // then replace them as they arrive.
    void applyMacroDefaults(const std::vector<AbacDsp::Graph::LoweredMacro>& macros)
    {
        for (const auto& macro : macros)
        {
            m_bank.set(macro.slot, macro.defaultNormalized);
        }
    }

    void setKnob(const size_t knob, const float value)
    {
        m_bank.set(knob, value);
    }

    [[nodiscard]] AbacDsp::Graph::CompiledGraph makeDefaultGraph()
    {
        auto result = m_engine.compile(Engine::skeleton());
        assert(result.graph.has_value());
        m_defaultMacros = std::move(result.macros);
        return std::move(*result.graph);
    }

    AbacDsp::Graph::MacroBank m_bank;
    Engine m_engine;
    std::vector<AbacDsp::Graph::LoweredMacro> m_defaultMacros;
    AbacDsp::Graph::GraphSwapper m_swapper;
    std::vector<AbacDsp::Graph::LoweredMacro> m_macros;
    std::string m_scriptError;
    std::string m_scriptWarnings;

    std::vector<float> m_visualWavedata;
    std::vector<float> m_preparedWavedata;
    size_t m_currentSample{0};
};
