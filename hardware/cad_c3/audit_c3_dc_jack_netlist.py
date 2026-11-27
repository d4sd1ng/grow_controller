#!/usr/bin/env python3
"""Static named-net audit for the C3 low-voltage PCB and service-disconnect candidate.

This checks the *intended* pad-to-net assignments that KiCad DRC cannot infer
from floating/unassigned pads. It does NOT establish safety or manufacture approval.
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

BOARD = Path(__file__).with_name("c3_6motor_DC_J1_PJ037AH_CANDIDATE_NOT_FOR_ORDER.kicad_pcb")


def extract_footprints(data: str) -> dict[str, dict[str, str]]:
    result: dict[str, dict[str, str]] = {}
    for match in re.finditer(r'\(footprint\s+"([^"]+)"', data):
        start = match.start()
        depth = 0
        quoted = False
        escaped = False
        end = None
        for pos in range(start, len(data)):
            char = data[pos]
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    end = pos + 1
                    break
        if end is None:
            raise AssertionError(f"Unterminated footprint near offset {start}")
        block = data[start:end]
        ref_match = re.search(r'\(fp_text\s+reference\s+"([^"]+)"', block)
        if ref_match is None:
            raise AssertionError("Footprint without reference")
        ref = ref_match.group(1)
        if ref in result:
            raise AssertionError(f"Duplicate reference: {ref}")
        pads = {}
        for line in block.splitlines():
            if "(pad " not in line:
                continue
            match_pad = re.search(r'\(pad\s+"([^"]*)"\s+', line)
            if match_pad is None or not match_pad.group(1):
                continue
            n = re.search(r'\(net\s+(\d+)\s+"([^"]+)"\)', line)
            pads[match_pad.group(1)] = n.group(2) if n else "<UNASSIGNED>"
        result[ref] = pads
    return result


def expect(pads: dict[str, dict[str, str]], ref: str, mapping: dict[str, str]) -> None:
    if ref not in pads:
        raise AssertionError(f"Missing {ref}")
    for pin, expected in mapping.items():
        actual = pads[ref].get(str(pin), "<MISSING>")
        if actual != expected:
            raise AssertionError(
                f"{ref} pin {pin}: expected {expected!r}, got {actual!r}"
            )


def main() -> None:
    data = BOARD.read_text(encoding="utf-8")
    pads = extract_footprints(data)
    expected = '(footprint "Grow_Controller_C3:SameSky_PJ-037AH_THT_2P"'
    if expected not in data:
        raise AssertionError("J1 must be the PJ-037AH DC barrel jack, not the old Phoenix terminal")
    if '(footprint "Grow_Controller_C3:MSTBA_2P_5p08_NORTH" (layer "F.Cu") (at 16 20)' in data:
        raise AssertionError("Old Phoenix J1 footprint still present")
    expect(pads, "J1", {"1": "+12V_IN", "2": "GND"})
    expect(pads, "U_C3", {
        "1": "+5V_PROTECTED", "2": "GND", "3": "3V3_LOGIC",
        "11": "I2C_SDA_GPIO7", "14": "I2C_SCL_GPIO10", "15": "PCA_OE_GPIO20",
    })
    expect(pads, "U_PCA", {
        "1": "GND", "2": "GND", "3": "GND", "4": "GND", "5": "GND",
        "14": "GND", "23": "PCA_OE_GPIO20", "24": "GND",
        "25": "GND", "26": "I2C_SCL_GPIO10", "27": "I2C_SDA_GPIO7",
        "28": "3V3_LOGIC",
    })
    expect(pads, "U_BUCK", {
        "1": "+12V_IN", "2": "GND", "3": "+5V_BUCK",
    })
    expect(pads, "D1", {
        "1": "+5V_DIODE", "2": "+5V_BUCK",
    })
    expect(pads, "JP1", {"1": "+5V_DIODE", "2": "+5V_PROTECTED"})
    expect(pads, "R_SDA", {"1": "3V3_LOGIC", "2": "I2C_SDA_GPIO7"})
    expect(pads, "R_SCL", {"1": "3V3_LOGIC", "2": "I2C_SCL_GPIO10"})
    expect(pads, "R_OE", {"1": "3V3_LOGIC", "2": "PCA_OE_GPIO20"})
    expect(pads, "C_PCA", {"1": "3V3_LOGIC", "2": "GND"})
    for number in range(1, 7):
        ctrl = f"M{number}_CTRL"
        gate = f"M{number}_GATE"
        switched = f"M{number}_SW"
        expect(pads, "U_PCA", {str(5 + number): ctrl})
        expect(pads, f"RG{number}", {"1": ctrl, "2": gate})
        expect(pads, f"RPD{number}", {"1": "GND", "2": gate})
        expect(pads, f"QM{number}", {"1": gate, "2": "GND", "3": switched})
        expect(pads, f"DM{number}", {"1": "+12V_IN", "2": switched})
        expect(pads, f"J{number+6}", {"1": "+12V_IN", "2": switched})

    if any("230V" in net or "AC_MAINS" in net for row in pads.values() for net in row.values()):
        raise AssertionError("Mains voltage net not permitted on this PCB")
    if len(pads) != 50:
        raise AssertionError(f"Unexpected footprint count {len(pads)} != 50")

    print("PASS: PJ-037AH J1 / C3 service-separated 5V supply, PCA logic, and six generic 12V motor output net assignments.")
    print("CHECKED: 50 footprints, 6 motor channels, 3 pull-ups, 1 logic capacitor and a split 5V service path.")
    print("WARNING: A passing net audit plus DRC is NOT a manufacturing or electrical safety certification.")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
