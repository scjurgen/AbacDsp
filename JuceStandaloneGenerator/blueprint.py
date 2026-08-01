import json


def loadConfig(module: str):
    jsonFile = f"blueprints/{module}.json"
    configString = open(jsonFile).read()
    js = json.loads(configString)
    js['VersionString'] = "0.0.0"
    return js

def fillDefaults(item: dict):
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

def parse_and_fill_range(values):
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

def fillRange(item):
    if 'range' in item:
        item.update(parse_and_fill_range(item['range']))
    else:
        print(f"Using default range {item}")
        item.update(parse_and_fill_range([0, 1, 0, 1, "false"]))
    return item

def fillCc(item: dict):
    cc = item['cc']
    if 'valueLow' not in cc:
        cc['valueLow'] = item['rangeStart']
    if 'valueHigh' not in cc:
        cc['valueHigh'] = item['rangeEnd']
    return item

def dropChoices(item: dict) -> list:
    if isinstance(item.get('listitems'), list):
        return list(item['listitems'])
    return [str(item['listitems']).format(i + 1) for i in range(item.get('count', 0))]

def enrich(m: dict):
    items_to_remove = []
    for item in m['ports-control']:
        if item['type'] in ['dial', 'switch'] and 'count' in item:
            for idx in range(item['count']):
                newItem = dict()
                newItem['short'] = item['short'].format(idx + 1)
                newItem['type'] = item['type']
                newItem['display'] = item['display'].format(idx + 1)
                newItem['symbol'] = item['symbol'].format(idx + 1)
                newItem['range'] = item['range']
                newItem['precision'] = item['precision']
                newItem['unit'] = item['unit']
                m['ports-control'].append(newItem)
            items_to_remove.append(item)

    for item in items_to_remove:
        m['ports-control'].remove(item)
