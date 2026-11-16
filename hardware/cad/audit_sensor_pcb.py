#!/usr/bin/env python3
"""Static preflight for the sensor PCB. This does not replace KiCad DRC/ERC."""
from collections import Counter
from pathlib import Path
import re
import sys

board = Path(__file__).with_name("sensor_pcb_routed_candidate.kicad_pcb")
text = board.read_text(encoding="utf-8")
decls = [(int(i), n) for i, n in re.findall(r'^\\(net (\\d+) "([^"]+)"\\)', text, re.M)]
refs = [(int(i), n) for i, n in re.findall(r'\\(net (\\d+) "([^"]+)"\\)', text)]
counts = Counter(i for i, _ in decls)
names = dict(decls)
duplicate = [i for i, count in counts.items() if count != 1]
unknown = sorted(set((i, n) for i, n in refs if i not in names))
mismatch = sorted(set((i, n) for i, n in refs if i in names and names[i] != n))
footprints = re.findall(r'\\(footprint "([^"]+)"', text)
placeholders = [x for x in footprints if x.startswith("Grow_Placeholder:")]
balance = 0
minimum = 0
for char in text:
    balance += (char == "(") - (char == ")")
    minimum = min(minimum, balance)
print(f"PCB: {board.name}")
print(f"Net declarations: {len(decls)}, references: {len(refs)}")
print(f"Footprints: {len(footprints)}, unapproved placeholders: {len(placeholders)}")
print(f"Tracks: {text.count('(segment ')}, vias: {text.count('(via ')}")
if placeholders:
    print("NOT APPROVED FOR FABRICATION: unverified footprint geometry")
for title, errors in [("Duplicate nets", duplicate), ("Unknown net references", unknown),
                      ("Net name mismatch", mismatch)]:
    if errors:
        print(f"ERROR {title}: {errors}")
if balance != 0 or minimum != 0:
    print(f"ERROR parentheses balance: {balance}, minimum nesting: {minimum}")
if duplicate or unknown or mismatch or balance or minimum:
    sys.exit(1)
print("Static integrity check passed. This is NOT a fabrication release.")
