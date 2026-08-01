#!/usr/bin/env python3

"""
Todo:
  - center switch labels
  - controls for spectrogram and volume
  - Add Elements
    - line
    - image
    - background panel image
"""

import hashlib
import os
import re
import shutil
import sys

from parseboxstructure import parse_box_structure, construct_boxes
from file_sync import FileSync

from blueprint import loadConfig, fillDefaults, fillRange, fillCc, enrich
from report import printParameterTable
from codegen_processor import (
    createParameterLayout, createParameterChanged, updateParamById, idList,
    idStringList, paramConstExprList, loadPatches, createPatchChanged,
    getPatchCount, createPatchIndexAssign, createSettersImplementation,
    createtructVariablesImplementation, createVariablesImplementation,
    addParameterListeners, removeParameterListeners, createProcessorForwards,
    createExtraProcessorMethods, createExtraPrepareCalls,
    createExtraProcessorMembers, createExtraGetStateCalls,
    createExtraSetStateCalls, createCcMapping,
)
from codegen_widgets import (
    gauge_present, createGaugeCallbacks, createThemeCallbacks,
    createWidgetsDecl, createInitWidgets, createExtraPrivateMethods,
)
from template_engine import (
    getTargetName, runClangFormat, createAndSaveModuleSubstitutions,
    createAndSaveModuleSubstitutionsBraced,
)
from cli import list_modules, usage, parse_args

# Every path below (blueprints/, templates/, ../examples) is relative to this
# script's own directory, not the caller's cwd.
os.chdir(os.path.dirname(os.path.abspath(__file__)))

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
    "inc/ThemeOrbit.h",
    "inc/CpuMeter.h",
    "inc/CustomRotaryDial.h",
    "inc/MomentaryToggleButton.h",
    "inc/GenericMeter.h",
    "inc/StatusBar.h",
    "inc/SpectrogramDisplay.h",
    "inc/VuMeter.h",
    "inc/WaveformMeter.h",
    "inc/SliceWaveDisplay.h",
    "inc/CircularBarDisplay.h",
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
cppCcMapping = "impl/CcMapping.h"
cppCcSettings = "impl/CcSettings.h"
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
    "EXTRA_PROCESSOR_MEMBERS",
    "EXTRA_PREPARE_CALLS",
    "EXTRA_GET_STATE_CALLS",
    "EXTRA_SET_STATE_CALLS",
    "ParamStructMembers",
    "ParamIdList",
    "ParamIdStringList",
    "ParamIdConstExprList",
    "UpdateParamById",
    "LoadPatches",
    "PatchChanged",
    "PatchIndexAssign",
    "PatchCount",
    "CC_TARGET_ENUM_LIST",
    "CC_DEFAULT_MAPPINGS",
    "CC_TARGET_PARAMID_LIST",
    "CC_TARGET_FULL_RANGE",
    "NUM_CC_TARGETS"
]


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


def _generateModuleFile(fileField: str, m: dict):
    cppTargetFile = f"{cppTmpDir}/src/{fileField}"
    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{fileField}", m["CPP"], cppJuceFileVars)
    clangFormatFile = getTargetName(cppTargetFile, m)
    runClangFormat(clangFormatFile)


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
        if item['type'] == 'switch' and 'cc' in item:
            item['rangeStart'] = 0
            item['rangeEnd'] = 1
        if item['type'] in ['dial', 'switch'] and 'cc' in item:
            item = fillCc(item)
        item["keyUpper"] = item['symbol'][0].upper() + item['symbol'][1:]
        item["setter"] = "set"+item["keyUpper"]

    printParameterTable(m)

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
    m["CPP"].update(createCcMapping(m))

    m["CPP"]["INIT_WIDGETS"] = createInitWidgets(m)
    m["CPP"]["WIDGETS_DECL"] = createWidgetsDecl(m)
    m["CPP"]["RESIZED_AREA"] = construct_boxes(m)
    m["CPP"]["EXTRA_PRIVATE_METHODS"] = createExtraPrivateMethods(m)
    m["CPP"]["EXTRA_PROCESSOR_METHODS"] = createExtraProcessorMethods(m)
    m["CPP"]["EXTRA_PREPARE_CALLS"] = createExtraPrepareCalls(m)
    m["CPP"]["EXTRA_PROCESSOR_MEMBERS"] = createExtraProcessorMembers(m)
    m["CPP"]["EXTRA_GET_STATE_CALLS"] = createExtraGetStateCalls(m)
    m["CPP"]["EXTRA_SET_STATE_CALLS"] = createExtraSetStateCalls(m)

    for idx in range(len(m["ports-control"])):
        item = fillDefaults(m["ports-control"][idx])
        keyUpper = item['symbol'][0].upper() + item['symbol'][1:]

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
    if int(m["CPP"]["NUM_CC_TARGETS"]) > 0:
        m["CPP"]["GAUGES"].append("MIDICC")
    if m.get("patches", False):
        m["CPP"]["GAUGES"].append("PRESETBROWSER")
    if m.get("loops", False):
        m["CPP"]["GAUGES"].append("LOOPBROWSER")
    if m.get("host_transport", False):
        m["CPP"]["GAUGES"].append("HOSTTRANSPORT")

    for fileField in [cppJuceFile, cppJuceFileImplement, cppSourceFilesImpl,
                       cppSourceFilesImplFileIo, cppPatchParameters, cppCcMapping,
                       cppCcSettings, cppJuceFileEditor, cppSourceFilesUnitTest]:
        _generateModuleFile(fileField, m)

    cppTargetFile = f"{cppTmpDir}/src/{cppJuceFileConstants}"
    newConstants["Module"] = m["CPP"]["Module"]
    newConstants["module"] = m["CPP"]["module"]
    newConstants["WINDOW_WIDTH"] = m["layout"]["WINDOW_WIDTH"]
    newConstants["WINDOW_HEIGHT"] = m["layout"]["WINDOW_HEIGHT"]

    createAndSaveModuleSubstitutions(cppTargetFile, f"{sourceFiles}/{cppJuceFileConstants}", newConstants, cppConstants)
    clangFormatFile = getTargetName(cppTargetFile, m)
    runClangFormat(clangFormatFile)

    digest = hashlib.sha256(m["CPP"]["module"].encode("utf-8")).hexdigest()

    m["CPP"]["PluginCode"] = m["plugintype"][0] + digest[1:4].upper()
    m["CPP"]['IsSynth'] = "FALSE"
    m["CPP"]['NeedsMidiInput'] = "TRUE" if int(m["CPP"]["NUM_CC_TARGETS"]) > 0 else "FALSE"
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
    shutil.copytree(f"{sourceFiles}/inc/themes", f"{dir}/src/inc/themes", dirs_exist_ok=True)
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


def main():
    global stand_alone, cppJuceCmake, cppTargetDir, mainTargetDir

    moduleList = list_modules()

    parsed = parse_args(sys.argv, moduleList)
    if parsed is None:
        return

    if parsed.mode == "standalone":
        stand_alone = True
        cppJuceCmake = "CMakeListsStandalone.txt"

    if parsed.target_dir_overridden:
        cppTargetDir = parsed.target_dir_arg if "{module}" in parsed.target_dir_arg else f"{parsed.target_dir_arg}/{{module}}"
        mainTargetDir = parsed.target_dir_arg

    for m in parsed.modules_requested:
        if m in moduleList:
            cfg = loadConfig(m)
            if parsed.force_all:
                cfg["_force_all"] = True
            createPackageFromJsonDict(cfg)
        else:
            print(f'module "{m}" not found (use --list to obtain a list)')


if __name__ == "__main__":
    main()
