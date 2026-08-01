from blueprint import dropChoices


def formatParamRange(item: dict) -> str:
    match item['type']:
        case 'dial' | 'slider':
            unit = item.get('unit', '')
            return f"{item['rangeStart']}..{item['rangeEnd']} {unit}".strip()
        case 'switch':
            return "on/off"
        case 'drop':
            return f"{len(dropChoices(item))} choices"
        case _:
            return "-"

def formatParamDefault(item: dict) -> str:
    match item['type']:
        case 'dial' | 'slider':
            unit = item.get('unit', '')
            return f"{item['default']} {unit}".strip()
        case 'switch':
            return "on" if item['default'] else "off"
        case 'drop':
            choices = dropChoices(item)
            idx = item['default']
            return choices[idx] if 0 <= idx < len(choices) else str(idx)
        case _:
            return "-"

def formatParamAutomation(item: dict) -> str:
    if item['type'] not in ['dial', 'switch', 'drop']:
        return "-"
    automation = "Host"
    if 'cc' in item:
        automation += f", CC {item['cc']['controller']}"
    return automation

def printParameterTable(m: dict):
    rows = [(item['symbol'], formatParamRange(item), formatParamDefault(item), formatParamAutomation(item))
            for item in m['ports-control']]
    if not rows:
        return
    headers = ("Parameter", "Range", "Default", "Automation")
    widths = [max(len(headers[col]), max(len(row[col]) for row in rows)) for col in range(4)]

    def formatRow(cols):
        return "  ".join(col.ljust(widths[i]) for i, col in enumerate(cols))

    print(f"\t{formatRow(headers)}")
    print(f"\t{formatRow(['-' * w for w in widths])}")
    for row in rows:
        print(f"\t{formatRow(row)}")
