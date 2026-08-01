import os


def list_modules(blueprints_dir: str = "blueprints") -> list:
    modules = [f[:-5] for f in os.listdir(blueprints_dir) if f.endswith(".json")]
    modules.sort()
    return modules


def usage(progname: str, module_list: list):
    progname = os.path.basename(progname)
    print(f"Usage: {progname} --mode (standalone | localexample) [--forceall] [--target-dir <path>] module <module>...")
    print("module name of a module in blueprints ")
    print("--mode standalone     generate a standalone JUCE project; requires --target-dir")
    print("--mode localexample   generate an in-repo example under ../examples/{module}")
    print("--target-dir <path>  overwrite the output folder; mandatory with --mode standalone,")
    print("                     optional with --mode localexample (overrides its default target);")
    print("                     put {module} in <path> to control where the module name is inserted,")
    print("                     otherwise it is appended as a subfolder")
    print(module_list)
    exit(1)


class ParsedArgs:
    def __init__(self):
        self.force_all = False
        self.mode = None
        self.target_dir_overridden = False
        self.target_dir_arg = None
        self.modules_requested = []


def parse_args(argv: list, module_list: list):
    """Mirrors generate-juce-standalone.py's original argv-parsing block.
    Returns None if --list was handled (caller should just exit), else a
    ParsedArgs. --help/-h and error paths exit via usage(), as before."""
    if len(argv) == 1:
        usage(argv[0], module_list)

    if argv[1] == '--list':
        s = ""
        for m in module_list:
            s += m + " "
        print(s)
        return None
    elif argv[1] == '-h' or argv[1] == '--help':
        usage(argv[0], module_list)

    parsed = ParsedArgs()
    i = 1
    while i < len(argv):
        m = argv[i]
        if m == "--mode":
            i += 1
            if i >= len(argv):
                print("--mode requires an argument (standalone or localexample)")
                usage(argv[0], module_list)
            parsed.mode = argv[i]
            if parsed.mode not in ("standalone", "localexample"):
                print(f'unknown mode "{parsed.mode}" (expected standalone or localexample)')
                usage(argv[0], module_list)
        elif m == "--forceall":
            parsed.force_all = True
        elif m == "--target-dir":
            i += 1
            if i >= len(argv):
                print("--target-dir requires a path argument")
                usage(argv[0], module_list)
            parsed.target_dir_arg = os.path.abspath(os.path.expanduser(argv[i]))
            parsed.target_dir_overridden = True
        elif m[0] == '-':
            print(f'unknown option {m}')
            usage(argv[0], module_list)
        else:
            parsed.modules_requested.append(m)
        i += 1

    if parsed.mode is None:
        print("--mode is required (standalone or localexample)")
        usage(argv[0], module_list)

    if parsed.mode == "standalone" and not parsed.target_dir_overridden:
        print("--target-dir is required when --mode standalone is used")
        usage(argv[0], module_list)

    return parsed
