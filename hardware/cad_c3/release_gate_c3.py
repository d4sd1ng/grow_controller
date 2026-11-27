#!/usr/bin/env python3
"""Fail-closed manufacturing gate for the Grow Controller C3 CAD prototype.

The previously green KiCad DRC validates ONLY modeled nets. It does not
indicate that the entire control board is complete. This tool does not
generate Gerbers or provide instructions for hazardous loads.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BOARD = ROOT / "c3_6motor_usb_service_disconnect_DRAFT_NOT_FOR_ORDER.kicad_pcb"
EVIDENCE = ROOT / "C3_RELEASE_EVIDENCE.json"
REQUIRED_CONNECTORS = {"J2": 2, "J3": 2, "J4": 2, "J5": 2, "J6": 2, "J13": 3}


def footprint_block(board: str, ref: str) -> str:
    # Parse balanced KiCad s-expressions instead of matching across footprints.
    pattern = re.compile(r'\(footprint\s+"[^"]+"')
    for found in pattern.finditer(board):
        start = found.start()
        level = 0
        quote = False
        escape = False
        for i in range(start, len(board)):
            ch = board[i]
            if quote:
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == '"':
                    quote = False
            elif ch == '"':
                quote = True
            elif ch == '(':
                level += 1
            elif ch == ')':
                level -= 1
                if level == 0:
                    block = board[start:i+1]
                    match = re.search(r'\(fp_text\s+reference\s+"([^"]+)"', block)
                    if match and match.group(1) == ref:
                        return block
                    break
    return ""


def main() -> int:
    failures: list[str] = []
    if not BOARD.is_file():
        failures.append("CAD-Ausgangsdatei fehlt")
        board = ""
    else:
        board = BOARD.read_text(encoding="utf-8")

    for ref, pins in REQUIRED_CONNECTORS.items():
        part = footprint_block(board, ref) if board else ""
        if not part:
            failures.append(f"{ref}: Steckplatz fehlt")
            continue
        pad_lines = [line for line in part.splitlines() if re.search(r'\(pad\s+"\d+"', line)]
        assigned = [line for line in pad_lines if re.search(r'\(net\s+\d+\s+"[^"]+"\)', line)]
        if len(pad_lines) != pins or len(assigned) != pins:
            failures.append(f"{ref}: {len(assigned)}/{pins} Anschluss-Pads besitzen Netze")

    if not EVIDENCE.is_file():
        failures.append("Freigabenachweise fehlen")
        checks = {}
    else:
        checks = json.loads(EVIDENCE.read_text(encoding="utf-8")).get("checks", {})

    required = {
        "complete_schematic_erc": "Vollständiger Schaltplan und ERC",
        "verified_connector_interfaces": "Verifizierte Steuerschnittstellen und Anschlussbelegung",
        "verified_12v_protection": "12-V-Eingangsschutz und Absicherung",
        "verified_usb_power_isolation": "USB-/5-V-Stromquellentrennung am echten Modul",
        "verified_component_footprints": "Bauteil- und Stecker-Footprints mit Originalteilen",
        "verified_enclosure_dimensions": "Gehäuse und Montageabstände",
        "verified_board_loads": "Strom- und Thermikprüfung an ungefährlicher Testlast",
        "independent_safety_review": "Unabhängige Sicherheitsprüfung der Gesamtanwendung",
        "manufacturing_drc_full_netlist": "KiCad-DRC des vollständigen elektrischen Netzes",
    }
    for key, explanation in required.items():
        entry = checks.get(key, {})
        if not (isinstance(entry, dict) and entry.get("passed") is True and entry.get("evidence")):
            failures.append(f"{explanation}: kein belegter Prüfnachweis")

    result = {
        "board": BOARD.name,
        "order_ready": not failures,
        "blocking_issues": failures,
        "note": "DRC=0 is not a full hardware approval; never order the draft as final.",
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
