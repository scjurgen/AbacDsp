from typing import Any

from blueprint import Blueprint


def create_patch_changed(blueprint: Blueprint) -> str:
    conditions = []
    for item in blueprint["ports-control"]:
        if "patch" in item:
            conditions.append(f'''parameterID == "{item['symbol']}"''')
    return " || ".join(conditions)

def get_patch_count(blueprint: Blueprint) -> int:
    idx = 0
    for item in blueprint["ports-control"]:
        if "patch" in item:
            idx = idx + 1
    return idx

def create_patch_index_assign(blueprint: Blueprint) -> str:
    result = ""
    idx = 0
    for item in blueprint["ports-control"]:
        if "patch" in item:
            result += (f''' if (parameterID == "{item['symbol']}") {{
                const int newIdx = static_cast<int>(newValue);
                if (m_patchIndex[{idx}] != newIdx) {{ m_patchIndex[{idx}] = newIdx; patchIndexChanged = true; }}
            }} ''')
            idx = idx + 1
    return result

def create_parameter_changed(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            match item["type"]:
                case 'dial':
                    result += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(v); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
                case 'drop':
                    cast_type = 'int' if item.get('signed', True) else 'size_t'
                    result += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<{cast_type}>(v)); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
                case 'switch':
                     result += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<bool>(v)); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
    return result


def update_param_by_id(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            param_id = item['symbol']
            cast_value = None
            match item['type']:
                case "dial":
                    cast_value = f"value"
                case "switch":
                    cast_value = f"""static_cast<bool>(value) """
                case "drop":
                    cast_value = f"""static_cast<size_t>(value) """
            if cast_value is not None:
                result += f""" case Id::{param_id}: if (!isEqual(get<Id::{param_id}>(), value)) {{get<Id::{param_id}>() = {cast_value};m_modified = true;}}\nbreak;\n"""
    return result

def id_list(blueprint: Blueprint) -> str:
    items = [item for item in blueprint["ports-control"] if 'patch' not in item and item['type'] in ["dial", "switch", "drop"]]
    if not items:
        return ""

    max_len = max(len(item['symbol']) for item in items)
    lines = [f"        {item['symbol']:<{max_len}}, // {item['type']}" for item in items[:-1]]
    if items:
        lines.append(f"        {items[-1]['symbol']:<{max_len}} // {items[-1]['type']}")
    return "\n".join(lines)

def id_string_list(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial","switch","drop"]:
                result += f""""{item['symbol']}",\n"""
    return result[:-2]

def param_const_expr_list(blueprint: Blueprint) -> str:
    items= []
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial","switch","drop"]:
                items.append(f"""if constexpr (ParamId == Id::{item['symbol']}) return {item['symbol']};\n""")
    return "        else ".join(items)

def load_patches(blueprint: Blueprint) -> str:
    result =""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial", "switch", "drop"]:
                result += f"""     if (auto* p = m_parameters.getParameter("{item['symbol']}"))
                        {{
                            const auto& range = m_parameters.getParameterRange("{item['symbol']}");
                            float normalized = range.convertTo0to1(params.{item['symbol']});
                            p->setValueNotifyingHost(normalized);
                        }}
                """
    return result

def create_setters_implementation(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            setter = item['setter']
            match item['type']:
                case "dial":
                    if item['unit'] in ['dB','DB','db']:
                        result += f"void {setter}(const float value){{ m_{symbol} = std::pow(10.f,value/20.f);}}\n"
                    else:
                        result += f"void {setter}(const float value){{ m_{symbol} = value;}}\n"
                case "switch":
                    result += f"void {setter}(const bool value){{ m_{symbol} = value;}}\n"
                case "drop":
                    result += f"void {setter}(const size_t value){{ m_{symbol} = value;}}\n"
    return result

def create_struct_variables_implementation(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            default = item.get('default', 0)
            match item['type']:
                case "dial":
                    result += f"float {symbol}{{{float(default)}f}};\n"
                case "switch":
                    result += f"bool {symbol}{{{'true' if default else 'false'}}};\n"
                case "drop":
                    result += f"size_t {symbol}{{{default}}};\n"
    return result

def create_variables_implementation(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            match item['type']:
                case "dial":
                    result += f"float m_{symbol}{{}};\n"
                case "switch":
                    result += f"bool m_{symbol}{{}};\n"
                case "drop":
                    result += f"size_t m_{symbol}{{}};\n"
    return result

# Types safe to default without construction/allocation, hence eligible for noexcept.
NOEXCEPT_FORWARD_TYPES = {"bool", "int", "float", "double", "size_t", "uint32_t", "int64_t"}

def default_forward_value(cpp_type: str, override: Any) -> str:
    if override is not None:
        return str(override)
    match cpp_type:
        case "bool":
            return "false"
        case "float" | "double":
            return "0.f" if cpp_type == "float" else "0.0"
        case "size_t" | "uint32_t":
            return "0u"
        case "int" | "int64_t":
            return "0"
        case _:
            return f"{cpp_type}{{}}"

def create_processor_forwards(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint.get("processor_forwards", []):
        name = item["name"]
        call = item.get("call", name)
        cpp_type = item["type"]
        default = default_forward_value(cpp_type, item.get("default"))
        is_noexcept = item["noexcept"] if "noexcept" in item else cpp_type in NOEXCEPT_FORWARD_TYPES
        noexcept_kw = " noexcept" if is_noexcept else ""
        if cpp_type == "bool" and "default" not in item:
            body = f"pluginRunner && pluginRunner->{call}()"
        else:
            body = f"pluginRunner ? pluginRunner->{call}() : {default}"
        result += f"[[nodiscard]] {cpp_type} {name}() const{noexcept_kw} {{ return {body}; }}\n"
    return result

def create_extra_processor_methods(blueprint: Blueprint) -> str:
    result = create_processor_forwards(blueprint)
    methods = blueprint.get("extra_processor_methods", [])
    if methods:
        result += "\n".join(methods) + "\n"
    return result

# Runs once in prepareToPlay(), right after pluginRunner is constructed. Empty
# by default, so blueprints that don't set it get byte-identical output.
def create_extra_prepare_calls(blueprint: Blueprint) -> str:
    calls = blueprint.get("extra_prepare_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")

# Extra private member declarations, right after the pluginRunner unique_ptr.
def create_extra_processor_members(blueprint: Blueprint) -> str:
    members = blueprint.get("extra_processor_members", [])
    return "\n".join(members) + ("\n" if members else "")

# Runs in getStateInformation(), after the parameter XML is built but before
# it's serialized to destData.
def create_extra_get_state_calls(blueprint: Blueprint) -> str:
    calls = blueprint.get("extra_get_state_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")

# Runs in setStateInformation(), inside the parsed-parameter-XML branch.
def create_extra_set_state_calls(blueprint: Blueprint) -> str:
    calls = blueprint.get("extra_set_state_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")


def add_parameter_listeners(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] in ["dial", "drop", "switch"]:
            result += f"""m_parameters.addParameterListener("{item['symbol']}", this);\n"""
    return result

def remove_parameter_listeners(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] in ["dial", "drop", "switch"]:
            result += f"""m_parameters.removeParameterListener("{item['symbol']}", this);\n"""
    return result


def create_parameter_layout(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        param_id = item['symbol']
        param_name = item['display']
        param_default = item['default']

        match item['type']:
            case 'dial':
                precision = item['precision']
                ln = f"""std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("{param_id}", 1), juce::String::fromUTF8("{param_name}"),
                    juce::NormalisableRange<float>({item['rangeStart']}, {item['rangeEnd']}, {item['intervalValue']}, {item['skewFactor']}, {item['useSymmetricSkew']}),
                    {param_default},
                    juce::AudioParameterFloatAttributes{{}}
                        .withLabel("{item['unit']}")
                        .withStringFromValueFunction([](float value, int) {{ return juce::String(value, {precision}) + " {item['unit']}"; }}))"""
                result += f"params.push_back({ln});\n"
            case 'switch':
                ln = f"""std::make_unique<juce::AudioParameterBool>(juce::ParameterID("{param_id}",1), juce::String::fromUTF8("{param_name}"), {param_default})"""
                result += f"params.push_back({ln});\n"
            case 'drop':
                if isinstance(item['listitems'], list):
                    menu_list = 'juce::StringArray {' + ','.join(f'juce::String::fromUTF8("{i}")' for i in item['listitems']) + '}'
                else:
                    menu_list = 'juce::StringArray {' + ','.join(
                        f'juce::String::fromUTF8("{item["listitems"].format(i+1)}")' for i in range(item['count'])
                    ) + '}'
                ln = f"""std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("{param_id}",1), juce::String::fromUTF8("{item["display"]}"), {menu_list}, {param_default})"""
                result += f"params.push_back({ln});\n"
            case 'gauge' | 'label':
                pass
    return result

# Switches map like sustain/damper pedals (CC64-66): the runtime scales the 0..127
# CC value across the parameter's 0..1 range, so an AudioParameterBool flips at the
# 63/64 split automatically. No runtime change is needed beyond listing them here.
def cc_enabled_controls(blueprint: Blueprint) -> list[dict[str, Any]]:
    return [item for item in blueprint["ports-control"] if item['type'] in ('dial', 'switch') and 'cc' in item]

def create_cc_mapping(blueprint: Blueprint) -> dict[str, str]:
    items = cc_enabled_controls(blueprint)
    return {
        "CC_TARGET_ENUM_LIST": ", ".join(item['symbol'] for item in items),
        "CC_DEFAULT_MAPPINGS": "\n".join(
            f'{{{item["cc"]["controller"]}, {float(item["cc"]["valueLow"])}f, {float(item["cc"]["valueHigh"])}f}},'
            for item in items
        ),
        "CC_TARGET_PARAMID_LIST": "\n".join(f'"{item["symbol"]}",' for item in items),
        "CC_TARGET_FULL_RANGE": "\n".join(
            f'{{{float(item["rangeStart"])}f, {float(item["rangeEnd"])}f}},' for item in items
        ),
        "NUM_CC_TARGETS": str(len(items)),
    }
