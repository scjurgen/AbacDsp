#!/usr/bin/env python3

import re

patterns = ["-",  # (C1) (A1) (h)
            "|",  # (C1) (A1) (v)
            "+",  # (C1:C2 R1:R2) (A1-4) (hh hh)
            "⧺",  # (C1:C3 R1:R2) (A1-6) (vv vv vv) // TODO
            "╪",  # (C1:C2 R1:R3) (A1-6) (hh hh hh) // TODO
            "#",  # (C1:C2:C3  R1:R2:R3) (A1-9) (hhh hhh hhh) // TODO
            "||",  # (C1:C2) (A1-2) (vv)
            "|||",  # (C1:C2:C3) (A1-3) (vvv)
            "||||",  # (C1:C2:C3:C4) (A1-4) (vvvv)
            "|||||",  # (C1:C2:C3:C4:C5) (A1-5) (vvvvv)
            "||||||",  # (C1:C2:C3:C4:C5:C6) (A1-6) (vvvvvv)
            "|||||||",  # (C1:C2:C3:C4:C5:C6:C7) (A1-7) (vvvvvvv)
            "|-"  # (C1:C2) (A1-2) (vh)
            "=",  # (R1:R2) (A1-2) (hh)
            "≡",  # (R1:R2:R3) (A1-A) (hhh)
            "=4",  # (R1:R2:R3:R4) (A1-A) (hhhh) // TODO
            "=5",  # (R1:R2:R3:R4:R5) (A1-5) (hhhhh)
            "=|",  # (C1:C2  R1:R2)  (A1-3) (hh v) // TODO
            "|=",  # (C1:C2  R1:R2)  (A1-3) (v hh)
            "|=|",  # (C1:C2:C3  R1:R2)  (A1-4) (v h v) // TODO
            "|≡",  # (C1:C2  R1:R2:R3)  (A1-4) (v hhh)
            "≡|",  # (C1:C2  R1:R2:R3)  (A1-4) (hhh v) // TODO
            "|≡|",  # (C1:C2  R1:R2:R3)  (A1-5) (v hhh v) // TODO
            "|=3",  # (C1:C2  R1:R2:R3)  (A1-5) (v hh)
            "|=4",  # (C1:C2  R1:R2:R3:R4)  (A1-5) (v hh)
            "|=5",  # (C1:C2  R1:R2:R3:R4:R5)  (A1-6) (v hh)
            ]


# column elements: |
# row elements: -=≡
# col/row elements: #+

# space distributions
# C1:C2:Cn R1:R2:Rn
# Example usage
# input_str = "C=(1:1) R=(1:2) A1=(MIX*1,SHP*1,DPT*1) A2=(DPT1*1,DNS*1.5f) A3=(FFT*3)"


def parse_box_structure(input_str: str):
    columns_definition = re.search(r'C=?\((.*?)\)', input_str)
    if columns_definition is not None:
        columns = list(map(int, columns_definition.group(1).split(':')))
    else:
        columns = [1]

    rows_definition = re.search(r'R=?\((.*?)\)', input_str)
    if rows_definition is not None:
        rows = list(map(int, rows_definition.group(1).split(':')))
    else:
        rows = [1]
    max_area = 0

    area_sections = re.findall(r'A(\d+)=?(\(.*?\))', input_str)
    for idx, area_content in area_sections:
        if int(idx) > max_area:
            max_area = int(idx)
    areas = [[] for _ in range(max_area + 1)]

    for idx, area_content in area_sections:
        items = area_content.strip('()').split(',')
        area_dict = []
        for item in items:
            if '*' in item:
                symbol, size = item.split('*')
                flex = "abs"
            elif '%' in item:
                symbol, size = item.split('%')
                flex = "stretch"
            else:
                symbol = item
                size = 1
                flex = "stretch"
            area_dict.append({"symbol": symbol.strip(), "size": size, "flex": flex})
        areas[int(idx)] = area_dict

    return columns, rows, areas


def construct_boxes(m: dict, section: str = 'layout'):
    columns, rows, areas = parse_box_structure(m[section]['composition'])
    result = ""

    def findShortEntry(short: str):
        for item in m["ports-control"]:
            if item["short"] == short:
                return item
        print(f"ERROR {short} not found in ports-control")
        exit(3)

    def saveArea(areas: list, idx: int, isColumn: bool):
        if isColumn:
            direction = "column"
            withDirection = "withHeight"
            labelSize = "labelHeight"
        else:
            direction = "row"
            withDirection = "withWidth"
            labelSize = "labelWidth"

        result = f"""{{\n"""
        result += f"""juce::FlexBox box;\n"""
        result += f"""box.flexWrap = juce::FlexBox::Wrap::noWrap;\n"""
        result += f"""box.flexDirection = juce::FlexBox::Direction::{direction};\n"""
        result += f"""box.justifyContent = juce::FlexBox::JustifyContent::spaceAround;\n"""

        if idx >= len(areas):
            print(f"ERROR: missing area {idx}")
            exit(4)

        for item in areas[idx]:
            p = findShortEntry(item["symbol"])
            # "script" is the one type whose widget suffix ("Button") isn't just its own
            # capitalized name - every other type's suffix happens to equal that already.
            widget_suffix = "Button" if p['type'] == 'script' else p['type'].capitalize()
            var = f"""{p["symbol"]}{widget_suffix}"""
            flex_line = f"""box.items.add(juce::FlexItem({var})"""
            match p['type']:
                case 'dial':
                    flex_line += f""".withFlex({item["size"]})"""
                case 'drop':
                    if item["flex"] == "abs":
                        flex_line += f".withFlex(0).withWidth({item['size']}).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::center)"
                    elif isColumn:
                        flex_line += f".withFlex(0).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::stretch)"
                    else:
                        flex_line += f".withFlex(1).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::center)"
                case 'gauge':
                    if item['flex'] == 'abs':
                        flex_line += f""".{withDirection}({item["size"]})"""
                    else:
                        flex_line += f""".withFlex({item["size"]})"""
                case 'switch':
                    if isColumn:
                        flex_line += ".withFlex(0).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::stretch)"
                    else:
                        flex_line += ".withWidth(Constants::Text::labelWidth).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::center)"
                case 'label':
                    if isColumn:
                        flex_line += ".withFlex(0).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::stretch)"
                    else:
                        flex_line += ".withWidth(Constants::Text::labelWidth).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::center)"
                case 'script':
                    if isColumn:
                        flex_line += ".withFlex(0).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::stretch)"
                    else:
                        flex_line += ".withWidth(Constants::Text::labelWidth).withHeight(Constants::Text::labelHeight).withAlignSelf(juce::FlexItem::AlignSelf::center)"
            flex_line += ".withMargin(knobMarginSmall));\n"
            if "visible_when" in p:
                result += f"""if ({var}.isVisible()) {{\n{flex_line}}}\n"""
            else:
                result += flex_line
        result += f"""box.performLayout(areas[{idx - 1}].toFloat());\n}}\n"""
        return result

    def saveAreaColumn(areas: dict, idx: int):
        return saveArea(areas, idx, True)

    def saveAreaRow(areas: dict, idx: int):
        return saveArea(areas, idx, False)

    def saveoneColumnMultiRows(rowCnt: int):
        result = f"""
        std::vector<juce::Rectangle<int>> areas({rowCnt + 1});
        const auto colWidth = area.getWidth() / {virtual_columns};
        const auto rowHeight = area.getHeight() / {virtual_rows};
        areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);\n"""
        for idx in range(rowCnt - 1):
            result += f"""areas[{idx + 1}] = area.removeFromTop(rowHeight*{rows[idx]}).reduced(Constants::Margins::small);\n"""
        result += f"""areas[{rowCnt}] = area.reduced(Constants::Margins::small);\n\n"""
        result += saveAreaColumn(areas, 1)
        for idx in range(rowCnt):
            result += saveAreaRow(areas, idx + 2)
        return result

    virtual_columns = sum(columns)
    virtual_rows = sum(rows)
    result += f"""
    // auto generated
        // const juce::FlexItem::Margin knobMargin = juce::FlexItem::Margin(Constants::Margins::small);
        const juce::FlexItem::Margin knobMarginSmall = juce::FlexItem::Margin(Constants::Margins::medium);        
        """
    match m[section]['type']:
        case '-':
            result += f"""std::vector<juce::Rectangle<int>> areas(1);
                    areas[0] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
        case '|':
            result += f"""std::vector<juce::Rectangle<int>> areas(1);
                    const auto colWidth = area.getWidth() / {virtual_columns};
                    areas[0] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
        case '||':
            result += f"""std::vector<juce::Rectangle<int>> areas(2);
                    const auto colWidth = area.getWidth() / {virtual_columns};
                       areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                       areas[1] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
        case '|||':
            result += f"""std::vector<juce::Rectangle<int>> areas(3);
                    const auto colWidth = area.getWidth() / {virtual_columns};
                           areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                           areas[1] = area.removeFromLeft(colWidth*{columns[1]}).reduced(Constants::Margins::small);
                           areas[2] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
            result += saveAreaColumn(areas, 3)
        case '||||':
            result += f"""std::vector<juce::Rectangle<int>> areas(4);
                    const auto colWidth = area.getWidth() / {virtual_columns};
                           areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                           areas[1] = area.removeFromLeft(colWidth*{columns[1]}).reduced(Constants::Margins::small);
                           areas[2] = area.removeFromLeft(colWidth*{columns[2]}).reduced(Constants::Margins::small);
                           areas[3] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
            result += saveAreaColumn(areas, 3)
            result += saveAreaColumn(areas, 4)
        case '|||||':
            result += f"""std::vector<juce::Rectangle<int>> areas(5);
                    const auto colWidth = area.getWidth() / {virtual_columns};
                           areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                           areas[1] = area.removeFromLeft(colWidth*{columns[1]}).reduced(Constants::Margins::small);
                           areas[2] = area.removeFromLeft(colWidth*{columns[2]}).reduced(Constants::Margins::small);
                           areas[3] = area.removeFromLeft(colWidth*{columns[3]}).reduced(Constants::Margins::small);
                           areas[4] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
            result += saveAreaColumn(areas, 3)
            result += saveAreaColumn(areas, 4)
            result += saveAreaColumn(areas, 5)
        case '||||||':
            result += f"""std::vector<juce::Rectangle<int>> areas(6);
                        const auto colWidth = area.getWidth() / {virtual_columns};
                               areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                               areas[1] = area.removeFromLeft(colWidth*{columns[1]}).reduced(Constants::Margins::small);
                               areas[2] = area.removeFromLeft(colWidth*{columns[2]}).reduced(Constants::Margins::small);
                               areas[3] = area.removeFromLeft(colWidth*{columns[3]}).reduced(Constants::Margins::small);
                               areas[4] = area.removeFromLeft(colWidth*{columns[4]}).reduced(Constants::Margins::small);
                               areas[5] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
            result += saveAreaColumn(areas, 3)
            result += saveAreaColumn(areas, 4)
            result += saveAreaColumn(areas, 5)
            result += saveAreaColumn(areas, 6)
        case '|||||||':
            result += f"""std::vector<juce::Rectangle<int>> areas(7);
                        const auto colWidth = area.getWidth() / {virtual_columns};
                               areas[0] = area.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                               areas[1] = area.removeFromLeft(colWidth*{columns[1]}).reduced(Constants::Margins::small);
                               areas[2] = area.removeFromLeft(colWidth*{columns[2]}).reduced(Constants::Margins::small);
                               areas[3] = area.removeFromLeft(colWidth*{columns[3]}).reduced(Constants::Margins::small);
                               areas[4] = area.removeFromLeft(colWidth*{columns[4]}).reduced(Constants::Margins::small);
                               areas[5] = area.removeFromLeft(colWidth*{columns[5]}).reduced(Constants::Margins::small);
                               areas[6] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaColumn(areas, 1)
            result += saveAreaColumn(areas, 2)
            result += saveAreaColumn(areas, 3)
            result += saveAreaColumn(areas, 4)
            result += saveAreaColumn(areas, 5)
            result += saveAreaColumn(areas, 6)
            result += saveAreaColumn(areas, 7)
        case '=':
            result += f"""std::vector<juce::Rectangle<int>> areas(2);
                   const auto rowHeight = area.getHeight() / {virtual_rows};
                                   areas[0] = area.removeFromTop(rowHeight*{rows[0]}).reduced(Constants::Margins::small);
                                   areas[1] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
            result += saveAreaRow(areas, 2)
        case '≡':
            result += f"""std::vector<juce::Rectangle<int>> areas(3);
                   const auto rowHeight = area.getHeight() / {virtual_rows};
                                   areas[0] = area.removeFromTop(rowHeight*{rows[0]}).reduced(Constants::Margins::small);
                                   areas[1] = area.removeFromTop(rowHeight*{rows[1]}).reduced(Constants::Margins::small);
                                   areas[2] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
            result += saveAreaRow(areas, 2)
            result += saveAreaRow(areas, 3)
        case '=5':
            result += f"""std::vector<juce::Rectangle<int>> areas(5);
                   const auto rowHeight = area.getHeight() / {virtual_rows};
                                   areas[0] = area.removeFromTop(rowHeight*{rows[0]}).reduced(Constants::Margins::small);
                                   areas[1] = area.removeFromTop(rowHeight*{rows[1]}).reduced(Constants::Margins::small);
                                   areas[2] = area.removeFromTop(rowHeight*{rows[2]}).reduced(Constants::Margins::small);
                                   areas[3] = area.removeFromTop(rowHeight*{rows[3]}).reduced(Constants::Margins::small);
                                   areas[4] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
            result += saveAreaRow(areas, 2)
            result += saveAreaRow(areas, 3)
            result += saveAreaRow(areas, 4)
            result += saveAreaRow(areas, 5)
        case '=6':
            result += f"""std::vector<juce::Rectangle<int>> areas(6);
                   const auto rowHeight = area.getHeight() / {virtual_rows};
                                   areas[0] = area.removeFromTop(rowHeight*{rows[0]}).reduced(Constants::Margins::small);
                                   areas[1] = area.removeFromTop(rowHeight*{rows[1]}).reduced(Constants::Margins::small);
                                   areas[2] = area.removeFromTop(rowHeight*{rows[2]}).reduced(Constants::Margins::small);
                                   areas[3] = area.removeFromTop(rowHeight*{rows[3]}).reduced(Constants::Margins::small);
                                   areas[4] = area.removeFromTop(rowHeight*{rows[4]}).reduced(Constants::Margins::small);
                                   areas[5] = area.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
            result += saveAreaRow(areas, 2)
            result += saveAreaRow(areas, 3)
            result += saveAreaRow(areas, 4)
            result += saveAreaRow(areas, 5)
            result += saveAreaRow(areas, 6)
        case '+':
            result += f"""
                        std::vector<juce::Rectangle<int>> areas(4);
                        const auto colWidth = area.getWidth() / {virtual_columns};
                        const auto rowHeight = area.getHeight() / {virtual_rows};
                        auto topArea = area.removeFromTop(rowHeight*{rows[0]}).reduced(Constants::Margins::small);
                        auto keepArea = area;
                        areas[0] = topArea.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                        areas[1] = topArea.reduced(Constants::Margins::small);
                        areas[2] = keepArea.removeFromLeft(colWidth*{columns[0]}).reduced(Constants::Margins::small);
                        areas[3] = keepArea.reduced(Constants::Margins::small);\n\n"""
            result += saveAreaRow(areas, 1)
            result += saveAreaRow(areas, 2)
            result += saveAreaRow(areas, 3)
            result += saveAreaRow(areas, 4)
        case '|=':
            rowCnt = 2
            result += saveoneColumnMultiRows(rowCnt)
        case '|=3':
            rowCnt = 3
            result += saveoneColumnMultiRows(rowCnt)
        case '|=4':
            rowCnt = 4
            result += saveoneColumnMultiRows(rowCnt)
        case '|=5':
            rowCnt = 5
            result += saveoneColumnMultiRows(rowCnt)
        case '|=6':
            rowCnt = 6
            result += saveoneColumnMultiRows(rowCnt)
        case '|=7':
            rowCnt = 7
            result += saveoneColumnMultiRows(rowCnt)
        case _:
            raise ValueError(f"Unsupported layout type: {m['layout']['type']}")

    return result
