def createPatchChanged(m:dict) -> str:
    res=[]
    for item in m["ports-control"]:
        if "patch" in item:
            res.append(f'''parameterID == "{item['symbol']}"''')
    return " || ".join(res)

def getPatchCount(m:dict) -> int:
    idx = 0
    for item in m["ports-control"]:
        if "patch" in item:
            idx = idx + 1
    return idx

def createPatchIndexAssign(m:dict) -> str:
    res = ""
    idx = 0
    for item in m["ports-control"]:
        if "patch" in item:
            res += (f''' if (parameterID == "{item['symbol']}") {{
                const int newIdx = static_cast<int>(newValue);
                if (m_patchIndex[{idx}] != newIdx) {{ m_patchIndex[{idx}] = newIdx; patchIndexChanged = true; }}
            }} ''')
            idx = idx + 1
    return res

def createParameterChanged(m:dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            match item["type"]:
                case 'dial':
                    res += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(v); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
                case 'drop':
                    castType = 'int' if item.get('signed', True) else 'size_t'
                    res += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<{castType}>(v)); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
                case 'switch':
                     res += f"""{{"{symbol}", [](AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<bool>(v)); p.m_fileIo.updateParameter(PatchParameters::Id::{symbol}, v); }}}},\n"""
    return res


def updateParamById(m:dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            id = item['symbol']
            castValue = None
            match item['type']:
                case "dial":
                    castValue = f"value"
                case "switch":
                    castValue = f"""static_cast<bool>(value) """
                case "drop":
                    castValue = f"""static_cast<size_t>(value) """
            if castValue is not None:
                res += f""" case Id::{id}: if (!isEqual(get<Id::{id}>(), value)) {{get<Id::{id}>() = {castValue};m_modified = true;}}\nbreak;\n"""
    return res

def idList(m: dict) -> str:
    items = [item for item in m["ports-control"] if 'patch' not in item and item['type'] in ["dial", "switch", "drop"]]
    if not items:
        return ""

    max_len = max(len(item['symbol']) for item in items)
    lines = [f"        {item['symbol']:<{max_len}}, // {item['type']}" for item in items[:-1]]
    if items:
        lines.append(f"        {items[-1]['symbol']:<{max_len}} // {items[-1]['type']}")
    return "\n".join(lines)

def idStringList(m:dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial","switch","drop"]:
                res += f""""{item['symbol']}",\n"""
    return res[:-2]

def paramConstExprList(m:dict) -> str:
    items= []
    for item in m["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial","switch","drop"]:
                items.append(f"""if constexpr (ParamId == Id::{item['symbol']}) return {item['symbol']};\n""")
    return "        else ".join(items)

def loadPatches(m:dict) -> str:
    res =""
    for item in m["ports-control"]:
        if 'patch' not in item:
            if item['type'] in ["dial", "switch", "drop"]:
                res += f"""     if (auto* p = m_parameters.getParameter("{item['symbol']}"))
                        {{
                            const auto& range = m_parameters.getParameterRange("{item['symbol']}");
                            float normalized = range.convertTo0to1(params.{item['symbol']});
                            p->setValueNotifyingHost(normalized);
                        }}
                """
    return res

def createSettersImplementation(m:dict):
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            setter = item['setter']
            match item['type']:
                case "dial":
                    if item['unit'] in ['dB','DB','db']:
                        res += f"void {setter}(const float value){{ m_{symbol} = std::pow(10.f,value/20.f);}}\n"
                    else:
                        res += f"void {setter}(const float value){{ m_{symbol} = value;}}\n"
                case "switch":
                    res += f"void {setter}(const bool value){{ m_{symbol} = value;}}\n"
                case "drop":
                    res += f"void {setter}(const size_t value){{ m_{symbol} = value;}}\n"
    return res

def createtructVariablesImplementation(m:dict):
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            default = item.get('default', 0)
            match item['type']:
                case "dial":
                    res += f"float {symbol}{{{float(default)}f}};\n"
                case "switch":
                    res += f"bool {symbol}{{{'true' if default else 'false'}}};\n"
                case "drop":
                    res += f"size_t {symbol}{{{default}}};\n"
    return res
def createVariablesImplementation(m:dict):
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            symbol = item['symbol']
            match item['type']:
                case "dial":
                    res += f"float m_{symbol}{{}};\n"
                case "switch":
                    res += f"bool m_{symbol}{{}};\n"
                case "drop":
                    res += f"size_t m_{symbol}{{}};\n"
    return res

# Types safe to default without construction/allocation, hence eligible for noexcept.
noexceptForwardTypes = {"bool", "int", "float", "double", "size_t", "uint32_t", "int64_t"}

def defaultForwardValue(cpp_type: str, override) -> str:
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

def createProcessorForwards(m: dict) -> str:
    res = ""
    for item in m.get("processor_forwards", []):
        name = item["name"]
        call = item.get("call", name)
        cpp_type = item["type"]
        default = defaultForwardValue(cpp_type, item.get("default"))
        is_noexcept = item["noexcept"] if "noexcept" in item else cpp_type in noexceptForwardTypes
        noexcept_kw = " noexcept" if is_noexcept else ""
        if cpp_type == "bool" and "default" not in item:
            body = f"pluginRunner && pluginRunner->{call}()"
        else:
            body = f"pluginRunner ? pluginRunner->{call}() : {default}"
        res += f"[[nodiscard]] {cpp_type} {name}() const{noexcept_kw} {{ return {body}; }}\n"
    return res

def createExtraProcessorMethods(m: dict) -> str:
    res = createProcessorForwards(m)
    methods = m.get("extra_processor_methods", [])
    if methods:
        res += "\n".join(methods) + "\n"
    return res

# Runs once in prepareToPlay(), right after pluginRunner is constructed. Empty
# by default, so blueprints that don't set it get byte-identical output.
def createExtraPrepareCalls(m: dict) -> str:
    calls = m.get("extra_prepare_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")

# Extra private member declarations, right after the pluginRunner unique_ptr.
def createExtraProcessorMembers(m: dict) -> str:
    members = m.get("extra_processor_members", [])
    return "\n".join(members) + ("\n" if members else "")

# Runs in getStateInformation(), after the parameter XML is built but before
# it's serialized to destData.
def createExtraGetStateCalls(m: dict) -> str:
    calls = m.get("extra_get_state_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")

# Runs in setStateInformation(), inside the parsed-parameter-XML branch.
def createExtraSetStateCalls(m: dict) -> str:
    calls = m.get("extra_set_state_calls", [])
    return "\n".join(calls) + ("\n" if calls else "")


def addParameterListeners(m:dict):
    res = ""
    for item in m["ports-control"]:
        if item['type'] in ["dial", "drop", "switch"]:
            res += f"""m_parameters.addParameterListener("{item['symbol']}", this);\n"""
    return res

def removeParameterListeners(m:dict):
    res = ""
    for item in m["ports-control"]:
        if item['type'] in ["dial", "drop", "switch"]:
            res += f"""m_parameters.removeParameterListener("{item['symbol']}", this);\n"""
    return res


def createParameterLayout(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        paramId = item['symbol']
        paramName = item['display']
        paramDefault = item['default']

        match item['type']:
            case 'dial':
                precision = item['precision']
                ln = f"""std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("{paramId}", 1), "{paramName}",
                    juce::NormalisableRange<float>({item['rangeStart']}, {item['rangeEnd']}, {item['intervalValue']}, {item['skewFactor']}, {item['useSymmetricSkew']}),
                    {paramDefault},
                    juce::AudioParameterFloatAttributes{{}}
                        .withLabel("{item['unit']}")
                        .withStringFromValueFunction([](float value, int) {{ return juce::String(value, {precision}) + " {item['unit']}"; }}))"""
                res += f"params.push_back({ln});\n"
            case 'switch':
                ln = f"""std::make_unique<juce::AudioParameterBool>(juce::ParameterID("{paramId}",1), "{paramName}", {paramDefault})"""
                res += f"params.push_back({ln});\n"
            case 'drop':
                if isinstance(item['listitems'], list):
                    menuList = 'juce::StringArray {' + ','.join(f'"{i}"' for i in item['listitems']) + '}'
                else:
                    menuList = 'juce::StringArray {' + ','.join(
                        f'"{item["listitems"].format(i+1)}"' for i in range(item['count'])
                    ) + '}'
                ln = f"""std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("{paramId}",1), "{item["display"]}", {menuList}, {paramDefault})"""
                res += f"params.push_back({ln});\n"
            case 'gauge' | 'label':
                pass
    return res

# Switches map like sustain/damper pedals (CC64-66): the runtime scales the 0..127
# CC value across the parameter's 0..1 range, so an AudioParameterBool flips at the
# 63/64 split automatically. No runtime change is needed beyond listing them here.
def cc_enabled_controls(m: dict) -> list:
    return [item for item in m["ports-control"] if item['type'] in ('dial', 'switch') and 'cc' in item]

def createCcMapping(m: dict) -> dict:
    items = cc_enabled_controls(m)
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
