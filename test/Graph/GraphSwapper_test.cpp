#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/GraphCompiler.h"
#include "Graph/GraphDescription.h"
#include "Graph/GraphSwapper.h"
#include "Graph/NodeRegistry.h"
#include "GraphTestNodes.h"
#include "Numbers/EqualPowerCrossfade.h"

namespace
{
thread_local bool tCountAllocations{false};
thread_local size_t tAllocations{0};
thread_local size_t tDeallocations{0};
}

void* operator new(const std::size_t size)
{
    if (tCountAllocations)
    {
        ++tAllocations;
    }
    if (void* pointer = std::malloc(size))
    {
        return pointer;
    }
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept
{
    if (tCountAllocations)
    {
        ++tDeallocations;
    }
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    if (tCountAllocations)
    {
        ++tDeallocations;
    }
    std::free(pointer);
}

namespace AbacDsp::Graph::Test
{
namespace
{

constexpr size_t kMaxBlock{64};
constexpr float kSampleRate{48000.f};
constexpr float kTolerance{1E-6f};

struct Probe
{
    std::atomic<int> created{0};
    std::atomic<int> destroyed{0};
    std::atomic<int> destroyedOnAudioThread{0};
    std::atomic<std::thread::id> audioThread{};
};

class ProbeGainNode final : public Node
{
  public:
    explicit ProbeGainNode(Probe& probe)
        : m_probe(probe)
    {
        ++m_probe.created;
    }

    ~ProbeGainNode() override
    {
        ++m_probe.destroyed;
        if (std::this_thread::get_id() == m_probe.audioThread.load())
        {
            ++m_probe.destroyedOnAudioThread;
        }
    }

    ProbeGainNode(const ProbeGainNode&) = delete;
    ProbeGainNode& operator=(const ProbeGainNode&) = delete;

    void process(const std::span<const float*> inputs, const std::span<float*> outputs,
                 const size_t numSamples) noexcept override
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            outputs[0][i] = inputs[0][i] * m_gain;
        }
    }

    void setParameter(const size_t paramIndex, const float value) noexcept override
    {
        if (paramIndex == 0)
        {
            m_gain = value;
        }
    }

  private:
    Probe& m_probe;
    float m_gain{1.0f};
};

class SwapperFixture : public ::testing::Test
{
  protected:
    SwapperFixture()
    {
        m_registry.registerType(
            "ProbeGain",
            NodeSchema{{audioInPort("in"), audioOutPort("out")},
                       {ParameterDescriptor{"gain", "linear", 0.0f, 4.0f, 1.0f, ParameterMapping::Linear, 0.0f, true}},
                       false},
            [this](const NodeInstance&, float) { return std::make_unique<ProbeGainNode>(m_probe); });
    }

    /// Two independent channels: out L = inL * gainL, out R = inR * gainR.
    [[nodiscard]] CompiledGraph makeStereo(const float gainL, const float gainR)
    {
        GraphDescription description;
        description.io = {{"inL", "inR"}, {"outL", "outR"}};
        description.nodes = {NodeInstance{.id = "l", .type = "ProbeGain"},
                             NodeInstance{.id = "r", .type = "ProbeGain"}};
        description.nodes[0].params["gain"] = gainL;
        description.nodes[1].params["gain"] = gainR;
        description.edges = {edge("", "inL", "l", "in"), edge("l", "out", "", "outL"), edge("", "inR", "r", "in"),
                             edge("r", "out", "", "outR")};
        auto result = GraphCompiler::compile(description, m_registry, kMaxBlock, kSampleRate);
        EXPECT_TRUE(result.graph.has_value());
        return std::move(*result.graph);
    }

    [[nodiscard]] CompiledGraph makeMono(const float gain)
    {
        GraphDescription description;
        description.io = {{"in"}, {"out"}};
        description.nodes = {NodeInstance{.id = "g", .type = "ProbeGain"}};
        description.nodes[0].params["gain"] = gain;
        description.edges = {edge("", "in", "g", "in"), edge("g", "out", "", "out")};
        auto result = GraphCompiler::compile(description, m_registry, kMaxBlock, kSampleRate);
        EXPECT_TRUE(result.graph.has_value());
        return std::move(*result.graph);
    }

    struct Rendered
    {
        std::vector<float> left;
        std::vector<float> right;
    };

    /// Renders blocks of the given sizes with a constant input of 1 on both channels.
    [[nodiscard]] static Rendered render(GraphSwapper& swapper, const std::vector<size_t>& blockSizes)
    {
        const std::vector<float> ones(kMaxBlock, 1.f);
        Rendered rendered;
        for (const auto numSamples : blockSizes)
        {
            std::vector<float> left(numSamples, 0.f);
            std::vector<float> right(numSamples, 0.f);
            std::array<const float*, 2> ins{ones.data(), ones.data()};
            std::array<float*, 2> outs{left.data(), right.data()};
            swapper.process(ins, outs, numSamples);
            rendered.left.insert(rendered.left.end(), left.begin(), left.end());
            rendered.right.insert(rendered.right.end(), right.begin(), right.end());
        }
        return rendered;
    }

    Probe m_probe;

  private:
    [[nodiscard]] static Edge edge(std::string fromNode, std::string fromPort, std::string toNode, std::string toPort)
    {
        return Edge{.fromNode = std::move(fromNode),
                    .fromPort = std::move(fromPort),
                    .toNode = std::move(toNode),
                    .toPort = std::move(toPort)};
    }

    NodeRegistry m_registry;
};

} // namespace

TEST_F(SwapperFixture, OutputEqualsTheActiveGraphBeforeAnySwap)
{
    GraphSwapper sut{makeStereo(1.5f, 0.25f), kMaxBlock, 100};
    const auto rendered = render(sut, {64, 17});
    for (const auto sample : rendered.left)
    {
        EXPECT_EQ(sample, 1.5f);
    }
    for (const auto sample : rendered.right)
    {
        EXPECT_EQ(sample, 0.25f);
    }
    EXPECT_FALSE(sut.isCrossfading());
}

TEST_F(SwapperFixture, ZeroFadeSwitchesAtTheNextBlockAndRetiresTheOldGraph)
{
    GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 0};
    ASSERT_TRUE(sut.submit(makeStereo(3.f, 2.f)));
    const auto rendered = render(sut, {32});
    for (size_t i = 0; i < 32; ++i)
    {
        EXPECT_EQ(rendered.left[i], 3.f);
        EXPECT_EQ(rendered.right[i], 2.f);
    }
    EXPECT_FALSE(sut.isCrossfading());
    EXPECT_TRUE(sut.collectRetired());
    EXPECT_FALSE(sut.collectRetired());
}

TEST_F(SwapperFixture, FadeFollowsTheEqualPowerCurveAcrossBlocksOfVaryingSize)
{
    constexpr size_t kFade{100};
    GraphSwapper sut{makeStereo(2.f, 0.5f), kMaxBlock, kFade};
    ASSERT_TRUE(sut.submit(makeStereo(0.5f, 2.f)));
    const auto rendered = render(sut, {64, 17, 64, 55});

    EqualPowerCrossfade reference;
    reference.start(kFade);
    for (size_t i = 0; i < rendered.left.size(); ++i)
    {
        const auto gains = reference.step();
        EXPECT_NEAR(rendered.left[i], 2.f * gains.outgoing + 0.5f * gains.incoming, kTolerance) << "left " << i;
        EXPECT_NEAR(rendered.right[i], 0.5f * gains.outgoing + 2.f * gains.incoming, kTolerance) << "right " << i;
    }
}

TEST_F(SwapperFixture, AfterTheFadeOnlyTheNewGraphPlaysAndTheOldOneIsRetired)
{
    GraphSwapper sut{makeStereo(2.f, 2.f), kMaxBlock, 40};
    ASSERT_TRUE(sut.submit(makeStereo(0.5f, 0.75f)));
    static_cast<void>(render(sut, {64}));
    EXPECT_FALSE(sut.isCrossfading());

    const auto after = render(sut, {64});
    for (size_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(after.left[i], 0.5f);
        EXPECT_EQ(after.right[i], 0.75f);
    }
    EXPECT_TRUE(sut.collectRetired());
}

TEST_F(SwapperFixture, GraphWithADifferentShapeIsRefusedAndTheRunningGraphIsUnaffected)
{
    GraphSwapper sut{makeStereo(1.5f, 1.5f), kMaxBlock, 0};
    auto mono = makeMono(4.f);
    EXPECT_FALSE(sut.submit(std::move(mono)));

    const auto rendered = render(sut, {16});
    for (const auto sample : rendered.left)
    {
        EXPECT_EQ(sample, 1.5f);
    }
    EXPECT_FALSE(sut.collectRetired());
}

TEST_F(SwapperFixture, SecondSubmitBeforeAdoptionReplacesTheFirstOnTheControlThread)
{
    GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 0};
    ASSERT_TRUE(sut.submit(makeStereo(3.f, 3.f)));
    ASSERT_EQ(m_probe.destroyed.load(), 0);
    ASSERT_TRUE(sut.submit(makeStereo(4.f, 4.f)));
    EXPECT_EQ(m_probe.destroyed.load(), 2);

    const auto rendered = render(sut, {8});
    for (const auto sample : rendered.left)
    {
        EXPECT_EQ(sample, 4.f);
    }
}

TEST_F(SwapperFixture, SubmitWaitsForARunningFadeAndForTheRetiredGraphToBeCollected)
{
    GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 100};
    ASSERT_TRUE(sut.submit(makeStereo(2.f, 2.f)));
    static_cast<void>(render(sut, {64}));
    ASSERT_TRUE(sut.isCrossfading());
    ASSERT_TRUE(sut.submit(makeStereo(4.f, 4.f)));

    static_cast<void>(render(sut, {64}));
    EXPECT_FALSE(sut.isCrossfading());
    const auto stalled = render(sut, {64});
    EXPECT_FALSE(sut.isCrossfading());
    for (const auto sample : stalled.left)
    {
        EXPECT_EQ(sample, 2.f);
    }

    ASSERT_TRUE(sut.collectRetired());
    const auto fadingIn = render(sut, {64, 64});
    EXPECT_NEAR(fadingIn.left.front(), 2.f, 1E-3f);
    EXPECT_EQ(fadingIn.left.back(), 4.f);
}

TEST_F(SwapperFixture, AudioThreadNeverDestroysAGraph)
{
    {
        GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 50};
        ASSERT_TRUE(sut.submit(makeStereo(2.f, 2.f)));
        std::thread audio{[&]
                          {
                              m_probe.audioThread = std::this_thread::get_id();
                              static_cast<void>(render(sut, {64, 64, 64}));
                          }};
        audio.join();
        EXPECT_EQ(m_probe.destroyed.load(), 0);
        EXPECT_TRUE(sut.collectRetired());
        EXPECT_EQ(m_probe.destroyed.load(), 2);
    }
    EXPECT_EQ(m_probe.destroyedOnAudioThread.load(), 0);
    EXPECT_EQ(m_probe.created.load(), m_probe.destroyed.load());
}

TEST_F(SwapperFixture, ProcessDoesNotAllocateOrFreeWhileAdoptingFadingAndRetiring)
{
    GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 100};
    ASSERT_TRUE(sut.submit(makeStereo(2.f, 2.f)));

    const std::vector<float> ones(kMaxBlock, 1.f);
    std::array<float, kMaxBlock> left{};
    std::array<float, kMaxBlock> right{};
    std::array<const float*, 2> ins{ones.data(), ones.data()};
    std::array<float*, 2> outs{left.data(), right.data()};

    tAllocations = 0;
    tDeallocations = 0;
    tCountAllocations = true;
    for (int block = 0; block < 4; ++block)
    {
        sut.process(ins, outs, kMaxBlock);
    }
    tCountAllocations = false;

    EXPECT_EQ(tAllocations, 0u);
    EXPECT_EQ(tDeallocations, 0u);
    EXPECT_FALSE(sut.isCrossfading());
}

TEST_F(SwapperFixture, ConcurrentSubmitsKeepTheOutputBoundedAndLeakNothing)
{
    constexpr int kSubmissions{300};
    constexpr float kMaxGain{2.5f};
    std::atomic<bool> stop{false};
    std::atomic<bool> outOfBounds{false};
    {
        GraphSwapper sut{makeStereo(1.f, 1.f), kMaxBlock, 80};
        std::thread audio{[&]
                          {
                              m_probe.audioThread = std::this_thread::get_id();
                              const std::vector<float> ones(kMaxBlock, 1.f);
                              std::array<float, kMaxBlock> left{};
                              std::array<float, kMaxBlock> right{};
                              std::array<const float*, 2> ins{ones.data(), ones.data()};
                              std::array<float*, 2> outs{left.data(), right.data()};
                              while (!stop.load())
                              {
                                  sut.process(ins, outs, kMaxBlock);
                                  for (size_t i = 0; i < kMaxBlock; ++i)
                                  {
                                      const auto bound = std::sqrt(2.f) * kMaxGain + 1E-3f;
                                      if (!std::isfinite(left[i]) || left[i] < 0.f || left[i] > bound)
                                      {
                                          outOfBounds = true;
                                      }
                                  }
                              }
                          }};

        for (int i = 0; i < kSubmissions; ++i)
        {
            const auto gain = 0.5f + static_cast<float>(i % 3);
            EXPECT_TRUE(sut.submit(makeStereo(gain, gain)));
            static_cast<void>(sut.collectRetired());
            std::this_thread::yield();
        }
        stop = true;
        audio.join();
    }
    EXPECT_FALSE(outOfBounds.load());
    EXPECT_EQ(m_probe.destroyedOnAudioThread.load(), 0);
    EXPECT_EQ(m_probe.created.load(), m_probe.destroyed.load());
}

}
