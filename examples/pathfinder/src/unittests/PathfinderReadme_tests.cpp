#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "impl/PathfinderImpl.h"

namespace
{

constexpr size_t BlockSize{16};
constexpr float SampleRate{48000.f};

using Impl = PathfinderImpl<BlockSize>;

[[nodiscard]] std::string readReadme()
{
    std::ifstream file(PATHFINDER_README_PATH);
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// The text between the two node reference markers, without the line breaks that frame it.
[[nodiscard]] std::string nodeReferenceInReadme(const std::string& readme)
{
    const std::string begin = "<!-- NODE-REFERENCE:BEGIN -->\n";
    const std::string end = "<!-- NODE-REFERENCE:END -->";
    const auto start = readme.find(begin);
    const auto stop = readme.find(end);
    if (start == std::string::npos || stop == std::string::npos || stop < start)
    {
        return {};
    }
    return readme.substr(start + begin.size(), stop - start - begin.size());
}

// Every block fenced as lua (a complete script); a fragment is fenced lua-fragment and not run.
[[nodiscard]] std::vector<std::string> luaExamples(const std::string& readme)
{
    const std::string open = "```lua\n";
    const std::string close = "```";
    std::vector<std::string> examples;
    for (auto at = readme.find(open); at != std::string::npos; at = readme.find(open, at))
    {
        const auto bodyStart = at + open.size();
        const auto bodyEnd = readme.find(close, bodyStart);
        examples.push_back(readme.substr(bodyStart, bodyEnd - bodyStart));
        at = bodyEnd;
    }
    return examples;
}

} // namespace

TEST(PathfinderReadmeTest, HasAScriptingSection)
{
    EXPECT_NE(readReadme().find("\n## Scripting\n"), std::string::npos);
}

TEST(PathfinderReadmeTest, NodeReferenceMatchesTheNodesTheEngineRegisters)
{
    const AbacDsp::Graph::MacroBank bank;
    const Impl::Engine engine{bank, SampleRate};
    const std::string inReadme = nodeReferenceInReadme(readReadme());
    ASSERT_FALSE(inReadme.empty()) << "the README needs the NODE-REFERENCE markers";
    EXPECT_EQ(inReadme, engine.nodeReferenceMarkdown()) << "regenerate with dev-scripts/dev-pathfinder-docs.sh";
}

TEST(PathfinderReadmeTest, EveryCompleteLuaExampleApplies)
{
    const auto examples = luaExamples(readReadme());
    ASSERT_GE(examples.size(), 3u);
    for (const auto& example : examples)
    {
        Impl impl{SampleRate};
        EXPECT_TRUE(impl.setScript(example)) << impl.scriptError() << "\n" << example;
    }
}
