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

MODE="${1:-check}"
if [[ "$MODE" == "check" ]]; then
  echo "DRC bestanden. Mit --export-prototype nur unbestueckte Prototyp-Dateien erzeugen."
  exit 0
fi
if [[ "$MODE" != "--export-prototype" && "$MODE" != "--export-reviewed" ]]; then
  echo "Unbekannter Modus. Erlaubt: --export-prototype oder --export-reviewed." >&2
  exit 2
fi
if grep -q 'Grow_Placeholder:' "$PCB"; then
  echo "FERTIGUNG GESPERRT: PCB enthaelt noch nicht freigegebene Grow_Placeholder-Footprints." >&2
  exit 6
fi

# Separate the first bare-PCB prototype from live electrical/assembled hardware release.
# There is no sensor enclosure yet. A case-fit test is NOT a prototype PCB prerequisite.
python3 "$BASE_DIR/audit_sensor_pcb.py" > "$OUT/static_audit.txt"
python3 "$BASE_DIR/sensor_pcb_mechanical_electrical_audit.py" > "$OUT/mechanical_electrical_audit.txt"
if [[ "$MODE" == "--export-reviewed" ]]; then
  if [[ ! -f "$BASE_DIR/FABRICATION_RELEASE_APPROVED" ]]; then
    echo "BETRIEBSFREIGABE GESPERRT: Live-Betrieb der bestueckten Platine nicht freigegeben." >&2
    exit 7
  fi
  if ! python3 "$BASE_DIR/sensor_pcb_mechanical_electrical_audit.py" --release > "$OUT/mechanical_electrical_release.txt"; then
    echo "BETRIEBSFREIGABE GESPERRT: USB- und elektrische Pruefung offen." >&2
    exit 8
  fi
fi

# No extra 230V/SSR or chemical-dosing circuit on this sensor board.
mkdir -p "$OUT/gerber"
kicad-cli pcb export gerbers \
  --layers "F.Cu,B.Cu,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts" \
  --output "$OUT/gerber" "$PCB"
kicad-cli pcb export drill --format excellon --excellon-separate-th \
  --output "$OUT/gerber" "$PCB"
if [[ "$MODE" == "--export-prototype" ]]; then
  cat > "$OUT/gerber/PROTOTYPE_ONLY.txt" <<'EOF'
UNBESTUECKTER SENSOR-PCB-PROTOTYP — KEINE BETRIEBSFREIGABE
Board: 179 x 115 x 1,6 mm, 2 Lagen, 4 x M3 NPTH.
Kein Sensor-Gehaeuse vorhanden; ein Gehaeusetest ist keine Voraussetzung der ersten Leerplatine.
Offen: ESP32 USB-C-Zugang bei gesteckten Phoenix-Gegensteckern, Sensormodul-Pegel und reales Strombudget.
Keine Freigabe zum Anschluss von 230 V, SSR-Lasten oder chemischer Dosierung.
EOF
  echo "Prototyp-Dateien (nur Leerplatine) nach $OUT/gerber exportiert."
else
  echo "Gerber/Excellon nach $OUT/gerber exportiert; Live-/Bestueckungsfreigabe separat dokumentiert."
fi
