// Prints the node reference table that examples/pathfinder/README.md embeds. Run through
// dev-scripts/dev-pathfinder-docs.sh, which also splices it into the README.
#include <cstdio>

#include "Graph/MacroBank.h"
#include "impl/PathfinderScriptEngine.h"

int main()
{
    const AbacDsp::Graph::MacroBank bank;
    const PathfinderScriptEngine<16> engine{bank, 48000.f};
    std::fputs(engine.nodeReferenceMarkdown().c_str(), stdout);
    return 0;
}
