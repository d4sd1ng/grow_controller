#!/usr/bin/env python3
"""Static preflight for the sensor PCB. This does not replace KiCad DRC/ERC."""
from collections import Counter
from pathlib import Path
import re
import sys

board = Path(__file__).with_name("sensor_pcb_routed_candidate.kicad_pcb")
text = board.read_text(encoding="utf-8")
decls = [(int(i), n) for i, n in re.findall(r'^\(net (\d+) "([^"]+)"\)', text, re.M)]
refs = [(int(i), n) for i, n in re.findall(r'\(net (\d+) "([^"]+)"\)', text)]
counts = Counter(i for i, _ in decls)
names = dict(decls)
duplicate = [i for i, count in counts.items() if count != 1]
unknown = sorted(set((i, n) for i, n in refs if i not in names))
mismatch = sorted(set((i, n) for i, n in refs if i in names and names[i] != n))
footprints = re.findall(r'\(footprint "([^"]+)"', text)
placeholders = [x for x in footprints if x.startswith("Grow_Placeholder:")]
wrong_library = [x for x in footprints if not x.startswith("Grow_Controller_Sensor:")]
balance = 0
minimum = 0
for char in text:
    balance += (char == "(") - (char == ")")
    minimum = min(minimum, balance)
print(f"PCB: {board.name}")
print(f"Net declarations: {len(decls)}, references: {len(refs)}")
print(f"Footprints: {len(footprints)}, unapproved placeholders: {len(placeholders)}, wrong library: {len(wrong_library)}")
print(f"Tracks: {text.count('(segment ')}, vias: {text.count('(via ')}")

references = re.findall(r'\(fp_text reference "([^"]+)"', text)
duplicate_refs = sorted(ref for ref, count in Counter(references).items() if count != 1)
outline = re.findall(r'\(gr_rect \(start ([\d.]+) ([\d.]+)\) \(end ([\d.]+) ([\d.]+)\).*?\(layer "Edge.Cuts"\)', text)
expected = {"H1", "H2", "H3", "H4", "J_DHT1", "J_DHT2", "J_DHT3", "J_DS18", "J_LEVEL",
            "J_EC", "J_PH", "J_BH1750", "J_MLX", "J_RGB",
            "J_PHASE", "J_SERVICE", "U_ESP32", "U_ADS1115"}
missing = sorted(expected - set(references))
if duplicate_refs or missing or len(outline) != 1:
    print(f"ERROR footprint integrity: duplicates={duplicate_refs}, missing={missing}, outlines={len(outline)}")
print(f"Footprint references: {len(references)}, outline rectangles: {len(outline)}")

if placeholders:
    print("NOT APPROVED FOR FABRICATION: unverified footprint geometry")
    if "--release" in sys.argv:
        print("RELEASE BLOCKED: all placeholder geometries require physical verification")
        sys.exit(2)
for title, errors in [("Duplicate nets", duplicate), ("Unknown net references", unknown),
                      ("Net name mismatch", mismatch)]:
    if errors:
        print(f"ERROR {title}: {errors}")
if balance != 0 or minimum != 0:
    print(f"ERROR parentheses balance: {balance}, minimum nesting: {minimum}")
if duplicate or unknown or mismatch or balance or minimum or duplicate_refs or missing or len(outline) != 1 or len(footprints) != 29 or wrong_library:
    sys.exit(1)
print("Static integrity check passed. This is NOT a fabrication release.")
print("Run KiCad DRC and footprint/mechanical review before production.")
