#!/usr/bin/env python3

"""
Gold-master regression test for generate-juce-standalone.py.

For every blueprint in blueprints/*.json, generates the module (--mode
localexample) into a throwaway scratch directory and compares the
generated/templated files against a committed baseline under gold_master/.

  --record   capture current generator output as the new baseline
  --check    (default) diff current output against the baseline
"""

import argparse
import filecmp
import json
import os
import shutil
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GENERATOR = os.path.join(SCRIPT_DIR, "generate-juce-standalone.py")
BLUEPRINTS_DIR = os.path.join(SCRIPT_DIR, "blueprints")
GOLD_MASTER_DIR = os.path.join(SCRIPT_DIR, "gold_master")
SCRATCH_DIR = os.path.join(SCRIPT_DIR, ".gm_scratch")
ROOT_CMAKE_LISTS = os.path.join(SCRIPT_DIR, "..", "CMakeLists.txt")

# Static files/dirs the generator copies verbatim (shutil.copy/copytree) rather
# than running through moduleSubstitutions/moduleSubstitutionsBraced. The split
# cannot change these, so they are excluded from the comparison.
STATIC_EXCLUDE_FILES = {
    "src/inc/GuiConstants.h",
    "src/inc/ThemeOrbit.h",
    "src/inc/CpuMeter.h",
    "src/inc/CustomRotaryDial.h",
    "src/inc/MomentaryToggleButton.h",
    "src/inc/GenericMeter.h",
    "src/inc/StatusBar.h",
    "src/inc/SpectrogramDisplay.h",
    "src/inc/VuMeter.h",
    "src/inc/WaveformMeter.h",
    "src/inc/SliceWaveDisplay.h",
    "src/inc/CircularBarDisplay.h",
    "src/inc/AppSettings.h",
    "src/impl/EffectBase.h",
    "src/inc/LookAndFeel.h",
    ".gitignore",
    "logo.png",
}
STATIC_EXCLUDE_DIRS = {
    "src/inc/themes",
}


def list_modules() -> list:
    modules = [
        f[:-5] for f in os.listdir(BLUEPRINTS_DIR) if f.endswith(".json")
    ]
    modules.sort()
    return modules


def gather_relevant(module_dir: str) -> dict:
    result = {}
    for root, dirs, files in os.walk(module_dir):
        rel_root = os.path.relpath(root, module_dir)
        rel_root = "" if rel_root == "." else rel_root.replace(os.sep, "/")
        dirs[:] = [
            d for d in dirs
            if (f"{rel_root}/{d}" if rel_root else d) not in STATIC_EXCLUDE_DIRS
        ]
        for fn in files:
            rel_path = f"{rel_root}/{fn}" if rel_root else fn
            if rel_path in STATIC_EXCLUDE_FILES:
                continue
            result[rel_path] = os.path.join(root, fn)
    return result


def blueprint_output_name(module: str) -> str:
    path = os.path.join(BLUEPRINTS_DIR, f"{module}.json")
    with open(path) as f:
        return json.load(f)["name"]


def generate_module(module: str) -> str:
    """Runs the generator for one module in its own scratch subdir (blueprints
    can share an output 'name', so modules must never share a target-dir) and
    returns the path to the generated module directory."""
    module_scratch = os.path.join(SCRATCH_DIR, module)
    if os.path.isdir(module_scratch):
        shutil.rmtree(module_scratch)
    os.makedirs(module_scratch)
    subprocess.run(
        [sys.executable, GENERATOR, "--mode", "localexample",
         "--target-dir", module_scratch, module],
        cwd=SCRIPT_DIR, check=True,
    )
    return os.path.join(module_scratch, blueprint_output_name(module))


def run_generator(modules: list) -> dict:
    if os.path.isdir(SCRATCH_DIR):
        shutil.rmtree(SCRATCH_DIR)
    os.makedirs(SCRATCH_DIR)

    with open(ROOT_CMAKE_LISTS, "rb") as f:
        cmake_before = f.read()
    try:
        return {module: generate_module(module) for module in modules}
    finally:
        with open(ROOT_CMAKE_LISTS, "wb") as f:
            f.write(cmake_before)


def do_record(modules: list) -> int:
    module_dirs = run_generator(modules)
    for module in modules:
        module_dir = module_dirs[module]
        baseline_dir = os.path.join(GOLD_MASTER_DIR, module)
        if os.path.isdir(baseline_dir):
            shutil.rmtree(baseline_dir)
        files = gather_relevant(module_dir)
        for rel_path, abs_path in files.items():
            dest = os.path.join(baseline_dir, rel_path)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copyfile(abs_path, dest)
        print(f"recorded {len(files)} files for {module}")
    return 0


def do_check(modules: list) -> int:
    module_dirs = run_generator(modules)
    failures = []
    for module in modules:
        module_dir = module_dirs[module]
        baseline_dir = os.path.join(GOLD_MASTER_DIR, module)
        if not os.path.isdir(baseline_dir):
            failures.append(f"{module}: no baseline recorded (run --record first)")
            continue

        generated = gather_relevant(module_dir)
        baseline = gather_relevant(baseline_dir)
        gen_paths = set(generated)
        base_paths = set(baseline)

        for missing in sorted(base_paths - gen_paths):
            failures.append(f"{module}: {missing} present in baseline but not generated")
        for extra in sorted(gen_paths - base_paths):
            failures.append(f"{module}: {extra} generated but not in baseline")

        for rel_path in sorted(gen_paths & base_paths):
            if not filecmp.cmp(generated[rel_path], baseline[rel_path], shallow=False):
                failures.append(f"{module}: {rel_path} differs")
                diff = subprocess.run(
                    ["diff", "-u", baseline[rel_path], generated[rel_path]],
                    capture_output=True, text=True,
                )
                print(diff.stdout)

    if failures:
        print(f"\n{len(failures)} mismatch(es):")
        for f in failures:
            print(f"  - {f}")
    else:
        print(f"OK: {len(modules)} module(s) match the gold master")
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument("--record", action="store_true",
                             help="capture current output as the new baseline")
    mode_group.add_argument("--check", action="store_true",
                             help="diff current output against the baseline (default)")
    parser.add_argument("modules", nargs="*",
                         help="blueprint modules to test (default: all)")
    args = parser.parse_args()

    modules = args.modules or list_modules()
    unknown = [m for m in modules if m not in list_modules()]
    if unknown:
        print(f"unknown module(s): {', '.join(unknown)}")
        return 2

    if args.record:
        return do_record(modules)
    return do_check(modules)


if __name__ == "__main__":
    sys.exit(main())
