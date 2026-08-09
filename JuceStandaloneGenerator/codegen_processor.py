import re
from typing import Any

from blueprint import Blueprint


def has_script_port(blueprint: Blueprint) -> bool:
    return any(item['type'] == 'script' for item in blueprint["ports-control"])

# Body text for the Settings > About > License Info dialog, shared verbatim by every
# blueprint. \n\n here is a literal two-character escape landing inside a C++ string
# literal in the template, not a real newline - see showAboutDialog() in the Editor
# template for where this gets concatenated in.
def create_about_text(blueprint: Blueprint) -> str:
    description = blueprint.get("description", "").replace("\n", " ").strip()
    # Several blueprint descriptions still carry a leading placeholder emoji
    # (e.g. "✨ Tanpura drone synth..."); strip it rather than surface
    # non-ASCII in this dialog's text.
    description = re.sub(r'^[^\x00-\x7f]+\s*', '', description)
    lines = [description] if description else []
    lines.append("Part of the AbacDsp project - core DSP library is MIT licensed.")
    lines.append("Built with JUCE, licensed under AGPLv3 (or a commercial JUCE licence).")
    if has_script_port(blueprint):
        lines.append("Scripting powered by Lua and sol2 (both MIT licensed).")
    lines.append("Full third-party license details: THIRD-PARTY-LICENSES.md in the AbacDsp repository.")
    return "\\n\\n".join(lines)


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
            elif item['type'] == 'script':
                # No float-keyed update path for a string field (see update{Symbol}()
                # instead) - explicit no-op case rather than relying on the switch's
                # default:, since -Wswitch-enum flags any Id left unhandled either way.
                result += f""" case Id::{param_id}: break;\n"""
    return result

# "script" is included here (unlike the dial/switch/drop-only helpers below) because
# this same generated list feeds both the "enum class Id" body and, verbatim, the
# nlohmann serialization macro's field list further down in the template - leaving it
# out would silently drop script text from patch JSON entirely.
def id_list(blueprint: Blueprint) -> str:
    items = [item for item in blueprint["ports-control"] if 'patch' not in item and item['type'] in ["dial", "switch", "drop", "script"]]
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
            if item['type'] in ["dial","switch","drop","script"]:
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
                case "script":
                    # Empty by default (rather than duplicating the engine's stub script text
                    # here) - a never-saved patch leaves whatever the DSP impl already loaded
                    # at construction untouched; see create_load_script_calls().
                    result += f'std::string {symbol}{{}};\n'
    return result

# Per-symbol "script" update methods for PatchParameters, bypassing the float-keyed
# updateById() switch entirely - there's no APVTS parameter for a text blob to drive it.
def create_patch_parameters_script_methods(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] == 'script':
            symbol = item['symbol']
            upper = symbol[0].upper() + symbol[1:]
            result += (f"""void update{upper}(const std::string& value) {{ """
                       f"""if ({symbol} != value) {{ {symbol} = value; m_modified = true; }} }}\n""")
    return result

# Pushed into applyLoadedParametersToHost() after a patch load: an empty stored script
# means "this patch never touched it", so the engine's own already-loaded script (its
# stub, or whatever a prior loadScript() call left running) is left alone. Guarded on
# pluginRunner since a named-patch load (unlike the numbered-slot path, which only runs
# from parameterChanged() after pluginRunner already exists) can run before prepareToPlay().
def create_load_script_calls(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] == 'script':
            symbol = item['symbol']
            upper = symbol[0].upper() + symbol[1:]
            result += (f"""if (pluginRunner != nullptr && !params.{symbol}.empty()) """
                       f"""{{ pluginRunner->set{upper}(params.{symbol}); }}\n""")
    return result

# Pushed into prepareToPlay() right after pluginRunner is (re)constructed: without this,
# a freshly built pluginRunner starts with the DSP impl's own default script rather than
# whatever FileIo already loaded at construction, until the next patch load or manual
# script-editor Apply overwrites it. Mirrors create_load_script_calls() above, sourced
# from FileIo directly since there is no local "params" at this point.
def create_prepare_script_calls(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] == 'script':
            symbol = item['symbol']
            upper = symbol[0].upper() + symbol[1:]
            result += (f"""if (!m_fileIo.current{upper}().empty()) """
                       f"""{{ pluginRunner->set{upper}(m_fileIo.current{upper}()); }}\n""")
    return result

# FileIo public API for a "script"-type port: update/current mirror updateParameter()'s
# shape but for the string field directly; the rest is a named-item pool (list/save/
# load/delete/rename) structurally identical to the named-patch pool above it, just
# storing plain-text .lua files instead of JSON patch snapshots.
def create_fileio_script_methods(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] != 'script':
            continue
        symbol = item['symbol']
        upper = symbol[0].upper() + symbol[1:]
        result += f"""
    void update{upper}(const std::string& value)
    {{
        if (!m_isInitialized)
        {{
            return;
        }}
        m_currentParams.update{upper}(value);
    }}

    [[nodiscard]] const std::string& current{upper}() const
    {{
        return m_currentParams.{symbol};
    }}

    [[nodiscard]] std::vector<std::string> list{upper}Names() const
    {{
        std::vector<std::string> names;
        const auto rootDir = get{upper}Directory();
        for (const auto& f : rootDir.findChildFiles(juce::File::findFiles, true, "*.lua"))
        {{
            const auto relative = f.getRelativePathFrom(rootDir).replaceCharacter('\\\\', '/');
            names.push_back(relative.upToLastOccurrenceOf(".lua", false, false).toStdString());
        }}
        std::sort(names.begin(), names.end());
        return names;
    }}

    [[nodiscard]] const std::string& current{upper}Name() const
    {{
        return m_current{upper}Name;
    }}

    bool save{upper}Named(const std::string& name)
    {{
        const std::string filename = get{upper}Filename(name);
        if (filename.empty())
        {{
            return false;
        }}
        std::ofstream out(filename);
        if (!out)
        {{
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for writing" << std::endl;
            return false;
        }}
        out << m_currentParams.{symbol};
        m_current{upper}Name = name;
        return true;
    }}

    bool load{upper}Named(const std::string& name)
    {{
        const std::string filename = get{upper}Filename(name);
        std::ifstream in(filename);
        if (filename.empty() || !in)
        {{
            std::cerr << "FileIo: ERROR - Failed to open " << filename << " for reading" << std::endl;
            return false;
        }}
        std::ostringstream buffer;
        buffer << in.rdbuf();
        update{upper}(buffer.str());
        m_current{upper}Name = name;
        return true;
    }}

    bool delete{upper}Named(const std::string& name)
    {{
        const std::string filename = get{upper}Filename(name);
        if (filename.empty())
        {{
            return false;
        }}
        if (name == m_current{upper}Name)
        {{
            m_current{upper}Name.clear();
        }}
        return juce::File(filename).deleteFile();
    }}

    bool rename{upper}Named(const std::string& oldName, const std::string& newName)
    {{
        const std::string oldFilename = get{upper}Filename(oldName);
        const std::string newFilename = get{upper}Filename(newName);
        if (oldFilename.empty() || newFilename.empty())
        {{
            return false;
        }}
        if (!juce::File(oldFilename).moveFileTo(juce::File(newFilename)))
        {{
            return false;
        }}
        if (oldName == m_current{upper}Name)
        {{
            m_current{upper}Name = newName;
        }}
        return true;
    }}
"""
    return result

# Private helpers backing create_fileio_script_methods() above: the on-disk pool
# directory (a "<symbol>Scripts" sibling of the patch directory) and its filename
# sanitizing/subfolder logic, copied from getPatchDirectory()/getNamedPatchFilename().
def create_fileio_script_private(blueprint: Blueprint) -> str:
    result = ""
    module_upper = blueprint["CPP"]["MODULE_UPPER"]
    for item in blueprint["ports-control"]:
        if item['type'] != 'script':
            continue
        symbol = item['symbol']
        upper = symbol[0].upper() + symbol[1:]
        # Not embedding "/*MODULE_UPPER*/" here for the templating engine to resolve:
        # substitution runs once, in CPP_JUCE_FILE_VARS order, and MODULE_UPPER (near
        # the top of that list) would run before FileIoScriptPrivate (appended at the
        # end) has even inserted this text into the document - so it would never see it.
        result += f"""
    static juce::File get{upper}Directory()
    {{
        auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
        base = base.getChildFile("Application Support");
#endif
        const auto dir = base.getChildFile("AbacDsp").getChildFile("{module_upper}").getChildFile("Scripts");
        dir.createDirectory();
        return dir;
    }}

    static std::string get{upper}Filename(const std::string& name)
    {{
        juce::StringArray segments;
        segments.addTokens(juce::String(name), "/", "");
        segments.trim();
        segments.removeEmptyStrings();
        if (segments.isEmpty())
        {{
            return {{}};
        }}
        juce::File dir = get{upper}Directory();
        for (int i = 0; i < segments.size() - 1; ++i)
        {{
            const juce::String sanitized = segments[i].removeCharacters("\\\\:*?\\"<>|");
            if (sanitized.isEmpty())
            {{
                return {{}};
            }}
            dir = dir.getChildFile(sanitized);
        }}
        const juce::String fileName = segments[segments.size() - 1].removeCharacters("\\\\:*?\\"<>|");
        if (fileName.isEmpty())
        {{
            return {{}};
        }}
        dir.createDirectory();
        return dir.getChildFile(fileName + ".lua").getFullPathName().toStdString();
    }}
"""
    return result

def create_fileio_script_members(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item['type'] == 'script':
            upper = item['symbol'][0].upper() + item['symbol'][1:]
            result += f"std::string m_current{upper}Name;\n"
    return result

# Fixed-name convenience wrappers (getScriptText, applyScriptText, listScriptNames, ...)
# around the per-symbol FileIo/DSP methods above, so the Editor's popup/menu code (also
# fixed-name, mirroring buildPatchesMenu()'s static shape) doesn't need to know the
# port's actual symbol. Only the first "script"-type port gets this treatment - one
# script pool per instrument is the only case any blueprint so far needs; a blueprint
# wanting more would still have working per-symbol FileIo methods to build on directly.
def create_processor_script_methods(blueprint: Blueprint) -> str:
    script_items = [item for item in blueprint["ports-control"] if item['type'] == 'script']
    if not script_items:
        return ""
    symbol = script_items[0]['symbol']
    upper = symbol[0].upper() + symbol[1:]
    return f"""
    [[nodiscard]] juce::String getScriptText() const
    {{
        return juce::String(m_fileIo.current{upper}());
    }}

    bool applyScriptText(const juce::String& text)
    {{
        if (pluginRunner == nullptr)
        {{
            return false;
        }}
        const bool ok = pluginRunner->set{upper}(text.toStdString());
        if (ok)
        {{
            m_fileIo.update{upper}(text.toStdString());
        }}
        return ok;
    }}

    [[nodiscard]] std::vector<juce::String> listScriptNames() const
    {{
        std::vector<juce::String> result;
        for (const auto& n : m_fileIo.list{upper}Names())
        {{
            result.push_back(juce::String(n));
        }}
        return result;
    }}

    [[nodiscard]] juce::String getCurrentScriptName() const
    {{
        return juce::String(m_fileIo.current{upper}Name());
    }}

    bool requestLoadScript(const juce::String& name)
    {{
        if (!m_fileIo.load{upper}Named(name.toStdString()))
        {{
            return false;
        }}
        return applyScriptText(juce::String(m_fileIo.current{upper}()));
    }}

    bool saveCurrentScriptAs(const juce::String& name)
    {{
        return m_fileIo.save{upper}Named(name.toStdString());
    }}

    bool deleteScriptNamed(const juce::String& name)
    {{
        return m_fileIo.delete{upper}Named(name.toStdString());
    }}

    bool renameScript(const juce::String& oldName, const juce::String& newName)
    {{
        return m_fileIo.rename{upper}Named(oldName.toStdString(), newName.toStdString());
    }}
"""

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
    return create_prepare_script_calls(blueprint) + "\n".join(calls) + ("\n" if calls else "")

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
