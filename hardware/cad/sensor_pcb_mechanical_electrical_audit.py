#!/usr/bin/env python3
"""Independent dimensional/netlist review of the sensor PCB (not a physical release)."""
from pathlib import Path
import math
import re
import sys

board = Path(__file__).with_name("sensor_pcb_routed_candidate.kicad_pcb")
source = board.read_text(encoding="utf-8")


def footprints(text):
    pattern = re.compile(r'\(footprint "[^"]+"')
    cursor = 0
    while found := pattern.search(text, cursor):
        start = found.start()
        depth = 0
        quoted = False
        escaped = False
        for i in range(start, len(text)):
            char = text[i]
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
                    raw = text[start:i + 1]
                    cursor = i + 1
                    yield raw
                    break
        else:
            raise ValueError("Unclosed PCB footprint")


def padmap(raw):
    return {int(num): name for num, name in re.findall(
        r'\(pad "(\d+)"[^\n]*?\(net \d+ "([^"]+)"\)', raw)}


expected = {
    "J_DHT1": ["3V3", "DHT1_DATA", "GND"],
    "J_DHT2": ["3V3", "DHT2_DATA", "GND"],
    "J_DHT3": ["3V3", "DHT3_DATA", "GND"],
    "J_DS18": ["3V3", "DS18_DATA", "GND"],
    "J_LEVEL": ["3V3", "LEVEL_TX", "GND"],
    "J_EC": ["5V_USB_BOARD", "GND", "EC_ANALOG_UNVERIFIED"],
    "J_PH": ["5V_USB_BOARD", "GND", "GND", "PH_PO_UNVERIFIED"],
    "J_BH1750": ["3V3", "GND", "I2C_BH_SDA", "I2C_BH_SCL"],
    "J_MLX": ["3V3", "GND", "I2C_MAIN_SDA", "I2C_MAIN_SCL"],
    "J_RGB": ["RGB_R_LED", "RGB_G_LED", "RGB_B_LED", "GND"],
    "J_PHASE": ["PHASE_BUTTON", "GND"],
    "J_SERVICE": ["SERVICE_BUTTON", "GND"],
    "U_ADS1115": ["3V3", "GND", "I2C_MAIN_SCL", "I2C_MAIN_SDA",
                  "GND", "ALRT_NC", "PH_PO_UNVERIFIED",
                  "EC_ANALOG_UNVERIFIED", "A2_NC", "A3_NC"],
    "U_ESP32": ["EN", "GPIO36_NC", "GPIO39_NC", "GPIO34_NC",
                "GPIO35_NC", "GPIO32_NC", "RGB_R_GPIO",
                "DHT1_DATA", "DHT2_DATA", "DHT3_DATA",
                "I2C_BH_SCL", "GPIO12_NC", "I2C_BH_SDA",
                "GND", "5V_USB_BOARD", "3V3", "GND",
                "GPIO15_NC", "GPIO2_NC", "RGB_G_GPIO",
                "PHASE_BUTTON", "RGB_B_GPIO", "GPIO5_NC",
                "LEVEL_TX", "SERVICE_BUTTON", "I2C_MAIN_SDA",
                "GPIO3_NC", "GPIO1_NC", "I2C_MAIN_SCL",
                "DS18_DATA"],
}
for ref, signal in (
    ("R_DHT1", "DHT1_DATA"), ("R_DHT2", "DHT2_DATA"),
    ("R_DHT3", "DHT3_DATA"), ("R_DS18", "DS18_DATA")
):
    expected[ref] = ["3V3", signal]
for color in ("R", "G", "B"):
    expected[f"R_RGB_{color}"] = [f"RGB_{color}_GPIO", f"RGB_{color}_LED"]
for bus, name in ((0, "MAIN"), (1, "BH")):
    for wire in ("SDA", "SCL"):
        expected[f"R_I2C{bus}_{wire}"] = ["3V3", f"I2C_{name}_{wire}"]

outside = []
collisions = []
errors = []
warning = []
parts = []
outline = re.search(r'\(gr_rect \(start ([\d.]+) ([\d.]+)\) '
                    r'\(end ([\d.]+) ([\d.]+)\).*?\(layer "Edge.Cuts"\)', source)
if not outline:
    raise SystemExit("FAIL: board outline missing")
edge = tuple(map(float, outline.groups()))

for raw in footprints(source):
    match_ref = re.search(r'\(fp_text reference "([^"]+)"', raw)
    match_at = re.match(r'\(footprint "[^"]+" \(layer "[^"]+"\) '
                        r'\(at ([\d.-]+) ([\d.-]+)(?: ([\d.-]+))?\)', raw)
    match_cr = re.search(r'\(fp_rect \(start ([\d.-]+) ([\d.-]+)\) '
                         r'\(end ([\d.-]+) ([\d.-]+)\).*?'
                         r'\(layer "F.CrtYd"\)', raw)
    if not match_ref or not match_at or not match_cr:
        errors.append("Footprint missing ref, placement or courtyard")
        continue
    ref = match_ref.group(1)
    x, y = map(float, match_at.group(1, 2))
    angle = float(match_at.group(3) or 0)
    a, b, c, d = map(float, match_cr.groups())
    if angle != 0:
        errors.append(f"{ref}: rotated footprint requires geometry transform")
    box = (x + a, y + b, x + c, y + d)
    if box[0] < edge[0] or box[1] < edge[1] or box[2] > edge[2] or box[3] > edge[3]:
        outside.append(ref)
    pad = padmap(raw)
    target = expected.get(ref)
    if target is None:
        errors.append(f"Unexpected footprint {ref}")
    elif [pad.get(i) for i in range(1, len(target) + 1)] != target or len(pad) != len(target):
        errors.append(f"{ref}: pad net mapping mismatch: {pad}")
    parts.append((ref, box))

if set(ref for ref, _ in parts) != set(expected):
    errors.append("Footprint reference set differs from 25-item expected BOM")
for i, (ref1, a) in enumerate(parts):
    for ref2, b in parts[i+1:]:
        xgap = max(a[0] - b[2], b[0] - a[2], 0)
        ygap = max(a[1] - b[3], b[1] - a[3], 0)
        if xgap == 0 and ygap == 0:
            xoverlap = min(a[2], b[2]) - max(a[0], b[0])
            yoverlap = min(a[3], b[3]) - max(a[1], b[1])
            if xoverlap > 0 and yoverlap > 0:
                collisions.append(f"{ref1}/{ref2}")
        distance = math.hypot(xgap, ygap)
        if distance < 2.0:
            warning.append((ref1, ref2, distance))

mounting_holes = len(re.findall(r'"\w*MountingHole[^"]*"|\(pad "[^"]*" np_thru_hole', source))
esp = next((box for ref, box in parts if ref == "U_ESP32"), None)
lower_connectors = [p for p in parts if p[0] in ("J_BH1750", "J_MLX")]

print("# Sensor PCB — automated mechanical and electrical review")
print(f"- Edge.Cuts: {edge[2]-edge[0]:.2f} x {edge[3]-edge[1]:.2f} mm; 2 copper layers")
print(f"- Footprints: {len(parts)}; complete expected pin-net mappings: {len(parts)-len(errors)} checked")
print(f"- Courtyard overlaps: {len(collisions)}; items beyond Edge.Cuts: {len(outside)}")
print(f"- Mounting-hole objects in layout: {mounting_holes}")
print("- Closest component courtyards:")
for a, b, d in sorted(warning, key=lambda z:z[2]):
    print(f"  - {a} / {b}: {d:.2f} mm")
if esp:
    print(f"- ESP32 courtyard: x={esp[0]:.2f}..{esp[2]:.2f}, y={esp[1]:.2f}..{esp[3]:.2f} mm")
    print("- USB location is at ESP32 module bottom per existing module documentation; "
          "clearance to lower connector row is NOT a validated cable insertion envelope.")
print("- Analog inputs A0/A1 are directly connected from 5-V-powered pH/EC breakouts; "
      "ADC 3.3-V maximum-level compliance not established by a netlist check.")
print("- 3.3-V and 5-V power-source current, reverse-feed and load margins require real measurements.")
if collisions or outside:
    errors.append(f"collisions={collisions}, outside={outside}")
if mounting_holes == 0:
    print("HOLD: No mounting holes / enclosure interface defined on this PCB.")
if any("U_ESP32" in (a, b) for a, b, _ in warning):
    print("HOLD: ESP32 USB access must be checked against lower connector plug bodies.")
for e in errors:
    print("FAIL:", e)
if errors:
    sys.exit(2)
print("PASS: Static electrical pin mappings and basic in-board footprint envelopes.")
print("NOT APPROVED: Cable access, mounting geometry and live electrical measurements remain open.")
if "--release" in sys.argv:
    unresolved = []
    if mounting_holes == 0:
        unresolved.append("No board mounting interface")
    if any("U_ESP32" in (a, b) for a, b, _ in warning):
        unresolved.append("Unverified USB connector and plugged-header clearance")
    if unresolved:
        for issue in unresolved:
            print("FABRICATION RELEASE BLOCKED:", issue)
        sys.exit(3)
