#!/usr/bin/env python3
"""Validate a KiCad-generated sensor prototype CAM set and package it.

This is a *bare PCB prototype* export, not an assembled/live-use approval.
No geometry is synthesized and no missing CAM files are silently replaced.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import re
import sys
import zipfile

REQUIRED_GERBER = (
    "F_Cu", "B_Cu", "F_Silkscreen", "B_Silkscreen",
    "F_Mask", "B_Mask", "Edge_Cuts",
)


def fail(message: str) -> None:
    raise SystemExit("CAM CHECK FAILED: " + message)


def check_cam(cam_dir: Path) -> list[Path]:
    if not cam_dir.is_dir():
        fail(f"CAM directory does not exist: {cam_dir}")
    gerbers = sorted(p for p in cam_dir.iterdir() if p.suffix.lower() in {".gbr", ".ger"})
    drills = sorted(p for p in cam_dir.iterdir() if p.suffix.lower() == ".drl")
    if len(gerbers) != len(REQUIRED_GERBER):
        fail(f"expected exactly {len(REQUIRED_GERBER)} Gerber films, found {len(gerbers)}")
    for token in REQUIRED_GERBER:
        if not any(re.search(r"(?<![A-Za-z])" + re.escape(token) + r"(?![A-Za-z])", p.stem, re.I)
                   for p in gerbers):
            fail(f"Gerber film missing: {token}")
    if len(drills) != 2:
        fail(f"expected separate PTH and NPTH Excellon drill files, found {len(drills)}")
    pth = [p for p in drills if "PTH" in p.stem.upper() and "NPTH" not in p.stem.upper()]
    npth = [p for p in drills if "NPTH" in p.stem.upper()]
    if len(pth) != 1 or len(npth) != 1:
        fail("PTH/NPTH drill split not demonstrated by file names")
    for p in gerbers + drills:
        if p.stat().st_size == 0:
            fail(f"empty file: {p.name}")
    # KiCad Excellon coordinates start with X and/or Y; 4 M3 NPTH drills required.
    coords = [line for line in npth[0].read_text(errors="replace").splitlines()
              if re.match(r"^[XY][-+0-9]", line.strip(), re.I)]
    if len(coords) != 4:
        fail(f"expected 4 NPTH drill hits for M3 mounting, found {len(coords)}")
    return sorted(gerbers + drills, key=lambda p: p.name)


def main() -> None:
    if len(sys.argv) != 3:
        fail("usage: package_sensor_prototype.py <CAM_DIRECTORY> <BOARD_FILE>")
    cam_dir = Path(sys.argv[1]).resolve()
    board = Path(sys.argv[2]).resolve()
    if not board.is_file():
        fail(f"board source missing: {board}")
    files = check_cam(cam_dir)
    sha = hashlib.sha256(board.read_bytes()).hexdigest()
    files_report = [
        f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}"
        for p in files
    ]
    readme = (
        "GROW CONTROLLER SENSOR PCB Rev A — UNBESTUECKTER TESTPROTOTYP\n"
        "Aussenkontur: 179 x 115 mm; Dicke: 1,6 mm; Lagen: 2; 4 x M3 NPTH.\n"
        "Dateien: 7 Gerberfilme, Excellon PTH und NPTH separat.\n"
        f"KiCad-Quelldatei SHA256: {sha}\n"
        "Diese CAM-Dateien stammen direkt aus dem KiCad-CLI-Export nach bestandenem DRC.\n"
        "NUR LEERPLATINEN-PROTOTYP — KEINE FREIGABE FUER BESTUECKUNG/LIVE-BETRIEB.\n"
        "Pruefen am Prototyp: USB-C bei gesteckten Phoenix-Klemmen, Modulsteckung,\n"
        "3,3-V/5-V-Lastbudget, pH-/EC-Spannungspegel vor Anschluss des ADS1115.\n"
        "KEINE 230-V-LASTVERDRAHTUNG UND KEINE CHEMISCHE DOSIERUNG.\n"
    )
    # Metadata is deliberately included in the archive: prevents an
    # undocumented preview from being mistaken for a live-use release.
    archive = cam_dir.with_suffix(".zip")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as z:
        for p in files:
            z.write(p, arcname=p.name)
        z.writestr("PROTOTYPE_ONLY.txt", readme)
        z.writestr("CAM_SHA256SUMS.txt", "\n".join(files_report) + "\n")
    print(f"PROTOTYPE CAM CHECK PASSED: {len(files)} manufacturing files")
    print(f"UNPOPULATED PROTOTYPE ZIP: {archive}")
    print("Electrical/mechanical operating release: NOT GRANTED")


if __name__ == "__main__":
    main()
