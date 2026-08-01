import os
import re


def getTargetName(target: str, m: dict):
    tmp = target.replace("{module}", m["module"])
    tmp = tmp.replace("{Module}", m["module"][0].upper() + m["module"][1:])
    return tmp

# clang-format leaves existing namespace-closing comments untouched even with
# FixNamespaceComments:false, so strip them explicitly after formatting.
# cppTmpDir stages files under /tmp, outside the repo, so a plain
# -style=file lookup can never find ../.clang-format and silently falls
# back to LLVM style; point at it explicitly instead.
def runClangFormat(target: str):
    clangFormatConfig = os.path.abspath("../.clang-format")
    os.system(f"clang-format -i -style=file:{clangFormatConfig} {target}")
    with open(target, "r") as f:
        content = f.read()
    stripped = re.sub(r'^(\})\s*//\s*namespace\b.*$', r'\1', content, flags=re.MULTILINE)
    if stripped != content:
        with open(target, "w") as f:
            f.write(stripped)

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
