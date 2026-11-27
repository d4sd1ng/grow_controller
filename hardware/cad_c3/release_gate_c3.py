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
BOARD = ROOT / "c3_6motor_DC_J1_PJ037AH_CANDIDATE_NOT_FOR_ORDER.kicad_pcb"
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

    # The selected J1 is a DC barrel jack, not the old Phoenix screw terminal.
    j1 = footprint_block(board, "J1") if board else ""
    if not j1:
        failures.append("J1: DC-Eingangssteckplatz fehlt")
    elif "PJ-037AH" not in j1 and "PJ_037AH" not in j1:
        failures.append("J1: altes Phoenix-Footprint statt Same Sky PJ-037AH")

    if board and '(footprint "Grow_Controller_C3:SameSky_PJ-037AH_THT_2P"' not in board:
        failures.append("J1 DC-Hohlbuchse PJ-037AH nicht im Board ersetzt")
    if board and '(footprint "Grow_Controller_C3:MSTBA_2P_5p08_NORTH" (layer "F.Cu") (at 16 20)' in board:
        failures.append("J1 enthaelt weiterhin das alte Phoenix-Footprint")

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

    # The bare PCB can be ordered without certifying a complete operated system.
    # Its electrical interfaces and manufacturing package must nevertheless be
    # fully designed and checked. Operation of hazardous peripherals is a
    # separate, strictly later acceptance decision.
    pcb_requirements = {
        "complete_schematic_erc": "Vollständiger Schaltplan und ERC",
        "verified_connector_interfaces": "Elektrische Auslegung aller Stecker und Signalports",
        "verified_12v_protection": "12-V-Eingangsschutz und Absicherung im PCB-Design",
        "verified_usb_power_isolation": "USB-/5-V-Quellentrennung im PCB-Design",
        "verified_component_footprints": "Verifizierte reale Bauteile und Footprints",
        "verified_enclosure_dimensions": "Gehäuse- und Montagefreigabe",
        "manufacturing_drc_full_netlist": "KiCad-DRC des vollständigen PCB-Netzes",
        "fabrication_outputs_verified": "Geprüfte Gerber-, Bohr- und Stapeldaten",
    }
    operation_requirements = {
        "verified_board_loads": "Belastungs- und Thermiktest am realen Board",
        "independent_safety_review": "Sicherheitsfreigabe für die vorgesehene Gesamtanlage",
    }

    pcb_blockers = list(failures)
    operation_blockers = []
    for key, explanation in pcb_requirements.items():
        entry = checks.get(key, {})
        if not (isinstance(entry, dict) and entry.get("passed") is True and entry.get("evidence")):
            pcb_blockers.append(f"{explanation}: kein belegter Prüfnachweis")
    for key, explanation in operation_requirements.items():
        entry = checks.get(key, {})
        if not (isinstance(entry, dict) and entry.get("passed") is True and entry.get("evidence")):
            operation_blockers.append(f"{explanation}: kein belegter Prüfnachweis")

    pcb_ready = not pcb_blockers
    operation_ready = pcb_ready and not operation_blockers
    result = {
        "board": BOARD.name,
        "order_ready": pcb_ready,
        "pcb_order_ready": pcb_ready,
        "system_operation_ready": operation_ready,
        "pcb_blocking_issues": pcb_blockers,
        "operation_blocking_issues": operation_blockers,
        "blocking_issues": pcb_blockers,
        "note": "KiCad DRC alone is not a PCB order approval. Bare-PCB ordering and system operation are separate release gates.",
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if pcb_ready else 1


if __name__ == "__main__":
    sys.exit(main())
