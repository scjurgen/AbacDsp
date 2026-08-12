#pragma once

#include <vector>

#include "../inc/LuaControlArea.h"
#include "DroneScriptEngine.h"

// Hand-written, dronesequencer-only glue between LuaControlArea (a shared, generic
// template) and DroneScriptEngine's types. Not in CPP_SOURCE_FILES_FIXED, so
// regeneration never touches it.

[[nodiscard]] inline LuaControlType toLuaControlType(const LuaUiParamType type)
{
    switch (type)
    {
        case LuaUiParamType::Knob:
            return LuaControlType::Knob;
        case LuaUiParamType::Drop:
            return LuaControlType::Drop;
        case LuaUiParamType::Switch:
            return LuaControlType::Switch;
    }
    return LuaControlType::Knob;
}

[[nodiscard]] inline std::vector<LuaControlDescriptor> toLuaControlDescriptors(
    const DroneScriptEngine::UiParamSlots& slots)
{
    std::vector<LuaControlDescriptor> result{};
    for (size_t i = 0; i < slots.size(); ++i)
    {
        const auto& slot = slots[i];
        if (!slot.claimed)
        {
            continue;
        }
        LuaControlDescriptor descriptor{};
        descriptor.parameterId = "luaParam" + std::to_string(i + 1);
        descriptor.name = slot.name;
        descriptor.type = toLuaControlType(slot.type);
        descriptor.rangeMin = slot.rangeMin;
        descriptor.rangeMax = slot.rangeMax;
        descriptor.rangeStep = slot.rangeStep;
        descriptor.rangeSkew = slot.rangeSkew;
        descriptor.description = slot.description;
        descriptor.unit = slot.unit;
        descriptor.items = slot.items;
        result.push_back(std::move(descriptor));
    }
    return result;
}
