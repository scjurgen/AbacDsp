#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "GraphDescription.h"

namespace AbacDsp::Graph
{

enum class ParameterMapping
{
    Linear,
    Log,
    Exp
};

struct ParameterDescriptor
{
    std::string id;
    std::string unit;
    float minValue{0.0f};
    float maxValue{1.0f};
    float defaultValue{0.0f};
    ParameterMapping mapping{ParameterMapping::Linear};
    float smoothingMs{0.0f};
    bool automatable{true};
};

/**
 * @ingroup graph
 * @brief Static per-node-type description: ports, parameters, cycle-breaking.
 *
 * One NodeSchema exists per type name registered in NodeRegistry, not per
 * node instance. A ParameterDescriptor's position in parameters is the
 * paramIndex a compiled parameter target resolves to and passes to
 * Node::setParameter() - that index is the "bound setter", so binding a
 * parameter target costs nothing at runtime.
 */
struct NodeSchema
{
    std::vector<PortDescriptor> ports;
    std::vector<ParameterDescriptor> parameters;
    bool breaksCycle{false};

    [[nodiscard]] const PortDescriptor* findPort(const std::string_view name) const noexcept
    {
        for (const auto& port : ports)
        {
            if (port.name == name)
            {
                return &port;
            }
        }
        return nullptr;
    }

    [[nodiscard]] int findParameterIndex(const std::string_view id) const noexcept
    {
        for (size_t i = 0; i < parameters.size(); ++i)
        {
            if (parameters[i].id == id)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
};

}
