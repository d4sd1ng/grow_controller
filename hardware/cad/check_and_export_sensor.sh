#!/usr/bin/env bash
# Sensor PCB quality gate. Requires locally installed KiCad CLI (modern version).
# Runs DRC first; produces Gerber/Excellon only after explicit review.
set -euo pipefail

BASE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PCB="$BASE_DIR/sensor_pcb_routed_candidate.kicad_pcb"
OUT="$BASE_DIR/out_sensor"
mkdir -p "$OUT"

if ! command -v kicad-cli >/dev/null 2>&1; then
  echo "FEHLER: kicad-cli nicht installiert; kein DRC oder Gerber exportiert." >&2
  exit 127
fi
if [[ ! -f "$PCB" ]]; then
  echo "FEHLER: KiCad-Boarddatei fehlt." >&2
  exit 2
fi

echo "Pruefe KiCad-Board: $PCB"
if ! kicad-cli pcb drc --exit-code-violations --all-track-errors \
  --output "$OUT/sensor_drc.txt" "$PCB"; then
  echo "DRC nicht bestanden. Bericht: $OUT/sensor_drc.txt" >&2
  exit 5
fi
echo "DRC bestanden. Bericht: $OUT/sensor_drc.txt"

if [[ "${1:-}" != "--export-reviewed" ]]; then
  echo "Gerber nicht erzeugt. Erneuter Aufruf mit --export-reviewed erst nach Footprint-/Mechanikpruefung."
  exit 0
fi
if grep -q 'Grow_Placeholder:' "$PCB"; then
  echo "FERTIGUNG GESPERRT: PCB enthaelt noch nicht freigegebene Grow_Placeholder-Footprints." >&2
  exit 6
fi

# Explicit hold: DRC=0 and assigned footprints are not enough to authorize the physical production order.
# A separate physical mounting, connector and electrical review is required.
if [[ ! -f "$BASE_DIR/FABRICATION_RELEASE_APPROVED" ]]; then
  echo "FERTIGUNG GESPERRT: Freigabe nach mechanischer und elektrischer Pruefung fehlt." >&2
  exit 7
fi

# No extra 230V/SSR or chemical-dosing circuit on this sensor board.
mkdir -p "$OUT/gerber"
kicad-cli pcb export gerbers \
  --layers "F.Cu,B.Cu,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts" \
  --output "$OUT/gerber" "$PCB"
kicad-cli pcb export drill --format excellon \
  --output "$OUT/gerber" "$PCB"
echo "Gerber/Excellon exportiert nach $OUT/gerber; vor Bestellung visuell pruefen."
