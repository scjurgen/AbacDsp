#!/usr/bin/env python3

import os
import shutil
from typing import List, Tuple
from genSinc import make_filter

HEADER_DIRECTORY = ""
TARGET_CPP = "SincFilter.cpp"
TARGET_FOLDER_CPP = "./"
TARGET_FOLDER_INCLUDE = "./"

PARAMETERS: List[Tuple[float, int, float]] = [
    (69, 768, 170.0),
    (33, 512, 170.0),
    (21, 512, 180.0),
    (13, 256, 180.0),
    (11, 128, 160.0),
    (8,  128, 100.0),
    (7,  128,  90.0),
    (6,  128,  87.2),
    (5,  128,  77.6),
    (4,  128,  55.0),
    (3,  128,  44.3),
    (2,  128,  27.2),
]


def _cycle_str(cycle: float) -> str:
    return str(int(cycle)) if cycle == int(cycle) else str(cycle).replace(".", "p")

def create_filter(cycle: float, steps: int, attenuation: float) -> str:
    try:
        make_filter(cycle, steps, attenuation)
        src = f"Sinc{int(cycle)}.h"
        dst = f"sinc_{_cycle_str(cycle)}.h"
        os.rename(src, dst)
        print(f"File generated: {cycle}, {steps}, {attenuation} -> {dst}")
        return dst
    except Exception as e:
        print(f"Error generating filter ({cycle}, {steps}, {attenuation}): {e}")
        return ""

def move_include_files(generated_files: List[str], destination_folder: str) -> None:
    os.makedirs(destination_folder, exist_ok=True)
    for file in generated_files:
        if file and os.path.exists(file):
            dst = os.path.join(destination_folder, file)
            shutil.move(file, dst)
            print(f"Moved {file} to {dst}")
        else:
            print(f"File {file} not found for moving.")

def save_results(generated_files: List[str], parameters: List[Tuple[float, int, float]], target_cpp: str) -> None:
    lines = [f'#include "{HEADER_DIRECTORY}SincFilter.h"\n']
    for file in generated_files:
        lines.append(f'#include "{HEADER_DIRECTORY}{file}"')

    lines.append('\nnamespace AbacDsp\n{')
    lines.append('const std::vector sincFilterSet{')
    for cycle, steps, _ in parameters:
        lines.append(f'    SincFilter{{init_{_cycle_str(cycle)}}},')
    lines.append('};\n}')

    with open(target_cpp, 'w') as f:
        f.write('\n'.join(lines))

    dst = os.path.join(TARGET_FOLDER_CPP, target_cpp)
    shutil.move(target_cpp, dst)
    print(f"{target_cpp} moved to {dst}")


def main() -> None:
    generated_files = []
    for cycle, steps, attenuation in PARAMETERS:
        filename = create_filter(cycle, steps, attenuation)
        if filename:
            generated_files.append(filename)

    save_results(generated_files, PARAMETERS, TARGET_CPP)
    move_include_files(generated_files, TARGET_FOLDER_INCLUDE)


if __name__ == "__main__":
    main()
