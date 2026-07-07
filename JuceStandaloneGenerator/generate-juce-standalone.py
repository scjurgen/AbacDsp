#!/usr/bin/env python3

"""
Todo:
  - save correct color scheme
  - automatic save / load state based on parameters
  - load save preset
  - name preset label
  - center switch labels
  - controls to spectrogram and volume

Control types:
    - drop
    - dial
    - switch

Meter types
    - gauge (fft levels cpu)


Elements
    - line
    - text
    - image

juce::Label divider1;
addAndMakeVisible(divider1);
divider1.setText(juce::String::fromUTF8("-- ① -- ⓿ "), juce::dontSendNotification);

"""

import hashlib
import json
import os
import re
import shutil
import sys

from parseboxstructure import parse_box_structure, construct_boxes
from file_sync import FileSync


stand_alone = False
templateFiles = "templates"
sourceFiles = f"{templateFiles}/sourcefiles"
mainTargetDir = "../examples"
cppTmpDir = "/tmp/{module}"
cppTargetDir = "../examples/{module}"
cppAllElementsInclude = "UiElements.h"
rootCMakeLists = "../CMakeLists.txt"

cppLookAndFeel = "inc/LookAndFeel.h"

cppSourceFilesFixed = [
    "inc/GuiConstants.h",
    "inc/CpuMeter.h",
    "inc/CustomRotaryDial.h",
    "inc/GenericMeter.h",
    "inc/SpectrogramDisplay.h",
    "inc/VuMeter.h",
    "inc/WaveformMeter.h",
    "inc/AppSettings.h",
    "impl/EffectBase.h",
]
cppSourceFiles3rdParty = [
    "3rdparty/CMakeLists.txt",
]


cppJuceCmake = "CMakeListsExamples.txt"
cppGitignore = "gitignore"
cppJuceFile = "{Module}Processor.h"
cppJuceFileImplement = "{Module}Processor.cpp"
cppJuceFileEditor = "{Module}Editor.h"
cppJuceFileConstants = "{Module}Constants.h"

cppSourceFilesImpl = "impl/GenericImpl.h"
cppSourceFilesImplFileIo  = "impl/FileIo.h"
cppPatchParameters = "impl/PatchParameters.h"
cppSourceFilesUnitTest = "unittests/{Module}_tests.cpp"

cppConstants = [
    "MAIN_COLOR",
    "WINDOW_WIDTH",
    "WINDOW_HEIGHT"
]

newConstants = {
    "MAIN_COLOR": "0xffdddddd",
    "WINDOW_WIDTH": "1024",
    "WINDOW_HEIGHT": "600"
}

cppJuceFileVars = [
    "MODULE",
    "MODULE_UPPER",
    "ADD_PARAMETER_LISTENERS",
    "REMOVE_PARAMETER_LISTENERS",
    "CLASS_NAME",
    "CREATE_PARAMETER_LAYOUT",
    "ID_PARAMETERS",
    "INCLUDE_PEDAL",
    "PARAMETER_CHANGED",
    "SETTERS",
    "SETTERS_PARAMS",
    "RUN_MODULE",
    "INIT_WIDGETS",
    "WIDGETS_DECL",
    "RESIZED_AREA",
    "TIMER_CALLBACKS",
    "APPLY_THEME_CALLBACKS",
    "EXTRA_PRIVATE_METHODS",
    "EXTRA_PROCESSOR_METHODS",
    "ParamStructMembers",
    "ParamIdList",
    "ParamIdStringList",
    "ParamIdConstExprList",
    "UpdateParamById",
    "LoadPatches",
    "PatchChanged",
    "PatchIndexAssign",
    "PatchCount"
]

firstMidiIn = True

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
    extra = m.get("extra_timer_callbacks", [])
    if extra:
        res += "\n" + "\n".join(extra) + "\n"
    return res


def createThemeCallbacks(m: dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if item["type"] == "gauge":
            match item.get("gaugetype"):
                case "spectrogram" | "iris":
                    res += f"""{item["symbol"]}Gauge.setGradientPreset(preset);\n"""
                case "cpuload" | "levels" | "signal":
                    res += f"""{item["symbol"]}Gauge.updateColors();\n"""
    return res


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
            res += (f''' if (parameterID == "{item['symbol']}") {{ m_patchIndex[{idx}] = static_cast<int>(newValue);}} ''')
            idx = idx + 1
    return res

def createParameterChanged(m:dict) -> str:
    res = ""
    for item in m["ports-control"]:
        if 'patch' not in item:
            match item["type"]:
                case 'dial':
                    res += f"""{{"{item['symbol']}", [](const AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(v); }}}},\n"""
                case 'drop':
                    res += f"""{{"{item['symbol']}", [](const AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<int>(v)); }}}},\n"""
                case 'switch':
                     res += f"""{{"{item['symbol']}", [](const AudioPluginAudioProcessor& p, const float v) {{ p.pluginRunner->{item['setter']}(static_cast<bool>(v)); }}}},\n"""
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
                res += f"""juce::ToggleButton {varname}{{juce::String::fromUTF8("{item['display']}")}};\n"""
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

def createExtraProcessorMethods(m: dict) -> str:
    methods = m.get("extra_processor_methods", [])
    if not methods:
        return ""
    return "\n".join(methods) + "\n"


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

def getTargetName(target: str, m: dict):
    tmp = target.replace("{module}", m["module"])
    tmp = tmp.replace("{Module}", m["module"][0].upper() + m["module"][1:])
    return tmp

# substitute all variables starting with // in the cpp/h templates
def moduleSubstitutions(source: str, m: dict, vars: list):
    try:
        with open(source, "r") as f:
            content = f.read()
            for v in vars:
                varReplace = '/*' + v + '*/'
                result = content.find(varReplace)
                if result != -1:
                    content = content.replace(varReplace, str(m[v]))
                else:
                    pass
        return content
    except Exception as e:
        print(f"AN ERROR occurred: {type(e).__name__} - {str(e)}")
        print(os.getcwd())
        exit(2)

def moduleRemoveRemainingSectionFromString(content: str) -> str:
    pattern = re.compile(r'/\*START_[A-Z]+\*/.*?/\*END_[A-Z]+\*/\s?', re.DOTALL)
    return pattern.sub('', content)

def moduleRemoveSectionIndicator(content: str, name:str) -> str:
    pattern = re.compile(fr'/\*(START|END)_{name}\*/\s?')
    content = pattern.sub('', content)
    return content

def moduleSubstitutionsBraced(source: str, m: dict, vars: list):
    try:
        with open(source, "r") as f:
            content = f.read()
            for v in vars:
                varReplace = '{' + v + '}'
                if content.find(varReplace) != -1:
                    content = content.replace(varReplace, str(m[v]))
        return content
    except Exception as e:
        print(f"AN ERROR occurred in braced substitution: {e}")
        print(os.getcwd())
        exit(2)

def createAndSaveModuleSubstitutions(target: str, source: str, m: dict, vars: list):
    content = moduleSubstitutions(source, m, vars)
    if "GAUGES" in m:
        for v in m["GAUGES"]:
            content = moduleRemoveSectionIndicator(content, v)
    content = moduleRemoveRemainingSectionFromString(content)
    try:
        with open(getTargetName(target, m), "w") as tf:
            tf.write(content)
    except Exception as e:
        print(f"AN ERROR while saving f{target} occurred: {e}")
        print(os.getcwd())
        exit(2)


def createAndSaveModuleSubstitutionsBraced(target: str, source: str, m: dict, vars: list):
    content = moduleSubstitutionsBraced(source, m, vars)
    try:
        with open(getTargetName(target, m), "w") as tf:
            tf.write(content)
    except Exception as e:
        print(f"AN ERROR while saving f{target} occurred: {e}")
        print(os.getcwd())
        exit(2)


# TODO: parameterChanged handling
def setRunner(keyUpper: str, symbol: str, transform: str):
    return f"""    if (isChanged(*{symbol}, prev_{symbol}))\n    {{
        // print_log("{symbol}:%f",*{symbol});
        prev_{symbol} = *{symbol};
        pluginRunner->set{keyUpper}(*{symbol}{transform});
    }}\n"""


def getVersion(module: str):
    try:
        with open(f"versions.json") as f:
            v = f.read()
            versions = json.loads(v)
            if module in versions:
                micro = versions[module]["micro"]
                micro += 1
                versions[module]["micro"] = micro
            else:
                versions[module] = dict()
                versions[module]["major"] = 0
                versions[module]["minor"] = 0
                versions[module]["micro"] = 0

    except:
        versions = {module: {}}
        versions[module]["major"] = 0
        versions[module]["minor"] = 0
        versions[module]["micro"] = 0
    jsonStr = json.dumps(versions, indent=4, separators=(", ", ": "), sort_keys=True)
    with open(f"versions.json", "w") as f:
        f.write(jsonStr)
    return versions[module]


def loadConfig(module: str):
    jsonFile = f"blueprints/{module}.json"
    configString = open(jsonFile).read()
    js = json.loads(configString)
    js['VersionString'] = "0.0.0"
    return js

def fillDefaults(item: dict):
    if not 'unit' in item:
        item['unit'] = ""
    if not 'default' in item:
        item['default'] = 0
    if not 'minimum' in item:
        item['minimum'] = 0
    if not 'maximum' in item:
        item['maximum'] = 1
    if not 'precision' in item:
        item['precision'] = 1
    return item

def parse_and_fill_range(values):
    defaults = [0, 1, 0, 1, "false"]
    keys = ['rangeStart', 'rangeEnd', 'intervalValue', 'skewFactor', 'useSymmetricSkew']

    # Parse and fill values
    result = {}
    for i, key in enumerate(keys):
        if i < len(values):
            result[key] = values[i]
        else:
            result[key] = defaults[i]
    if result['useSymmetricSkew'] == False:
        result['useSymmetricSkew'] = "false"
    else:
        result['useSymmetricSkew'] = "true"
    return result

def fillRange(item):
    if 'range' in item:
        item.update(parse_and_fill_range(item['range']))
    else:
        print(f"Using default range {item}")
        item.update(parse_and_fill_range([0, 1, 0, 1, "false"]))
    return item


def enrich(m: dict):
    items_to_remove = []
    for item in m['ports-control']:
        if item['type'] in ['dial', 'switch'] and 'count' in item:
            for idx in range(item['count']):
                newItem = dict()
                newItem['short'] = item['short'].format(idx + 1)
                newItem['type'] = item['type']
                newItem['display'] = item['display'].format(idx + 1)
                newItem['symbol'] = item['symbol'].format(idx + 1)
                newItem['range'] = item['range']
                newItem['precision'] = item['precision']
                newItem['unit'] = item['unit']
                m['ports-control'].append(newItem)
            items_to_remove.append(item)

    for item in items_to_remove:
        m['ports-control'].remove(item)


def ensureCMakeSubdirectory(module: str):
    with open(rootCMakeLists, "r") as f:
        lines = f.readlines()
    entry_pattern = re.compile(r'^(\s*)add_subdirectory\(examples/([A-Za-z0-9_]+)\)\s*$')
    entries = [(i, match.group(2)) for i, line in enumerate(lines) if (match := entry_pattern.match(line))]
    if not entries:
        print(f"WARNING: no add_subdirectory(examples/...) entries found in {rootCMakeLists}, skipping")
        return
    if module in (mod for _, mod in entries):
        return
    indent = entry_pattern.match(lines[entries[0][0]]).group(1)
    insert_at = next((i for i, mod in entries if module < mod), entries[-1][0] + 1)
    lines.insert(insert_at, f"{indent}add_subdirectory(examples/{module})\n")
    with open(rootCMakeLists, "w") as f:
        f.writelines(lines)
    print(f"Added add_subdirectory(examples/{module}) to {rootCMakeLists}")


def createPackageFromJsonDict(m: dict):
    targetDir = f"{mainTargetDir}/{m['name']}"
    enrich(m)
    print(f"Generating: {targetDir}")

    m["module"] = m["name"]
    m["Module"] = m["name"][0].upper() + m["name"][1:]
    m["description"] = "\n".join(m["description"])
    m["ports"] = "\n"
    m["CPP"]["module"] = m["name"]
    m["CPP"]["name"] = m["name"]
    m["CPP"]["MODULE"] = m["name"]
    m["CPP"]["MODULE_UPPER"] = m["Module"]
    m["CPP"]["ADD_PARAMETERS"] = ""
    m["CPP"]["ID_PARAMETERS"] = ""
    m["CPP"]["NEW_RUNNER"] = ""
    m["CPP"]["JUCE_PARAMS"] = ""
    m["CPP"]["RUN_MODULE"] = ""

    for idx in range(len(m["ports-control"])):
        item = fillDefaults(m["ports-control"][idx])
        if item['type'] in ['dial', 'slider']:
            item = fillRange(item)
        item["keyUpper"] = item['symbol'][0].upper() + item['symbol'][1:]
        item["setter"] = "set"+item["keyUpper"]

    m["CPP"]["TIMER_CALLBACKS"] = createGaugeCallbacks(m)
    m["CPP"]["APPLY_THEME_CALLBACKS"] = createThemeCallbacks(m)
    m["CPP"]["ADD_PARAMETER_LISTENERS"] = addParameterListeners(m)
    m["CPP"]["REMOVE_PARAMETER_LISTENERS"] = removeParameterListeners(m)
    m["CPP"]["CREATE_PARAMETER_LAYOUT"] = createParameterLayout(m)
    m["CPP"]["PARAMETER_CHANGED"] = createParameterChanged(m)
    m["CPP"]["SETTERS"] = createSettersImplementation(m)
    m["CPP"]["SETTERS_PARAMS"] = createVariablesImplementation(m)

    m["CPP"]["ParamIdList"] = idList(m)
    m["CPP"]["ParamIdStringList"] = idStringList(m)
    m["CPP"]["ParamIdConstExprList"]  = paramConstExprList(m)
    m["CPP"]["UpdateParamById"]= updateParamById(m)
    m["CPP"]["LoadPatches"] = loadPatches(m)
    m["CPP"]["PatchChanged"] = createPatchChanged(m)
    m["CPP"]["PatchIndexAssign"] = createPatchIndexAssign(m)
    m["CPP"]["PatchCount"] = getPatchCount(m)
    m["CPP"]["ParamStructMembers"] = createtructVariablesImplementation(m)

    m["CPP"]["INIT_WIDGETS"] = createInitWidgets(m)
    m["CPP"]["WIDGETS_DECL"] = createWidgetsDecl(m)
    m["CPP"]["RESIZED_AREA"] = construct_boxes(m)
    m["CPP"]["EXTRA_PRIVATE_METHODS"] = createExtraPrivateMethods(m)
    m["CPP"]["EXTRA_PROCESSOR_METHODS"] = createExtraProcessorMethods(m)

    for idx in range(len(m["ports-control"])):
        item = fillDefaults(m["ports-control"][idx])
        keyUpper = item['symbol'][0].upper() + item['symbol'][1:]
        print(f"\t{item['symbol']}")

        type = item['type']
        if type == 'drop':
            idx = 0
            menuList = 'juce::StringArray {'
            for enumItem in item['listitems']:
                menuList += f'"{enumItem}",'
                idx += 1
            menuList = menuList[:-1] + '}'
            item['minimum'] = 0
            item['maximum'] = idx - 1
        m["CPP"]["ID_PARAMETERS"] += f'PARAMETER_ID({item["symbol"]})\n'
        m["CPP"]["JUCE_PARAMS"] += f"    juce::AudioParameter{type}* {item['symbol']}; // {item['display']}\n"
        if item['type'] == 'integer':
            m["CPP"]["JUCE_PARAMS"] += f"    int prev_{item['symbol']} {{ {item['minimum'] - 1} }};\n\n"
        elif item['type'] == 'Choice':
            val = item["default"] - 1
            if val < 0:
                val = val + 2
            m["CPP"]["JUCE_PARAMS"] += f"    int prev_{item['symbol']} {{ {val} }};\n\n"
        elif item['type'] == 'bool':
            val = item["default"]
            val = not val
            m["CPP"]["JUCE_PARAMS"] += f"    bool prev_{item['symbol']}{{ {val} }};\n\n"
        else:
            m["CPP"]["JUCE_PARAMS"] += f"    float prev_{item['symbol']} {{ {item['minimum'] - 1} }};\n\n"

        # m["CPP"]["RUN_MODULE"] += setRunner(keyUpper, item['symbol'], "")

    if (m["CPP"]["PROCMODE"] == "Stereo-Stereo"):
        m["CPP"][
            "RUN_MODULE"] += """
            std::array<std::array<float, NumSamplesPerBlock>, 2> output;
            pluginRunner->processBlockStereoStereo(buffer.getReadPointer(0) + i, buffer.getReadPointer(1) + i, output[0].data(), output[1].data());
            std::copy_n(output[0].data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output[1].data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
           """
    elif (m["CPP"]["PROCMODE"] == "Mono-Stereo"):
        m["CPP"][
            "RUN_MODULE"] += """ 
            std::array<std::array<float, NumSamplesPerBlock>, 2> output;
            pluginRunner->processBlockMonoStereo(buffer.getReadPointer(0)+i, output[0].data(), output[1].data()); 
            std::copy_n(output[0].data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output[1].data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
            """
    elif (m["CPP"]["PROCMODE"] == "Mono-Mono"):
        m["CPP"]["RUN_MODULE"] += (
            """ 
            std::array<float, NumSamplesPerBlock> output;
            pluginRunner->processBlockMonoMono(buffer.getReadPointer(0)+i, output.data());
            std::copy_n(output.data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output.data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
            """)
    elif (m["CPP"]["PROCMODE"] == "Midi-Stereo"):
        m["CPP"][
            "RUN_MODULE"] += """ pluginRunner->processBlockMidiStereo(buffer.getWritePointer(0)+i, buffer.getWritePointer(1)+i); """

    dir = cppTmpDir.replace("{module}", m["module"])
    os.makedirs(f"{dir}/3rdparty", mode=0o777, exist_ok=True)
    os.makedirs(f"{dir}/src", mode=0o777, exist_ok=True)
    os.makedirs(f"{dir}/src/impl", mode=0o777, exist_ok=True)
    os.makedirs(f"{dir}/src/inc", mode=0o777, exist_ok=True)
    os.makedirs(f"{dir}/src/unittests", mode=0o777, exist_ok=True)

    m["CPP"]["Module"] = m["CPP"]["module"].capitalize()
    m["CPP"]["GAUGES"] = gauge_present(m)
    if getPatchCount(m) > 0:
        m["CPP"]["GAUGES"].append("PATCHSUPPORT")

    cppTargetFile = f"{cppTmpDir}/src/{cppJuceFile}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppJuceFile}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppJuceFileImplement}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppJuceFileImplement}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppSourceFilesImpl}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppSourceFilesImpl}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppSourceFilesImplFileIo}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppSourceFilesImplFileIo}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppPatchParameters}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppPatchParameters}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppJuceFileEditor}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppJuceFileEditor}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppSourceFilesUnitTest}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppSourceFilesUnitTest}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    cppTargetFile = f"{cppTmpDir}/src/{cppJuceFileConstants}"
    newConstants["Module"] = m["CPP"]["Module"]
    newConstants["module"] = m["CPP"]["module"]
    newConstants["WINDOW_WIDTH"] = m["layout"]["WINDOW_WIDTH"]
    newConstants["WINDOW_HEIGHT"] = m["layout"]["WINDOW_HEIGHT"]

    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppJuceFileConstants}", newConstants, cppConstants)
    clangFormatFile = getTargetName(cppTargetFile, m)
    cmd = f"clang-format -i -style=file {clangFormatFile}"
    os.system(cmd)

    digest = hashlib.sha256(m["CPP"]["module"].encode("utf-8")).hexdigest()

    m["CPP"]["PluginCode"] = m["plugintype"][0] + digest[1:4].upper()
    m["CPP"]['IsSynth'] = "FALSE"
    m["CPP"]['NeedsMidiInput'] = "FALSE"
    m["CPP"]['NeedsMidiOutput'] = "FALSE"
    m["CPP"]['IsMidiEffect'] = "FALSE"
    m["CPP"]['EditorWantsKeyboardFocus'] = "FALSE"
    m["CPP"]['VersionString'] = m['VersionString']

    cppTargetFile = f"{cppTmpDir}/CMakeLists.txt"
    createAndSaveModuleSubstitutionsBraced(cppTargetFile, f"{sourceFiles}/{cppJuceCmake}", m["CPP"],
                                           ['module', 'Module', 'PluginCode',
                                            'IsSynth',
                                            'NeedsMidiInput',
                                            'NeedsMidiOutput',
                                            'VersionString',
                                            'IsMidiEffect',
                                            'EditorWantsKeyboardFocus'])
    cppTargetFile = f"{cppTmpDir}/src/unittests/CMakeLists.txt"
    createAndSaveModuleSubstitutionsBraced(cppTargetFile, f"{sourceFiles}/unittests/CMakeLists.txt", m["CPP"],
                                           ['module', 'Module', 'PluginCode',
                                            'IsSynth',
                                            'NeedsMidiInput',
                                            'NeedsMidiOutput',
                                            'VersionString',
                                            'IsMidiEffect',
                                            'EditorWantsKeyboardFocus'])
    with open(f"{dir}/VERSION", "w") as f:
        f.write(m['VersionString'])
    shutil.copyfile(f"{sourceFiles}/gitignore", f"{dir}/.gitignore")
    shutil.copyfile(f"{sourceFiles}/logo.png", f"{dir}/logo.png")
    if stand_alone:
        shutil.copyfile(f"{templateFiles}/init-project.sh", f"{dir}/init-project.sh")
    for file_name in cppSourceFilesFixed:
        shutil.copyfile(f"{sourceFiles}/{file_name}", f"{dir}/src/{file_name}")
    if stand_alone:
        for file_name in cppSourceFiles3rdParty:
            shutil.copyfile(f"{sourceFiles}/{file_name}", f"{dir}/{file_name}")
    shutil.copyfile(f"{sourceFiles}/{cppLookAndFeel}", f"{dir}/src/{cppLookAndFeel}")

    with open(f"{dir}/src/{cppAllElementsInclude}", "w") as f:
        f.write("#pragma once\n\n")
        f.write("/*\n * AUTO GENERATED,\n * NOT A GOOD IDEA TO CHANGE STUFF HERE\n */\n\n")
        f.write(f"""#include "{m["Module"]}Constants.h"\n\n""")
        for file_name in cppSourceFilesFixed:
            f.write(f"""#include "{file_name}"\n""")
        for extra in m.get("extra_ui_includes", []):
            f.write(f"""#include "{extra}"\n""")
        f.write(f"""\n#include "{cppLookAndFeel}"\n""")
    protected_files = {
        "gitignore",
        "src/unittests/CMakeLists.txt",
        "src/CMakeLists.txt",
        "CMakeLists.txt",
    }
    for pf in m.get("protected_files", []):
        protected_files.add(pf)
    targetDir = cppTargetDir.replace("{module}", m["module"])
    force = m.get("_force_all", False)
    syncer = FileSync(cppTmpDir.replace("{module}", m["module"]), targetDir, set() if force else protected_files)
    syncer.sync()

    if not stand_alone:
        ensureCMakeSubdirectory(m["module"])


def usage(progname: str):
    progname = os.path.basename(progname)
    print(f"Usage: {progname} (--list | --help | --forceall | --standalone | --target-dir <path>) module <module>")
    print("module name of a module in blueprints ")
    print("--target-dir <path>  overwrite the output folder (overrides --standalone's default target too);")
    print("                     put {module} in <path> to control where the module name is inserted,")
    print("                     otherwise it is appended as a subfolder")
    print(moduleList)
    exit(1)


moduleList = []
for file in os.listdir("blueprints"):
    if file.endswith(".json"):
        moduleList.append(file[:-5])

moduleList.sort()

if len(sys.argv) == 1:
    usage(sys.argv[0])

if len(sys.argv) >= 2:
    if sys.argv[1] == '--list':
        s = ""
        for m in moduleList:
            s += m + " "
        print(s)
    elif sys.argv[1] == '-h' or sys.argv[1] == '--help':
        usage(sys.argv[0])
    else:
        force_all = False
        i = 1
        while i < len(sys.argv):
            m = sys.argv[i]
            if m == "--standalone":
                mainTargetDir = "../juce-projects"
                cppTargetDir = "../juce-projects/{module}"
                stand_alone = True
                cppJuceCmake = "CMakeListsStandalone.txt"
            elif m == "--forceall":
                force_all = True
            elif m == "--target-dir":
                i += 1
                if i >= len(sys.argv):
                    print("--target-dir requires a path argument")
                    usage(sys.argv[0])
                targetDirArg = os.path.abspath(os.path.expanduser(sys.argv[i]))
                cppTargetDir = targetDirArg if "{module}" in targetDirArg else f"{targetDirArg}/{{module}}"
                mainTargetDir = targetDirArg
            elif m[0] == '-':
                print(f'unknown option {m}')
                usage(sys.argv[0])
            else:
                if m in moduleList:
                    cfg = loadConfig(m)
                    if force_all:
                        cfg["_force_all"] = True
                    createPackageFromJsonDict(cfg)
                else:
                    print(f'module "{sys.argv[i]}" not found (use --list to obtain a list)')
            i += 1