#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "GraphDescription.h"
#include "Node.h"
#include "NodeSchema.h"

namespace AbacDsp::Graph
{

using NodeFactory = std::function<std::unique_ptr<Node>(const NodeInstance&)>;

/**
 * @ingroup graph
 * @brief Maps a node type name to its NodeSchema and construction factory.
 *
 * Registration is an off-audio-thread setup step: it allocates and builds
 * std::string keys freely. Nothing here is touched again once GraphCompiler
 * has resolved a GraphDescription against it.
 */
class NodeRegistry
{
  public:
    void registerType(std::string typeName, NodeSchema schema, NodeFactory factory)
    {
        m_entries.emplace(std::move(typeName), Entry{std::move(schema), std::move(factory)});
    }

    [[nodiscard]] const NodeSchema* findSchema(const std::string& typeName) const noexcept
    {
        const auto it = m_entries.find(typeName);
        return it == m_entries.end() ? nullptr : &it->second.schema;
    }

    [[nodiscard]] std::unique_ptr<Node> create(const std::string& typeName, const NodeInstance& instance) const
    {
        const auto it = m_entries.find(typeName);
        return it == m_entries.end() ? nullptr : it->second.factory(instance);
    }

  private:
    struct Entry
    {
        NodeSchema schema;
        NodeFactory factory;
    };

    std::map<std::string, Entry> m_entries;
};

}
