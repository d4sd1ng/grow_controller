#!/usr/bin/env python3
"""Confirm a bare-PCB prototype fabrication ZIP matches the latest 12V board.

This is a manufacturing-data check, not an electrical performance test.
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BOARD = ROOT / "c3_12v_5signals_1fan_WIP_NOT_FOR_ORDER.kicad_pcb"
PACKAGE = ROOT / "manufacturing" / "C3_12V_PROTOTYPE_GERBER_DRILL_2026-11-27.zip"
REQUIRED_EXTENSIONS = {
    ".gtl", ".gbl", ".gts", ".gbs", ".gto", ".gbo", ".gm1", ".drl"
}

def run() -> dict:
    result = {"board": BOARD.name, "package": PACKAGE.name,
              "bare_pcb_prototype_order_ready": False, "checks": {}}
    if not BOARD.is_file() or not PACKAGE.is_file():
        result["error"] = "Source PCB or manufacturing ZIP missing"
        return result

    with zipfile.ZipFile(PACKAGE) as archive:
        corrupted = archive.testzip()
        result["checks"]["zip_integrity"] = corrupted is None
        names = archive.namelist()
        suffixes = {Path(n).suffix.lower() for n in names}
        result["checks"]["gerber_and_drill_layers"] = REQUIRED_EXTENSIONS <= suffixes
        result["checks"]["files_nonempty"] = all(
            archive.getinfo(n).file_size > 0
            for n in names if Path(n).suffix.lower() in REQUIRED_EXTENSIONS
        )
        for file in ("source_sha256.txt", "KiCad_DRC.txt", "C3_12V_source.kicad_pcb"):
            result["checks"][f"contains_{file}"] = file in names
        if all(result["checks"][f"contains_{file}"] for file in
               ("source_sha256.txt", "KiCad_DRC.txt", "C3_12V_source.kicad_pcb")):
            expected = archive.read("source_sha256.txt").decode("utf-8").split()[0]
            raw = BOARD.read_bytes()
            actual = hashlib.sha256(raw).hexdigest()
            result["checks"]["archive_source_matches_board"] = expected == actual
            result["checks"]["embedded_source_matches_board"] = (
                archive.read("C3_12V_source.kicad_pcb") == raw
            )
            drc = archive.read("KiCad_DRC.txt").decode("utf-8")
            for line in (
                "** Found 0 DRC violations **",
                "** Found 0 unconnected pads **",
                "** Found 0 Footprint errors **",
            ):
                result["checks"]["drc_" + line.strip("* ").replace(" ", "_")] = (
                    line in drc
                )
    result["bare_pcb_prototype_order_ready"] = all(result["checks"].values())
    result["note"] = "Manufacturing files only; operation and component fit require physical verification."
    return result

if __name__ == "__main__":
    result = run()
    print(json.dumps(result, indent=2, ensure_ascii=False))
    sys.exit(0 if result["bare_pcb_prototype_order_ready"] else 1)
