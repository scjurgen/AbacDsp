import json
from typing import Any

Blueprint = dict[str, Any]


def load_config(module: str) -> Blueprint:
    json_file = f"blueprints/{module}.json"
    config_string = open(json_file).read()
    config = json.loads(config_string)
    config['VersionString'] = "0.0.0"
    return config

def fill_defaults(item: dict[str, Any]) -> dict[str, Any]:
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

def parse_and_fill_range(values: list) -> dict[str, Any]:
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

def fill_range(item: dict[str, Any]) -> dict[str, Any]:
    if 'range' in item:
        item.update(parse_and_fill_range(item['range']))
    else:
        print(f"Using default range {item}")
        item.update(parse_and_fill_range([0, 1, 0, 1, "false"]))
    return item

def fill_cc(item: dict[str, Any]) -> dict[str, Any]:
    cc = item['cc']
    if 'valueLow' not in cc:
        cc['valueLow'] = item['rangeStart']
    if 'valueHigh' not in cc:
        cc['valueHigh'] = item['rangeEnd']
    return item

def drop_choices(item: dict[str, Any]) -> list[str]:
    if isinstance(item.get('listitems'), list):
        return list(item['listitems'])
    return [str(item['listitems']).format(i + 1) for i in range(item.get('count', 0))]

def luacontrolarea_pool_ports(item: dict[str, Any]) -> list[dict[str, Any]]:
    """The hidden APVTS-backed pool a LuaControlArea widget draws its dynamic
    knobs/dropdowns/switches from - not shown in any composition, so it never gets a
    generator-owned widget of its own. count/ccbase default to 8 pool slots starting
    at CC16, matching DroneScriptEngine::kMaxLuaParams; keep both in sync by hand if
    either changes."""
    count = item.get('count', 8)
    cc_base = item.get('ccbase', 16)
    return [
        {
            'short': f'LP{idx + 1}',
            'type': 'dial',
            'display': f'Lua Param {idx + 1}',
            'symbol': f'luaParam{idx + 1}',
            'default': 0,
            'range': [0, 1, 0, 1, False],
            'precision': 2,
            'unit': '',
            'cc': {'controller': cc_base + idx},
        }
        for idx in range(count)
    ]


def enrich(blueprint: Blueprint) -> None:
    items_to_remove = []
    for item in blueprint['ports-control']:
        if item['type'] in ['dial', 'switch'] and 'count' in item:
            for idx in range(item['count']):
                new_item = {
                    'short': item['short'].format(idx + 1),
                    'type': item['type'],
                    'display': item['display'].format(idx + 1),
                    'symbol': item['symbol'].format(idx + 1),
                    'range': item['range'],
                    'precision': item['precision'],
                    'unit': item['unit'],
                }
                blueprint['ports-control'].append(new_item)
            items_to_remove.append(item)
        elif item['type'] == 'luacontrolarea':
            blueprint['ports-control'].extend(luacontrolarea_pool_ports(item))

    for item in items_to_remove:
        blueprint['ports-control'].remove(item)
