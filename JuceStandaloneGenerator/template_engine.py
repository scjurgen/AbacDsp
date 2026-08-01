import os
import re

from blueprint import Blueprint


class GeneratorError(Exception):
    """Raised when a template file cannot be read or written; the CLI entry
    point catches this once and exits, instead of each function exiting."""


def get_target_name(target: str, blueprint: Blueprint) -> str:
    tmp = target.replace("{module}", blueprint["module"])
    tmp = tmp.replace("{Module}", blueprint["module"][0].upper() + blueprint["module"][1:])
    return tmp

# clang-format leaves existing namespace-closing comments untouched even with
# FixNamespaceComments:false, so strip them explicitly after formatting.
# cppTmpDir stages files under /tmp, outside the repo, so a plain
# -style=file lookup can never find ../.clang-format and silently falls
# back to LLVM style; point at it explicitly instead.
def run_clang_format(target: str) -> None:
    clang_format_config = os.path.abspath("../.clang-format")
    os.system(f"clang-format -i -style=file:{clang_format_config} {target}")
    with open(target, "r") as f:
        content = f.read()
    stripped = re.sub(r'^(\})\s*//\s*namespace\b.*$', r'\1', content, flags=re.MULTILINE)
    if stripped != content:
        with open(target, "w") as f:
            f.write(stripped)

# substitute all variables starting with // in the cpp/h templates
def module_substitutions(source: str, blueprint: Blueprint, keys: list[str]) -> str:
    try:
        with open(source, "r") as f:
            content = f.read()
            for key in keys:
                var_replace = '/*' + key + '*/'
                result = content.find(var_replace)
                if result != -1:
                    content = content.replace(var_replace, str(blueprint[key]))
                else:
                    pass
        return content
    except Exception as e:
        raise GeneratorError(f"AN ERROR occurred: {type(e).__name__} - {e}\n{os.getcwd()}") from e

def module_remove_remaining_section_from_string(content: str) -> str:
    pattern = re.compile(r'/\*START_[A-Z]+\*/.*?/\*END_[A-Z]+\*/\s?', re.DOTALL)
    return pattern.sub('', content)

def module_remove_section_indicator(content: str, name: str) -> str:
    pattern = re.compile(fr'/\*(START|END)_{name}\*/\s?')
    content = pattern.sub('', content)
    return content

def module_substitutions_braced(source: str, blueprint: Blueprint, keys: list[str]) -> str:
    try:
        with open(source, "r") as f:
            content = f.read()
            for key in keys:
                var_replace = '{' + key + '}'
                if content.find(var_replace) != -1:
                    content = content.replace(var_replace, str(blueprint[key]))
        return content
    except Exception as e:
        raise GeneratorError(f"AN ERROR occurred in braced substitution: {e}\n{os.getcwd()}") from e

def create_and_save_module_substitutions(target: str, source: str, blueprint: Blueprint, keys: list[str]) -> None:
    content = module_substitutions(source, blueprint, keys)
    if "GAUGES" in blueprint:
        for gauge in blueprint["GAUGES"]:
            content = module_remove_section_indicator(content, gauge)
    content = module_remove_remaining_section_from_string(content)
    try:
        with open(get_target_name(target, blueprint), "w") as tf:
            tf.write(content)
    except Exception as e:
        raise GeneratorError(f"AN ERROR while saving f{target} occurred: {e}\n{os.getcwd()}") from e


def create_and_save_module_substitutions_braced(target: str, source: str, blueprint: Blueprint, keys: list[str]) -> None:
    content = module_substitutions_braced(source, blueprint, keys)
    try:
        with open(get_target_name(target, blueprint), "w") as tf:
            tf.write(content)
    except Exception as e:
        raise GeneratorError(f"AN ERROR while saving f{target} occurred: {e}\n{os.getcwd()}") from e
