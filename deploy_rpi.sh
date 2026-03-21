#!/bin/bash
# ============================================================
# deploy_rpi.sh – Telemetry-API auf dem Raspberry Pi (Zentrale)
#
# Standard-Architektur: API + SQLite + Dashboard laufen auf dem RPi;
# VPS nur WireGuard — siehe deployment_vps.md
#
# Auf dem Pi ausführen (sudo für systemd). Optional:
#   GROW_REPO_DIR=/pfad/zum/grow_controller ./deploy_rpi.sh
# ============================================================

set -e

REPO_DIR="${GROW_REPO_DIR:-$HOME/grow_controller}"
SERVICE_NAME="grow-telemetry.service"
SERVICE_SRC="${REPO_DIR}/${SERVICE_NAME}"
SCRIPT_DIR="${GROW_SCRIPT_DIR:-$HOME/scripts}"
LOG_DIR="${GROW_LOG_DIR:-$HOME/logs}"
UNIT_PATH="/etc/systemd/system/${SERVICE_NAME}"

echo ""
echo "========================================"
echo " Grow Controller – RPi (Telemetry API)"
echo "========================================"
echo ""
echo "  REPO_DIR=${REPO_DIR}"
echo ""

# 1. Abhängigkeiten prüfen
echo "[1/6] Abhängigkeiten prüfen..."
for cmd in python3 sqlite3; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "  FEHLER: '$cmd' nicht gefunden. Bitte installieren:"
        echo "  sudo apt install python3 python3-venv sqlite3"
        exit 1
    fi
done
echo "  OK"

# 2. Repo + venv + requirements
echo "[2/6] Python-Umgebung einrichten..."
if [ ! -d "${REPO_DIR}" ]; then
    echo "  FEHLER: Verzeichnis fehlt: ${REPO_DIR}"
    echo "  Repo klonen oder GROW_REPO_DIR setzen."
    exit 1
fi
if [ ! -f "${REPO_DIR}/run_telemetry_api.py" ]; then
    echo "  FEHLER: run_telemetry_api.py nicht gefunden unter ${REPO_DIR}"
    exit 1
fi
VENV="${REPO_DIR}/.venv"
if [ ! -d "$VENV" ]; then
    python3 -m venv "$VENV"
    echo "  .venv erstellt"
fi
"${VENV}/bin/pip" install -q -r "${REPO_DIR}/requirements.txt"
echo "  OK"

# 3. .env prüfen
echo "[3/6] .env prüfen..."
if [ ! -f "${REPO_DIR}/.env" ]; then
    echo "  WARNUNG: ${REPO_DIR}/.env nicht gefunden!"
    echo "  Bitte anlegen (von .env.example), mindestens:"
    echo "    TELEMETRY_API_HOST=0.0.0.0"
    echo "    TELEMETRY_API_PORT=8765"
    echo "    TELEMETRY_API_KEY=<sicherer-key>"
    echo "    GROW_DATA_DB=data/grow_telemetry.db"
    read -r -p "  Trotzdem fortfahren? (j/n): " ANS
    [ "$ANS" != "j" ] && exit 1
fi
echo "  OK"

# 4. Systemd-Unit installieren (Platzhalter __GROW_REPO__)
echo "[4/6] Systemd-Service installieren..."
if [ ! -f "$SERVICE_SRC" ]; then
    echo "  FEHLER: ${SERVICE_SRC} nicht gefunden."
    exit 1
fi
mkdir -p "$SCRIPT_DIR"
TMP_UNIT="$(mktemp)"
sed "s|__GROW_REPO__|${REPO_DIR}|g" "$SERVICE_SRC" >"$TMP_UNIT"
sudo cp "$TMP_UNIT" "$UNIT_PATH"
rm -f "$TMP_UNIT"
sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}"
sudo systemctl restart "${SERVICE_NAME}"
sleep 2
STATUS=$(sudo systemctl is-active "${SERVICE_NAME}" || true)
if [ "$STATUS" = "active" ]; then
    echo "  Service läuft (active)"
else
    echo "  FEHLER: Service Status = ${STATUS}"
    echo "  Logs: sudo journalctl -u ${SERVICE_NAME} -n 40"
    exit 1
fi

# 5. Backup-Cron (optional: grow_backup.sh ins SCRIPT_DIR legen)
echo "[5/6] Backup-Cron einrichten..."
mkdir -p "$LOG_DIR"
BACKUP_SH="${REPO_DIR}/grow_backup.sh"
if [ -f "$BACKUP_SH" ]; then
    chmod +x "$BACKUP_SH"
    CRON_LINE="0 2 * * * GROW_REPO=${REPO_DIR} ${BACKUP_SH}"
    ( crontab -l 2>/dev/null | grep -v "grow_backup.sh" || true; echo "$CRON_LINE" ) | crontab -
    echo "  Cron gesetzt: täglich 02:00 → grow_backup.sh (GROW_REPO=${REPO_DIR})"
else
    echo "  Hinweis: grow_backup.sh nicht im Repo gefunden – Cron übersprungen."
fi

# 6. Zusammenfassung
echo "[6/6] Fertig!"
echo ""
echo "  Health-Check:"
echo "    curl -sS http://127.0.0.1:8765/api/v1/health"
echo ""
echo "  Im LAN (erste IPv4):"
echo "    http://$(hostname -I | awk '{print $1}'):8765"
echo ""
echo "  Status:  sudo systemctl status ${SERVICE_NAME}"
echo "  Logs:    sudo journalctl -u ${SERVICE_NAME} -f"
echo ""
