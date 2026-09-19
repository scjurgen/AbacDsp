#include <array>
#include <memory>
#include <thread>

#include "gtest/gtest.h"

#include "Graph/MacroBank.h"
#include "Graph/Nodes/MacroInput.h"

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr size_t kBlock{8};

[[nodiscard]] std::array<float, kBlock> run(Node& node)
{
    std::array<float, kBlock> output{};
    std::array<const float*, 0> inputs{};
    std::array<float*, 1> outputs{output.data()};
    node.process(inputs, outputs, kBlock);
    return output;
}

} // namespace

TEST(MacroBankTest, StartsAtZeroAndReturnsWhatWasSet)
{
    MacroBank bank;
    EXPECT_EQ(bank.get(0), 0.f);
    bank.set(3, 0.75f);
    EXPECT_EQ(bank.get(3), 0.75f);
    EXPECT_EQ(bank.get(2), 0.f);
}

TEST(MacroBankTest, SlotsOutsideTheBankReadZeroAndIgnoreWrites)
{
    MacroBank bank;
    bank.set(MacroBank::kSlotCount, 1.f);
    EXPECT_EQ(bank.get(MacroBank::kSlotCount), 0.f);
    EXPECT_EQ(bank.get(MacroBank::kSlotCount + 100), 0.f);
}

TEST(MacroBankTest, AWriterThreadIsSeenByTheReader)
{
    MacroBank bank;
    std::thread writer{[&bank] { bank.set(5, 0.5f); }};
    writer.join();
    EXPECT_EQ(bank.get(5), 0.5f);
}

TEST(MacroInputTest, FillsItsOutputWithTheCurrentSlotValueEachBlock)
{
    MacroBank bank;
    Nodes::MacroInput node{bank, 2};
    for (const float sample : run(node))
    {
        EXPECT_EQ(sample, 0.f);
    }
    bank.set(2, 0.4f);
    for (const float sample : run(node))
    {
        EXPECT_EQ(sample, 0.4f);
    }
}

TEST(MacroInputTest, RegisteredNodeReadsItsSlotFromConfig)
{
    MacroBank bank;
    bank.set(4, 0.9f);
    bank.set(0, 0.1f);
    NodeRegistry registry;
    Nodes::registerMacroInputNode(registry, bank);

    ASSERT_NE(registry.findSchema("MacroInput"), nullptr);
    const NodeInstance slotFour{.id = "m", .type = "MacroInput", .config = {{"slot", "4"}}};
    const NodeInstance defaulted{.id = "m", .type = "MacroInput"};
    auto four = registry.create("MacroInput", slotFour, 48000.f);
    auto zero = registry.create("MacroInput", defaulted, 48000.f);
    EXPECT_EQ(run(*four)[0], 0.9f);
    EXPECT_EQ(run(*zero)[0], 0.1f);
}

}
