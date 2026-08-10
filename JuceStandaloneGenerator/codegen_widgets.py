from blueprint import Blueprint
from codegen_processor import cc_enabled_controls
from parseboxstructure import parse_box_structure

_WIDGET_SUFFIX = {
    "dial": "Dial",
    "switch": "Switch",
    "drop": "Drop",
    "gauge": "Gauge",
    "label": "Label",
    "script": "Button",
    "luacontrolarea": "LuaControlArea",
}


def widget_varname(item: dict) -> str:
    return f"{item['symbol']}{_WIDGET_SUFFIX[item['type']]}"


def gauge_present(blueprint: Blueprint) -> list[str]:
    result = []
    for item in blueprint["ports-control"]:
        if item["type"] == "gauge":
            match item["gaugetype"]:
                case "cpuload":
                    result.append("SHOWCPULOAD")
                case "levels":
                    result.append("SHOWVUMETER")
                case "spectrogram":
                    result.append("SHOWSPECTROGRAM")
                case "signal":
                    result.append("SHOWWAVEFORM")
    return result

def create_momentary_flash_ticks(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item["type"] == "switch" and item.get("momentary", False):
            result += f"""{item['symbol']}Switch.tickFlash();\n"""
    return result

def create_gauge_bindings(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item["type"] == "gauge":
            for binding in item.get("bindings", []):
                expr = f"""processorRef.{binding["from"]}()"""
                if "cast" in binding:
                    expr = f"""static_cast<{binding["cast"]}>({expr})"""
                result += f"""{item["symbol"]}Gauge.{binding["call"]}({expr});\n"""
    return result

def create_switch_label_swaps(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item["type"] == "switch" and "state_label" in item:
            state_label = item["state_label"]
            result += (f"""{item["symbol"]}Switch.setButtonText(processorRef.{state_label["query"]}() """
                       f"""? juce::String::fromUTF8("{state_label["on"]}") """
                       f""": juce::String::fromUTF8("{state_label["off"]}"));\n""")
    return result

def create_gauge_callbacks(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item["type"] == "gauge":
            match item["gaugetype"]:
                case "cpuload":
                    result += f"""{item["symbol"]}Gauge.update(processorRef.getCpuLoad());\n"""
                case "levels":
                    result += f"""{item["symbol"]}Gauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());\n"""
                case "spectrogram":
                    result += f"""{item["symbol"]}Gauge.update(processorRef.getSpectrogram());\n"""
                case "signal":
                    result += f"""{item["symbol"]}Gauge.update(processorRef.getWaveDataToShow());\n"""
    result += create_gauge_bindings(blueprint)
    result += create_momentary_flash_ticks(blueprint)
    result += create_switch_label_swaps(blueprint)
    extra = blueprint.get("extra_timer_callbacks", [])
    if extra:
        result += "\n" + "\n".join(extra) + "\n"
    if cc_enabled_controls(blueprint):
        result += "processorRef.consumeLastLearnedCc();\n"
    return result


def create_theme_callbacks(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if item["type"] == "gauge":
            match item.get("gaugetype"):
                case "spectrogram" | "iris":
                    result += f"""{item["symbol"]}Gauge.setGradientPreset(preset);\n"""
                case "cpuload" | "levels" | "signal" | "processingbins":
                    result += f"""{item["symbol"]}Gauge.updateColors();\n"""
    return result


def create_widgets_decl(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        symbol = item['symbol']
        match item['type']:
            case "dial":
                varname = f"{symbol}Dial"
                result += f"CustomRotaryDial {varname}{{this}};\n"
            case "switch":
                varname = f"{symbol}Switch"
                switch_type = "MomentaryToggleButton" if item.get("momentary", False) else "juce::ToggleButton"
                result += f"""{switch_type} {varname}{{juce::String::fromUTF8("{item['display']}")}};\n"""
                result += f"std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> {varname}Attachment;\n"
            case "drop":
                varname = f"{symbol}Drop"
                result += f"juce::ComboBox {varname}{{}};\n"
                result += f"std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> {varname}Attachment;\n"
            case "gauge":
                varname = f"{symbol}Gauge"
                if "customtype" in item:
                    result += f"{item['customtype']} {varname}{{}};\n"
                else:
                    match item['gaugetype']:
                        case "cpuload":
                            result += f"CpuGauge {varname}{{}};\n"
                        case "spectrogram":
                            result += f"SpectrogramDisplay {varname}{{AppSettings::loadTheme()}};\n"
                        case "levels":
                            result += f"Gauge {varname}{{}};\n"
                        case "signal":
                            result += f"WaveformGauge {varname}{{}};\n"
            case "label":
                varname = f"{symbol}Label"
                result += f"juce::Label {varname}{{}};\n"
            case "script":
                varname = f"{symbol}Button"
                result += f"""juce::TextButton {varname}{{juce::String::fromUTF8("{item['display']}")}};\n"""
            case "luacontrolarea":
                varname = f"{symbol}LuaControlArea"
                result += f"LuaControlArea {varname}{{}};\n"
    return result

def create_init_widgets(blueprint: Blueprint) -> str:
    # Build map: symbol -> list of ports that depend_on it (for onChange injection)
    dependents: dict[str, list] = {}
    for item in blueprint["ports-control"]:
        if "depends_on" in item:
            dep_sym = item["depends_on"]
            dependents.setdefault(dep_sym, []).append(item)

    result = ""
    for item in blueprint["ports-control"]:
        varname = f"{item['symbol']}"
        conditional = "visible_when" in item
        add_fn = "addChildComponent" if conditional else "addAndMakeVisible"
        match item['type']:
            case "dial":
                varname += "Dial"
                result += f"""{add_fn}({varname});
                {varname}.reset(valueTreeState, "{item['symbol']}");
                {varname}.setLabelText(juce::String::fromUTF8("{item['display']}"));\n"""
                if item['type'] == 'dial' and 'cc' in item:
                    symbol = item['symbol']
                    result += f"""{varname}.setCcMappable(true, {{
                        [this] {{ processorRef.beginCcLearn(CcTarget::{symbol}); }},
                        [this] {{ return processorRef.getCcRange(CcTarget::{symbol}); }},
                        [this] (float lo, float hi) {{ processorRef.setCcRange(CcTarget::{symbol}, lo, hi); }},
                        [this] {{ processorRef.clearCcAssignment(CcTarget::{symbol}); }},
                        [this] {{ return processorRef.getCcController(CcTarget::{symbol}); }}
                    }});\n"""
            case "switch":
                varname += "Switch"
                result += f"""{add_fn}({varname});
                {varname}Attachment = std::make_unique < juce::AudioProcessorValueTreeState::ButtonAttachment > (
                valueTreeState, "{item['symbol']}", {varname});
                \n"""
            case "drop":
                varname += "Drop"
                result += f"""{add_fn}({varname});
                {varname}.addItemList(valueTreeState.getParameter("{item['symbol']}")->getAllValueStrings(), 1);
                {varname}Attachment = std::make_unique < juce::AudioProcessorValueTreeState::ComboBoxAttachment > (
                valueTreeState, "{item['symbol']}", {varname});\n"""
                # Inject onChange handler for any dependents
                if item['symbol'] in dependents:
                    callbacks = " ".join(
                        f"update{d['symbol'][0].upper() + d['symbol'][1:]}Visibility();"
                        for d in dependents[item['symbol']]
                    )
                    result += f"""{varname}.onChange = [this] {{ {callbacks} }};\n"""
                    for dep in dependents[item['symbol']]:
                        dep_upper = dep['symbol'][0].upper() + dep['symbol'][1:]
                        result += f"""update{dep_upper}Visibility();\n"""
            case "gauge":
                varname += "Gauge"
                result += f"""{add_fn}({varname}); {varname}.setLabelText(juce::String::fromUTF8("{item['display']}"));\n"""
            case "label":
                varname += "Label"
                result += f"""{add_fn}({varname}); {varname}.setText(juce::String::fromUTF8("{item['display']}"), juce::dontSendNotification);\n"""
            case "script":
                varname += "Button"
                # Fixed name, not per-symbol: mirrors ProcessorScriptMethods only
                # supporting the first "script"-type port (see its docstring).
                result += f"""{add_fn}({varname}); {varname}.onClick = [this] {{ openScriptEditor(); }};\n"""
            case "luacontrolarea":
                varname += "LuaControlArea"
                result += f"""{add_fn}({varname});\n"""
    return result


def composition_shorts(blueprint: Blueprint, section: str) -> set[str]:
    sect = blueprint.get(section)
    if not sect or not sect.get("composition"):
        return set()
    _, _, areas = parse_box_structure(sect["composition"])
    return {entry["symbol"] for area in areas for entry in area}


def performance_page_shorts(blueprint: Blueprint) -> set[str]:
    return composition_shorts(blueprint, "performance-page")


def create_page_switch_methods(blueprint: Blueprint) -> dict[str, str]:
    perf_shorts = performance_page_shorts(blueprint)
    if not perf_shorts:
        return {"PAGE_SHOW_PERFORMANCE": "", "PAGE_SHOW_SETTINGS": ""}
    settings_shorts = composition_shorts(blueprint, "layout")
    show_performance = ""
    show_settings = ""
    for item in blueprint["ports-control"]:
        if "visible_when" in item:
            continue
        varname = widget_varname(item)
        settings_visible = "true" if item["short"] in settings_shorts else "false"
        show_settings += f"{varname}.setVisible({settings_visible});\n"
        performance_visible = "true" if item["short"] in perf_shorts else "false"
        show_performance += f"{varname}.setVisible({performance_visible});\n"
    return {"PAGE_SHOW_PERFORMANCE": show_performance, "PAGE_SHOW_SETTINGS": show_settings}


def create_extra_private_methods(blueprint: Blueprint) -> str:
    result = ""
    for item in blueprint["ports-control"]:
        if "visible_when" in item:
            symbol = item['symbol']
            symbol_upper = symbol[0].upper() + symbol[1:]
            varname = f"{symbol}{item['type'].capitalize()}"
            condition = item["visible_when"]
            result += f"""  void update{symbol_upper}Visibility()\n  {{\n"""
            result += f"""    {varname}.setVisible({condition});\n"""
            result += f"""    resized();\n  }}\n\n"""
    return result
