#!/bin/bash
# ============================================================
# grow_backup.sh – SQLite-Backup (Standard: data/grow_telemetry.db) auf NAS
# Cron (Beispiel): 0 2 * * * GROW_REPO=/home/pi/grow_controller /pfad/grow_backup.sh
# deploy_rpi.sh trägt GROW_REPO passend ein, wenn grow_backup.sh im Repo liegt.
# ============================================================

# --- Konfiguration ---
# Repo-Root des grow_controller (wie bei deploy_rpi.sh / GROW_REPO_DIR)
GROW_REPO="${GROW_REPO:-$HOME/grow_controller}"
# Entspricht typisch GROW_DATA_DB in .env (relativ zum Repo)
DB_SOURCE="${GROW_REPO}/data/grow_telemetry.db"
NAS_MOUNT="/mnt/nas"
BACKUP_DIR="${NAS_MOUNT}/backup/grow"
RETENTION_DAYS=30
LOG_DIR="${GROW_LOG_DIR:-$HOME/logs}"
LOG_FILE="${LOG_DIR}/grow_backup.log"
DATE=$(date +"%Y-%m-%d_%H-%M")

# --- Logging ---
mkdir -p "$(dirname "$LOG_FILE")"
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# --- Prüfungen ---
if [ ! -f "$DB_SOURCE" ]; then
    log "FEHLER: Quelldatenbank nicht gefunden: $DB_SOURCE"
    exit 1
fi

if ! mountpoint -q "$NAS_MOUNT"; then
    log "FEHLER: NAS nicht eingehängt unter $NAS_MOUNT – Backup übersprungen."
    exit 1
fi

# --- Backup-Verzeichnis anlegen ---
mkdir -p "$BACKUP_DIR"

# --- Sicheres SQLite-Backup (kein LOCK nötig) ---
BACKUP_FILE="${BACKUP_DIR}/grow_telemetry_${DATE}.db"

sqlite3 "$DB_SOURCE" ".backup '${BACKUP_FILE}'"
STATUS=$?

if [ $STATUS -eq 0 ]; then
    SIZE=$(du -h "$BACKUP_FILE" | cut -f1)
    log "OK: Backup erstellt → ${BACKUP_FILE} (${SIZE})"
else
    log "FEHLER: sqlite3 .backup fehlgeschlagen (Exit ${STATUS})"
    exit 1
fi

# --- Alte Backups löschen (älter als RETENTION_DAYS) ---
DELETED=$(find "$BACKUP_DIR" -name "grow_telemetry_*.db" -mtime +${RETENTION_DAYS} -print -delete | wc -l)
if [ "$DELETED" -gt 0 ]; then
    log "Aufräumen: ${DELETED} alte Backups gelöscht (>${RETENTION_DAYS} Tage)"
fi

log "Backup abgeschlossen."
exit 0
