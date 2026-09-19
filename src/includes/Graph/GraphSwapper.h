#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include "Graph/CompiledGraph.h"
#include "Numbers/EqualPowerCrossfade.h"

namespace AbacDsp::Graph
{

/**
 * @ingroup graph
 * @brief Replaces a running CompiledGraph with a new one without clicks or audio-thread allocation.
 *
 * One audio thread calls process(); one control thread calls submit() and collectRetired().
 * A submitted graph is adopted at the start of the next block and faded in against the old
 * one, both running, with an equal-power crossfade. The incoming graph starts from fresh
 * node state; the caller applies its parameters before submit(). Graphs must be compiled
 * for the same maxBlockSize and sample rate as the audio callback uses.
 *
 * The audio thread never frees a graph: a finished one waits in a retired slot for
 * collectRetired(). While it waits, or while a fade runs, a newer submission stays pending
 * and the running graph keeps playing. A second submit() before adoption replaces the first.
 */
class GraphSwapper
{
  public:
    GraphSwapper(CompiledGraph initial, const size_t maxBlockSize, const size_t fadeSamples)
        : m_active(std::make_unique<CompiledGraph>(std::move(initial)))
        , m_numInputs(m_active->graphInputCount())
        , m_numOutputs(m_active->graphOutputCount())
        , m_maxBlockSize(maxBlockSize)
        , m_fadeSamples(fadeSamples)
        , m_outgoingBuffers(m_numOutputs, std::vector<float>(maxBlockSize, 0.f))
        , m_incomingBuffers(m_numOutputs, std::vector<float>(maxBlockSize, 0.f))
        , m_outgoingPointers(m_numOutputs, nullptr)
        , m_incomingPointers(m_numOutputs, nullptr)
    {
        for (size_t channel = 0; channel < m_numOutputs; ++channel)
        {
            m_outgoingPointers[channel] = m_outgoingBuffers[channel].data();
            m_incomingPointers[channel] = m_incomingBuffers[channel].data();
        }
    }

    ~GraphSwapper()
    {
        const std::unique_ptr<CompiledGraph> pending{m_pending.load()};
        const std::unique_ptr<CompiledGraph> retired{m_retired.load()};
    }

    GraphSwapper(const GraphSwapper&) = delete;
    GraphSwapper& operator=(const GraphSwapper&) = delete;
    GraphSwapper(GraphSwapper&&) = delete;
    GraphSwapper& operator=(GraphSwapper&&) = delete;

    /// Control thread. Refuses, leaving the graph untouched, one whose input or output count differs.
    [[nodiscard]] bool submit(CompiledGraph&& graph)
    {
        if (graph.graphInputCount() != m_numInputs || graph.graphOutputCount() != m_numOutputs)
        {
            return false;
        }
        auto incoming = std::make_unique<CompiledGraph>(std::move(graph));
        const std::unique_ptr<CompiledGraph> replaced{
            m_pending.exchange(incoming.release(), std::memory_order_acq_rel)};
        return true;
    }

    /// Control thread. Frees a graph the audio thread has finished with; true if there was one.
    bool collectRetired() noexcept
    {
        const std::unique_ptr<CompiledGraph> retired{m_retired.exchange(nullptr, std::memory_order_acq_rel)};
        return retired != nullptr;
    }

    /// Any thread; applies from the next graph swap on.
    void setFadeSamples(const size_t fadeSamples) noexcept
    {
        m_fadeSamples.store(fadeSamples, std::memory_order_relaxed);
    }

    /// Audio thread.
    void process(const std::span<const float*> graphInputs, const std::span<float*> graphOutputs,
                 const size_t numSamples) noexcept
    {
        assert(graphInputs.size() == m_numInputs);
        assert(graphOutputs.size() == m_numOutputs);
        assert(numSamples <= m_maxBlockSize);

        adoptPending();
        if (!m_outgoing)
        {
            m_active->process(graphInputs, graphOutputs, numSamples);
            return;
        }
        m_outgoing->process(graphInputs, m_outgoingPointers, numSamples);
        m_active->process(graphInputs, m_incomingPointers, numSamples);
        mixChannels(graphOutputs, numSamples);
        m_crossfade.skip(numSamples);
        if (m_crossfade.isDone())
        {
            m_retired.store(m_outgoing.release(), std::memory_order_release);
        }
    }

    /// Audio thread.
    [[nodiscard]] bool isCrossfading() const noexcept
    {
        return m_outgoing != nullptr;
    }

  private:
    void adoptPending() noexcept
    {
        if (m_outgoing || m_retired.load(std::memory_order_acquire) != nullptr)
        {
            return;
        }
        std::unique_ptr<CompiledGraph> next{m_pending.exchange(nullptr, std::memory_order_acq_rel)};
        if (!next)
        {
            return;
        }
        const auto fadeSamples = m_fadeSamples.load(std::memory_order_relaxed);
        if (fadeSamples == 0)
        {
            m_retired.store(m_active.release(), std::memory_order_release);
        }
        else
        {
            m_outgoing = std::move(m_active);
            m_crossfade.start(fadeSamples);
        }
        m_active = std::move(next);
    }

    void mixChannels(const std::span<float*> graphOutputs, const size_t numSamples) noexcept
    {
        for (size_t channel = 0; channel < m_numOutputs; ++channel)
        {
            auto channelFade = m_crossfade;
            channelFade.mix(std::span<const float>{m_outgoingPointers[channel], numSamples},
                            std::span<const float>{m_incomingPointers[channel], numSamples},
                            std::span<float>{graphOutputs[channel], numSamples});
        }
    }

    std::unique_ptr<CompiledGraph> m_active;
    std::unique_ptr<CompiledGraph> m_outgoing;
    const size_t m_numInputs;
    const size_t m_numOutputs;
    const size_t m_maxBlockSize;
    std::atomic<size_t> m_fadeSamples;
    std::atomic<CompiledGraph*> m_pending{nullptr};
    std::atomic<CompiledGraph*> m_retired{nullptr};
    std::vector<std::vector<float>> m_outgoingBuffers;
    std::vector<std::vector<float>> m_incomingBuffers;
    std::vector<float*> m_outgoingPointers;
    std::vector<float*> m_incomingPointers;
    EqualPowerCrossfade m_crossfade;
};

}
