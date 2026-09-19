#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>

#include "Graph/MacroBank.h"
#include "Graph/Node.h"
#include "Graph/NodeRegistry.h"
#include "Graph/NodeSchema.h"

namespace AbacDsp::Graph::Nodes
{

/**
 * @ingroup graph
 * @brief Control source that fills its output with one MacroBank slot each block.
 *
 * 0 in, 1 out, no parameters; the slot is fixed at construction. Unlike Macro, whose value is
 * pushed in through setParameter(), this reads the bank itself, so an external control can change
 * it from any thread while the graph runs.
 */
class MacroInput final : public Node
{
  public:
    MacroInput(const MacroBank& bank, const size_t slot) noexcept
        : m_bank(bank)
        , m_slot(slot)
    {
    }

    void process(const std::span<const float*>, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        std::fill_n(outputs[0], numSamples, m_bank.get(m_slot));
    }

  private:
    const MacroBank& m_bank;
    size_t m_slot;
};

/**
 * @ingroup graph
 * @brief Registers "MacroInput" reading the given bank. Static config "slot" (default 0).
 */
inline void registerMacroInputNode(NodeRegistry& registry, const MacroBank& bank)
{
    const MacroBank* const bankPointer = &bank;
    registry.registerType(
        "MacroInput",
        NodeSchema{{PortDescriptor{
                       .name = "out", .direction = PortDirection::Output, .category = PortCategory::ControlAudioRate}},
                   {},
                   false},
        [bankPointer](const NodeInstance& instance, const float)
        {
            const auto it = instance.config.find("slot");
            const float slot = it == instance.config.end() ? 0.f : std::strtof(it->second.c_str(), nullptr);
            return std::make_unique<MacroInput>(*bankPointer, static_cast<size_t>(std::max(slot, 0.f)));
        });
}

}
