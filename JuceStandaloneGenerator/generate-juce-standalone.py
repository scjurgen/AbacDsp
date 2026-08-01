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

from blueprint import Blueprint, load_config, fill_defaults, fill_range, fill_cc, enrich
from report import print_parameter_table
from codegen_processor import (
    create_parameter_layout, create_parameter_changed, update_param_by_id, id_list,
    id_string_list, param_const_expr_list, load_patches, create_patch_changed,
    get_patch_count, create_patch_index_assign, create_setters_implementation,
    create_struct_variables_implementation, create_variables_implementation,
    add_parameter_listeners, remove_parameter_listeners,
    create_extra_processor_methods, create_extra_prepare_calls,
    create_extra_processor_members, create_extra_get_state_calls,
    create_extra_set_state_calls, create_cc_mapping,
)
from codegen_widgets import (
    gauge_present, create_gauge_callbacks, create_theme_callbacks,
    create_widgets_decl, create_init_widgets, create_extra_private_methods,
)
from template_engine import (
    get_target_name, run_clang_format, create_and_save_module_substitutions,
    create_and_save_module_substitutions_braced, GeneratorError,
)
from cli import list_modules, parse_args

# Every path below (blueprints/, templates/, ../examples) is relative to this
# script's own directory, not the caller's cwd.
os.chdir(os.path.dirname(os.path.abspath(__file__)))

stand_alone = False
cpp_juce_cmake = "CMakeListsExamples.txt"
cpp_target_dir = "../examples/{module}"
main_target_dir = "../examples"
new_constants = {
    "MAIN_COLOR": "0xffdddddd",
    "WINDOW_WIDTH": "1024",
    "WINDOW_HEIGHT": "600"
}

TEMPLATE_FILES = "templates"
SOURCE_FILES = f"{TEMPLATE_FILES}/sourcefiles"
CPP_TMP_DIR = "/tmp/{module}"
CPP_ALL_ELEMENTS_INCLUDE = "UiElements.h"
ROOT_CMAKE_LISTS = "../CMakeLists.txt"

CPP_LOOK_AND_FEEL = "inc/LookAndFeel.h"

CPP_SOURCE_FILES_FIXED = [
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
CPP_SOURCE_FILES_3RDPARTY = [
    "3rdparty/CMakeLists.txt",
]

CPP_JUCE_FILE = "{Module}Processor.h"
CPP_JUCE_FILE_IMPLEMENT = "{Module}Processor.cpp"
CPP_JUCE_FILE_EDITOR = "{Module}Editor.h"
CPP_JUCE_FILE_CONSTANTS = "{Module}Constants.h"

CPP_SOURCE_FILES_IMPL = "impl/GenericImpl.h"
CPP_SOURCE_FILES_IMPL_FILE_IO = "impl/FileIo.h"
CPP_PATCH_PARAMETERS = "impl/PatchParameters.h"
CPP_CC_MAPPING = "impl/CcMapping.h"
CPP_CC_SETTINGS = "impl/CcSettings.h"
CPP_SOURCE_FILES_UNIT_TEST = "unittests/{Module}_tests.cpp"

CPP_CONSTANTS = [
    "MAIN_COLOR",
    "WINDOW_WIDTH",
    "WINDOW_HEIGHT"
]

CPP_JUCE_FILE_VARS = [
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


def ensure_cmake_subdirectory(module: str) -> None:
    with open(ROOT_CMAKE_LISTS, "r") as f:
        lines = f.readlines()
    entry_pattern = re.compile(r'^(\s*)add_subdirectory\(examples/([A-Za-z0-9_]+)\)\s*$')
    entries = [(i, match.group(2)) for i, line in enumerate(lines) if (match := entry_pattern.match(line))]
    if not entries:
        print(f"WARNING: no add_subdirectory(examples/...) entries found in {ROOT_CMAKE_LISTS}, skipping")
        return
    if module in (mod for _, mod in entries):
        return
    indent = entry_pattern.match(lines[entries[0][0]]).group(1)
    insert_at = next((i for i, mod in entries if module < mod), entries[-1][0] + 1)
    lines.insert(insert_at, f"{indent}add_subdirectory(examples/{module})\n")
    with open(ROOT_CMAKE_LISTS, "w") as f:
        f.writelines(lines)
    print(f"Added add_subdirectory(examples/{module}) to {ROOT_CMAKE_LISTS}")


def _generate_module_file(file_field: str, blueprint: Blueprint) -> None:
    cpp_target_file = f"{CPP_TMP_DIR}/src/{file_field}"
    create_and_save_module_substitutions(cpp_target_file, f"{SOURCE_FILES}/{file_field}", blueprint["CPP"], CPP_JUCE_FILE_VARS)
    clang_format_file = get_target_name(cpp_target_file, blueprint)
    run_clang_format(clang_format_file)


def create_package_from_json_dict(blueprint: Blueprint) -> None:
    target_dir = f"{main_target_dir}/{blueprint['name']}"
    enrich(blueprint)
    print(f"Generating: {target_dir}")

    blueprint["module"] = blueprint["name"]
    blueprint["Module"] = blueprint["name"][0].upper() + blueprint["name"][1:]
    blueprint["description"] = "\n".join(blueprint["description"])
    blueprint["ports"] = "\n"
    blueprint["CPP"]["module"] = blueprint["name"]
    blueprint["CPP"]["name"] = blueprint["name"]
    blueprint["CPP"]["MODULE"] = blueprint["name"]
    blueprint["CPP"]["MODULE_UPPER"] = blueprint["Module"]
    blueprint["CPP"]["ADD_PARAMETERS"] = ""
    blueprint["CPP"]["ID_PARAMETERS"] = ""
    blueprint["CPP"]["NEW_RUNNER"] = ""
    blueprint["CPP"]["JUCE_PARAMS"] = ""
    blueprint["CPP"]["RUN_MODULE"] = ""

    for idx in range(len(blueprint["ports-control"])):
        item = fill_defaults(blueprint["ports-control"][idx])
        if item['type'] in ['dial', 'slider']:
            item = fill_range(item)
        if item['type'] == 'switch' and 'cc' in item:
            item['rangeStart'] = 0
            item['rangeEnd'] = 1
        if item['type'] in ['dial', 'switch'] and 'cc' in item:
            item = fill_cc(item)
        item["keyUpper"] = item['symbol'][0].upper() + item['symbol'][1:]
        item["setter"] = "set"+item["keyUpper"]

    print_parameter_table(blueprint)

    blueprint["CPP"]["TIMER_CALLBACKS"] = create_gauge_callbacks(blueprint)
    blueprint["CPP"]["APPLY_THEME_CALLBACKS"] = create_theme_callbacks(blueprint)
    blueprint["CPP"]["ADD_PARAMETER_LISTENERS"] = add_parameter_listeners(blueprint)
    blueprint["CPP"]["REMOVE_PARAMETER_LISTENERS"] = remove_parameter_listeners(blueprint)
    blueprint["CPP"]["CREATE_PARAMETER_LAYOUT"] = create_parameter_layout(blueprint)
    blueprint["CPP"]["PARAMETER_CHANGED"] = create_parameter_changed(blueprint)
    blueprint["CPP"]["SETTERS"] = create_setters_implementation(blueprint)
    blueprint["CPP"]["SETTERS_PARAMS"] = create_variables_implementation(blueprint)

    blueprint["CPP"]["ParamIdList"] = id_list(blueprint)
    blueprint["CPP"]["ParamIdStringList"] = id_string_list(blueprint)
    blueprint["CPP"]["ParamIdConstExprList"] = param_const_expr_list(blueprint)
    blueprint["CPP"]["UpdateParamById"] = update_param_by_id(blueprint)
    blueprint["CPP"]["LoadPatches"] = load_patches(blueprint)
    blueprint["CPP"]["PatchChanged"] = create_patch_changed(blueprint)
    blueprint["CPP"]["PatchIndexAssign"] = create_patch_index_assign(blueprint)
    blueprint["CPP"]["PatchCount"] = get_patch_count(blueprint)
    blueprint["CPP"]["ParamStructMembers"] = create_struct_variables_implementation(blueprint)
    blueprint["CPP"].update(create_cc_mapping(blueprint))

    blueprint["CPP"]["INIT_WIDGETS"] = create_init_widgets(blueprint)
    blueprint["CPP"]["WIDGETS_DECL"] = create_widgets_decl(blueprint)
    blueprint["CPP"]["RESIZED_AREA"] = construct_boxes(blueprint)
    blueprint["CPP"]["EXTRA_PRIVATE_METHODS"] = create_extra_private_methods(blueprint)
    blueprint["CPP"]["EXTRA_PROCESSOR_METHODS"] = create_extra_processor_methods(blueprint)
    blueprint["CPP"]["EXTRA_PREPARE_CALLS"] = create_extra_prepare_calls(blueprint)
    blueprint["CPP"]["EXTRA_PROCESSOR_MEMBERS"] = create_extra_processor_members(blueprint)
    blueprint["CPP"]["EXTRA_GET_STATE_CALLS"] = create_extra_get_state_calls(blueprint)
    blueprint["CPP"]["EXTRA_SET_STATE_CALLS"] = create_extra_set_state_calls(blueprint)

    for idx in range(len(blueprint["ports-control"])):
        item = fill_defaults(blueprint["ports-control"][idx])

        port_type = item['type']
        if port_type == 'drop':
            idx = 0
            menu_list = 'juce::StringArray {'
            for enum_item in item['listitems']:
                menu_list += f'"{enum_item}",'
                idx += 1
            menu_list = menu_list[:-1] + '}'
            item['minimum'] = 0
            item['maximum'] = idx - 1
        blueprint["CPP"]["ID_PARAMETERS"] += f'PARAMETER_ID({item["symbol"]})\n'
        blueprint["CPP"]["JUCE_PARAMS"] += f"    juce::AudioParameter{port_type}* {item['symbol']}; // {item['display']}\n"
        if item['type'] == 'integer':
            blueprint["CPP"]["JUCE_PARAMS"] += f"    int prev_{item['symbol']} {{ {item['minimum'] - 1} }};\n\n"
        elif item['type'] == 'Choice':
            val = item["default"] - 1
            if val < 0:
                val = val + 2
            blueprint["CPP"]["JUCE_PARAMS"] += f"    int prev_{item['symbol']} {{ {val} }};\n\n"
        elif item['type'] == 'bool':
            val = item["default"]
            val = not val
            blueprint["CPP"]["JUCE_PARAMS"] += f"    bool prev_{item['symbol']}{{ {val} }};\n\n"
        else:
            blueprint["CPP"]["JUCE_PARAMS"] += f"    float prev_{item['symbol']} {{ {item['minimum'] - 1} }};\n\n"

    if (blueprint["CPP"]["PROCMODE"] == "Stereo-Stereo"):
        blueprint["CPP"][
            "RUN_MODULE"] += """
            std::array<std::array<float, NumSamplesPerBlock>, 2> output;
            pluginRunner->processBlockStereoStereo(buffer.getReadPointer(0) + i, buffer.getReadPointer(1) + i, output[0].data(), output[1].data());
            std::copy_n(output[0].data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output[1].data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
           """
    elif (blueprint["CPP"]["PROCMODE"] == "Mono-Stereo"):
        blueprint["CPP"][
            "RUN_MODULE"] += """
            std::array<std::array<float, NumSamplesPerBlock>, 2> output;
            pluginRunner->processBlockMonoStereo(buffer.getReadPointer(0)+i, output[0].data(), output[1].data());
            std::copy_n(output[0].data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output[1].data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
            """
    elif (blueprint["CPP"]["PROCMODE"] == "Mono-Mono"):
        blueprint["CPP"]["RUN_MODULE"] += (
            """
            std::array<float, NumSamplesPerBlock> output;
            pluginRunner->processBlockMonoMono(buffer.getReadPointer(0)+i, output.data());
            std::copy_n(output.data(), NumSamplesPerBlock, buffer.getWritePointer(0) + i);
            std::copy_n(output.data(), NumSamplesPerBlock, buffer.getWritePointer(1) + i);
            """)
    elif (blueprint["CPP"]["PROCMODE"] == "Midi-Stereo"):
        blueprint["CPP"][
            "RUN_MODULE"] += """ pluginRunner->processBlockMidiStereo(buffer.getWritePointer(0)+i, buffer.getWritePointer(1)+i); """

    module_dir = CPP_TMP_DIR.replace("{module}", blueprint["module"])
    os.makedirs(f"{module_dir}/3rdparty", mode=0o777, exist_ok=True)
    os.makedirs(f"{module_dir}/src", mode=0o777, exist_ok=True)
    os.makedirs(f"{module_dir}/src/impl", mode=0o777, exist_ok=True)
    os.makedirs(f"{module_dir}/src/inc", mode=0o777, exist_ok=True)
    os.makedirs(f"{module_dir}/src/unittests", mode=0o777, exist_ok=True)

    blueprint["CPP"]["Module"] = blueprint["CPP"]["module"].capitalize()
    blueprint["CPP"]["GAUGES"] = gauge_present(blueprint)
    if get_patch_count(blueprint) > 0:
        blueprint["CPP"]["GAUGES"].append("PATCHSUPPORT")
    if int(blueprint["CPP"]["NUM_CC_TARGETS"]) > 0:
        blueprint["CPP"]["GAUGES"].append("MIDICC")
    if blueprint.get("patches", False):
        blueprint["CPP"]["GAUGES"].append("PRESETBROWSER")
    if blueprint.get("loops", False):
        blueprint["CPP"]["GAUGES"].append("LOOPBROWSER")
    if blueprint.get("host_transport", False):
        blueprint["CPP"]["GAUGES"].append("HOSTTRANSPORT")

    for file_field in [CPP_JUCE_FILE, CPP_JUCE_FILE_IMPLEMENT, CPP_SOURCE_FILES_IMPL,
                        CPP_SOURCE_FILES_IMPL_FILE_IO, CPP_PATCH_PARAMETERS, CPP_CC_MAPPING,
                        CPP_CC_SETTINGS, CPP_JUCE_FILE_EDITOR, CPP_SOURCE_FILES_UNIT_TEST]:
        _generate_module_file(file_field, blueprint)

    cpp_target_file = f"{CPP_TMP_DIR}/src/{CPP_JUCE_FILE_CONSTANTS}"
    new_constants["Module"] = blueprint["CPP"]["Module"]
    new_constants["module"] = blueprint["CPP"]["module"]
    new_constants["WINDOW_WIDTH"] = blueprint["layout"]["WINDOW_WIDTH"]
    new_constants["WINDOW_HEIGHT"] = blueprint["layout"]["WINDOW_HEIGHT"]

    create_and_save_module_substitutions(cpp_target_file, f"{SOURCE_FILES}/{CPP_JUCE_FILE_CONSTANTS}", new_constants, CPP_CONSTANTS)
    clang_format_file = get_target_name(cpp_target_file, blueprint)
    run_clang_format(clang_format_file)

    digest = hashlib.sha256(blueprint["CPP"]["module"].encode("utf-8")).hexdigest()

    blueprint["CPP"]["PluginCode"] = blueprint["plugintype"][0] + digest[1:4].upper()
    blueprint["CPP"]['IsSynth'] = "FALSE"
    blueprint["CPP"]['NeedsMidiInput'] = "TRUE" if int(blueprint["CPP"]["NUM_CC_TARGETS"]) > 0 else "FALSE"
    blueprint["CPP"]['NeedsMidiOutput'] = "FALSE"
    blueprint["CPP"]['IsMidiEffect'] = "FALSE"
    blueprint["CPP"]['EditorWantsKeyboardFocus'] = "FALSE"
    blueprint["CPP"]['VersionString'] = blueprint['VersionString']

    cpp_target_file = f"{CPP_TMP_DIR}/CMakeLists.txt"
    create_and_save_module_substitutions_braced(cpp_target_file, f"{SOURCE_FILES}/{cpp_juce_cmake}", blueprint["CPP"],
                                                 ['module', 'Module', 'PluginCode',
                                                  'IsSynth',
                                                  'NeedsMidiInput',
                                                  'NeedsMidiOutput',
                                                  'VersionString',
                                                  'IsMidiEffect',
                                                  'EditorWantsKeyboardFocus'])
    cpp_target_file = f"{CPP_TMP_DIR}/src/unittests/CMakeLists.txt"
    create_and_save_module_substitutions_braced(cpp_target_file, f"{SOURCE_FILES}/unittests/CMakeLists.txt", blueprint["CPP"],
                                                 ['module', 'Module', 'PluginCode',
                                                  'IsSynth',
                                                  'NeedsMidiInput',
                                                  'NeedsMidiOutput',
                                                  'VersionString',
                                                  'IsMidiEffect',
                                                  'EditorWantsKeyboardFocus'])
    with open(f"{module_dir}/VERSION", "w") as f:
        f.write(blueprint['VersionString'])
    shutil.copyfile(f"{SOURCE_FILES}/gitignore", f"{module_dir}/.gitignore")
    shutil.copyfile(f"{SOURCE_FILES}/logo.png", f"{module_dir}/logo.png")
    if stand_alone:
        shutil.copyfile(f"{TEMPLATE_FILES}/init-project.sh", f"{module_dir}/init-project.sh")
    for file_name in CPP_SOURCE_FILES_FIXED:
        shutil.copyfile(f"{SOURCE_FILES}/{file_name}", f"{module_dir}/src/{file_name}")
    shutil.copytree(f"{SOURCE_FILES}/inc/themes", f"{module_dir}/src/inc/themes", dirs_exist_ok=True)
    if stand_alone:
        for file_name in CPP_SOURCE_FILES_3RDPARTY:
            shutil.copyfile(f"{SOURCE_FILES}/{file_name}", f"{module_dir}/{file_name}")
    shutil.copyfile(f"{SOURCE_FILES}/{CPP_LOOK_AND_FEEL}", f"{module_dir}/src/{CPP_LOOK_AND_FEEL}")

    with open(f"{module_dir}/src/{CPP_ALL_ELEMENTS_INCLUDE}", "w") as f:
        f.write("#pragma once\n\n")
        f.write("/*\n * AUTO GENERATED,\n * NOT A GOOD IDEA TO CHANGE STUFF HERE\n */\n\n")
        f.write(f"""#include "{blueprint["Module"]}Constants.h"\n\n""")
        for file_name in CPP_SOURCE_FILES_FIXED:
            f.write(f"""#include "{file_name}"\n""")
        for extra in blueprint.get("extra_ui_includes", []):
            f.write(f"""#include "{extra}"\n""")
        f.write(f"""\n#include "{CPP_LOOK_AND_FEEL}"\n""")
    protected_files = {
        "gitignore",
        "src/unittests/CMakeLists.txt",
        "src/CMakeLists.txt",
        "CMakeLists.txt",
    }
    for pf in blueprint.get("protected_files", []):
        protected_files.add(pf)
    target_dir = cpp_target_dir.replace("{module}", blueprint["module"])
    force = blueprint.get("_force_all", False)
    syncer = FileSync(CPP_TMP_DIR.replace("{module}", blueprint["module"]), target_dir, set() if force else protected_files)
    syncer.sync()

    if not stand_alone:
        ensure_cmake_subdirectory(blueprint["module"])


def main() -> None:
    global stand_alone, cpp_juce_cmake, cpp_target_dir, main_target_dir

    module_list = list_modules()

    parsed = parse_args(sys.argv, module_list)
    if parsed is None:
        return

    if parsed.mode == "standalone":
        stand_alone = True
        cpp_juce_cmake = "CMakeListsStandalone.txt"

    if parsed.target_dir_overridden:
        cpp_target_dir = parsed.target_dir_arg if "{module}" in parsed.target_dir_arg else f"{parsed.target_dir_arg}/{{module}}"
        main_target_dir = parsed.target_dir_arg

    for module in parsed.modules_requested:
        if module in module_list:
            cfg = load_config(module)
            if parsed.force_all:
                cfg["_force_all"] = True
            try:
                create_package_from_json_dict(cfg)
            except GeneratorError as e:
                print(e)
                sys.exit(2)
        else:
            print(f'module "{module}" not found (use --list to obtain a list)')


if __name__ == "__main__":
    main()
