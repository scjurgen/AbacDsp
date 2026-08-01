from codegen_processor import cc_enabled_controls


def gauge_present(m:dict) -> list:
    res=[]
    for item in m["ports-control"]:
        if item["type"] == "gauge":
            match item["gaugetype"]:
                case "cpuload":
                    res.append("SHOWCPULOAD")
                case "levels":
                    res.append("SHOWVUMETER")
                case "spectrogram":
                    res.append("SHOWSPECTROGRAM")
                case "signal":
                    res.append("SHOWWAVEFORM")
    return res

def createMomentaryFlashTicks(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "switch" and item.get("momentary", False):
            res += f"""{item['symbol']}Switch.tickFlash();\n"""
    return res

def createGaugeBindings(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "gauge":
            for binding in item.get("bindings", []):
                expr = f"""processorRef.{binding["from"]}()"""
                if "cast" in binding:
                    expr = f"""static_cast<{binding["cast"]}>({expr})"""
                res += f"""{item["symbol"]}Gauge.{binding["call"]}({expr});\n"""
    return res

def createSwitchLabelSwaps(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "switch" and "state_label" in item:
            state_label = item["state_label"]
            res += (f"""{item["symbol"]}Switch.setButtonText(processorRef.{state_label["query"]}() """
                     f"""? juce::String::fromUTF8("{state_label["on"]}") """
                     f""": juce::String::fromUTF8("{state_label["off"]}"));\n""")
    return res

def createGaugeCallbacks(m:dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "gauge":
            match item["gaugetype"]:
                case "cpuload":
                    res += f"""{item["symbol"]}Gauge.update(processorRef.getCpuLoad());\n"""
                case "levels":
                    res += f"""{item["symbol"]}Gauge.update(processorRef.getInputDbLoad(), processorRef.getOutputDbLoad());\n"""
                case "spectrogram":
                    res += f"""{item["symbol"]}Gauge.update(processorRef.getSpectrogram());\n"""
                case "signal":
                    res += f"""{item["symbol"]}Gauge.update(processorRef.getWaveDataToShow());\n"""
    res += createGaugeBindings(m)
    res += createMomentaryFlashTicks(m)
    res += createSwitchLabelSwaps(m)
    extra = m.get("extra_timer_callbacks", [])
    if extra:
        res += "\n" + "\n".join(extra) + "\n"
    if cc_enabled_controls(m):
        res += "processorRef.consumeLastLearnedCc();\n"
    return res


def createThemeCallbacks(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "gauge":
            match item.get("gaugetype"):
                case "spectrogram" | "iris":
                    res += f"""{item["symbol"]}Gauge.setGradientPreset(preset);\n"""
                case "cpuload" | "levels" | "signal" | "processingbins":
                    res += f"""{item["symbol"]}Gauge.updateColors();\n"""
    return res


def createWidgetsDecl(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        symbol = item['symbol']
        match item['type']:
            case "dial":
                varname = f"{symbol}Dial"
                res += f"CustomRotaryDial {varname}{{this}};\n"
            case "switch":
                varname = f"{symbol}Switch"
                switchType = "MomentaryToggleButton" if item.get("momentary", False) else "juce::ToggleButton"
                res += f"""{switchType} {varname}{{juce::String::fromUTF8("{item['display']}")}};\n"""
                res += f"std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> {varname}Attachment;\n"
            case "drop":
                varname = f"{symbol}Drop"
                res += f"juce::ComboBox {varname}{{}};\n"
                res += f"std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> {varname}Attachment;\n"
            case "gauge":
                varname = f"{symbol}Gauge"
                if "customtype" in item:
                    res += f"{item['customtype']} {varname}{{}};\n"
                else:
                    match item['gaugetype']:
                        case "cpuload":
                            res += f"CpuGauge {varname}{{}};\n"
                        case "spectrogram":
                            res += f"SpectrogramDisplay {varname}{{AppSettings::loadTheme()}};\n"
                        case "levels":
                            res += f"Gauge {varname}{{}};\n"
                        case "signal":
                            res += f"WaveformGauge {varname}{{}};\n"
            case "label":
                varname = f"{symbol}Label"
                res += f"juce::Label {varname}{{}};\n"
    return res

def createInitWidgets(m: dict) -> str:
    # Build map: symbol -> list of ports that depend_on it (for onChange injection)
    dependents: dict = {}
    for item in m["ports-control"]:
        if "depends_on" in item:
            dep_sym = item["depends_on"]
            dependents.setdefault(dep_sym, []).append(item)

    res = ""
    for item in m["ports-control"]:
        varname = f"{item['symbol']}"
        conditional = "visible_when" in item
        add_fn = "addChildComponent" if conditional else "addAndMakeVisible"
        match item['type']:
            case "dial":
                varname += "Dial"
                res += f"""{add_fn}({varname});
                {varname}.reset(valueTreeState, "{item['symbol']}");
                {varname}.setLabelText(juce::String::fromUTF8("{item['display']}"));\n"""
                if item['type'] == 'dial' and 'cc' in item:
                    symbol = item['symbol']
                    res += f"""{varname}.setCcMappable(true, {{
                        [this] {{ processorRef.beginCcLearn(CcTarget::{symbol}); }},
                        [this] {{ return processorRef.getCcRange(CcTarget::{symbol}); }},
                        [this] (float lo, float hi) {{ processorRef.setCcRange(CcTarget::{symbol}, lo, hi); }},
                        [this] {{ processorRef.clearCcAssignment(CcTarget::{symbol}); }},
                        [this] {{ return processorRef.getCcController(CcTarget::{symbol}); }}
                    }});\n"""
            case "switch":
                varname += "Switch"
                res += f"""{add_fn}({varname});
                {varname}Attachment = std::make_unique < juce::AudioProcessorValueTreeState::ButtonAttachment > (
                valueTreeState, "{item['symbol']}", {varname});
                \n"""
            case "drop":
                varname += "Drop"
                res += f"""{add_fn}({varname});
                {varname}.addItemList(valueTreeState.getParameter("{item['symbol']}")->getAllValueStrings(), 1);
                {varname}Attachment = std::make_unique < juce::AudioProcessorValueTreeState::ComboBoxAttachment > (
                valueTreeState, "{item['symbol']}", {varname});\n"""
                # Inject onChange handler for any dependents
                if item['symbol'] in dependents:
                    callbacks = " ".join(
                        f"update{d['symbol'][0].upper() + d['symbol'][1:]}Visibility();"
                        for d in dependents[item['symbol']]
                    )
                    res += f"""{varname}.onChange = [this] {{ {callbacks} }};\n"""
                    for dep in dependents[item['symbol']]:
                        dep_upper = dep['symbol'][0].upper() + dep['symbol'][1:]
                        res += f"""update{dep_upper}Visibility();\n"""
            case "gauge":
                varname += "Gauge"
                res += f"""{add_fn}({varname}); {varname}.setLabelText(juce::String::fromUTF8("{item['display']}"));\n"""
            case "label":
                varname += "Label"
                res += f"""{add_fn}({varname}); {varname}.setText(juce::String::fromUTF8("{item['display']}"), juce::dontSendNotification);\n"""
    return res


def createExtraPrivateMethods(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if "visible_when" in item:
            symbol = item['symbol']
            symbol_upper = symbol[0].upper() + symbol[1:]
            varname = f"{symbol}{item['type'].capitalize()}"
            condition = item["visible_when"]
            res += f"""  void update{symbol_upper}Visibility()\n  {{\n"""
            res += f"""    {varname}.setVisible({condition});\n"""
            res += f"""    resized();\n  }}\n\n"""
    return res
