from typing import Any

from blueprint import Blueprint, drop_choices


def format_param_range(item: dict[str, Any]) -> str:
    match item['type']:
        case 'dial' | 'slider':
            unit = item.get('unit', '')
            return f"{item['rangeStart']}..{item['rangeEnd']} {unit}".strip()
        case 'switch':
            return "on/off"
        case 'drop':
            return f"{len(drop_choices(item))} choices"
        case _:
            return "-"

def format_param_default(item: dict[str, Any]) -> str:
    match item['type']:
        case 'dial' | 'slider':
            unit = item.get('unit', '')
            return f"{item['default']} {unit}".strip()
        case 'switch':
            return "on" if item['default'] else "off"
        case 'drop':
            choices = drop_choices(item)
            idx = item['default']
            return choices[idx] if 0 <= idx < len(choices) else str(idx)
        case _:
            return "-"

def format_param_automation(item: dict[str, Any]) -> str:
    if item['type'] not in ['dial', 'switch', 'drop']:
        return "-"
    automation = "Host"
    if 'cc' in item:
        automation += f", CC {item['cc']['controller']}"
    return automation

def print_parameter_table(blueprint: Blueprint) -> None:
    rows = [(item['symbol'], format_param_range(item), format_param_default(item), format_param_automation(item))
            for item in blueprint['ports-control']]
    if not rows:
        return
    headers = ("Parameter", "Range", "Default", "Automation")
    widths = [max(len(headers[col]), max(len(row[col]) for row in rows)) for col in range(4)]

    def format_row(cols):
        return "  ".join(col.ljust(widths[i]) for i, col in enumerate(cols))

    print(f"\t{format_row(headers)}")
    print(f"\t{format_row(['-' * w for w in widths])}")
    for row in rows:
        print(f"\t{format_row(row)}")
