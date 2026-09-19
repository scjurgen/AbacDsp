#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "Graph/NodeRegistry.h"
#include "GraphTestNodes.h"

namespace AbacDsp::Graph::Test
{

TEST(NodeRegistryTest, TypeNamesListEveryRegisteredTypeInAlphabeticalOrder)
{
    NodeRegistry registry;
    EXPECT_TRUE(registry.typeNames().empty());

    registerTestNodes(registry);
    const auto names = registry.typeNames();
    EXPECT_TRUE(std::ranges::is_sorted(names));
    for (const std::string name : {"PassThroughStub", "GainStub", "SumStub", "ConstantStub"})
    {
        EXPECT_NE(std::ranges::find(names, name), names.end()) << name;
        EXPECT_NE(registry.findSchema(name), nullptr) << name;
    }
}

}
