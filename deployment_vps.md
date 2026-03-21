# Deployment: Raspberry Pi (Standard) + VPS (WireGuard)

**So ist das Projekt gedacht:** Telemetry-API, **SQLite**, Streamlit-Dashboard und übrige Logik laufen auf dem **Raspberry Pi** (zusammen mit NAS-Anbindung usw.). Ein **VPS** (z. B. Hetzner) dient **vorerst nur als WireGuard-Endpunkt** für dich (Laptop) — **nicht** als Pflicht-Host für die Telemetry-API.

| Ort | Rolle |
|-----|--------|
| **RPi** | `run_telemetry_api.py`, **`GROW_DATA_DB`** (z. B. `data/grow_telemetry.db` oder Pfad auf gemountetem NAS), Streamlit, restliche Projekte |
| **VPS** | **WireGuard** (Admin-VPN); optional später wieder eine öffentliche API, siehe [Alternative: API auf dem VPS](#alternative-api-auf-dem-vps-im-internet) |

Einrichtung RPi: Shell-Skript **`deploy_rpi.sh`** (auf dem Pi ausführen). Details unten.

---

## 1. Raspberry Pi (`deploy_rpi.sh`)

1. Repo auf den Pi klonen, z. B. nach `~/grow_controller` (oder `GROW_REPO_DIR` setzen).
2. **`grow-telemetry.service`** liegt im Repo-Root; das Skript ersetzt Platzhalter und installiert die Unit unter `/etc/systemd/system/`.
3. **`.env`** im Projektroot (von `.env.example`): u. a. `TELEMETRY_API_HOST=0.0.0.0`, `TELEMETRY_API_PORT=8765`, **`TELEMETRY_API_KEY`**, **`GROW_DATA_DB`** (Standard relativ: `data/grow_telemetry.db` — Ordner `data/` wird bei Bedarf angelegt).
4. **ESP / CYD:** **`IOT_TELEMETRY_SERVER_HOST`** = **Hostname oder LAN-IP des RPi** (feste DHCP-Reservation sinnvoll). Im Heim-LAN meist **`IOT_USE_HTTPS=0`**. Danach `python export_firmware_secrets_from_env.py` und flashen.

**Firewall (RPi):** Port **8765/tcp** nur im LAN freigeben, wenn du einschaltest — nicht ins Internet öffnen, solange nur Heimgeräte zugreifen.

**Backup:** `grow_backup.sh` sichert die SQLite-Datei auf ein gemountetes NAS; `DB_SOURCE` an **`GROW_DATA_DB`** anpassen bzw. `GROW_REPO` in der Cron-Umgebung setzen (siehe Kommentare in der Datei).

---

## 2. VPS: WireGuard

- Der VPS bleibt **VPN-Server** (z. B. `wg0` mit `10.0.0.1/24`); dein Windows-Client erhält z. B. `10.0.0.2`.
- **Telemetry-API auf dem VPS ist dafür nicht nötig**, wenn alle Ingests zum **RPi** gehen.
- **Hinweis:** Mit dem Tunnel kommst du standardmäßig auf **Dienste auf dem VPS**, nicht automatisch auf den RPi im Heimnetz. Für „über VPN zum RPi“ bräuchtest du Routing/Heim-VPN — außerhalb dieses Dokuments.

**Falls** du die API **nur für dich** vom Laptop über den Hetzner-Tunnel testen willst (API läuft trotzdem auf dem VPS): siehe frühere Notizen zu **`TELEMETRY_API_HOST=0.0.0.0`** und UFW **`in on wg0`** — das ist **nicht** der empfohlene Hauptweg, wenn der RPi die Zentrale ist.

---

## 3. Alternative: API auf dem VPS (im Internet)

Nur sinnvoll, wenn ESPs **direkt aus dem Internet** zur API sollen und der **RPi nicht erreichbar** ist (kein Tunnel, keine öffentliche Heim-IP).

- Projekt auf dem VPS unter z. B. `/opt/grow_controller`, venv, `pip install -r requirements.txt`.
- **`.env`:** `TELEMETRY_API_HOST=127.0.0.1`, Uvicorn **8765**, davor **Caddy/Nginx auf 443** mit TLS.
- **`IOT_TELEMETRY_SERVER_HOST`** = **Domain** (A-Record auf VPS-IP), **`IOT_USE_HTTPS=1`**, Port **443**.
- Firewall: **443** (und **80** für ACME) öffnen, **8765** nicht öffentlich.

Dann existiert eine **zweite SQLite** auf dem VPS — sie ist **nicht** dieselbe wie auf dem RPi. Für ein einziges Dashboard/eine Historie entweder **nur einen** API-Standort wählen oder später eine gemeinsame Server-DB (z. B. Postgres) planen.

---

## 4. Firmware & TLS

Siehe **`firmware/README.md`** und **`.env.example`**. Heim-LAN zum RPi: meist HTTP. Öffentlicher VPS: HTTPS + `IOT_TLS_INSECURE` nur bewusst für den Start.

---

## Checkliste (RPi-Standard)

- [ ] Repo auf RPi, **`deploy_rpi.sh`** ausgeführt (oder manuell venv + systemd wie im Skript)
- [ ] **`.env`** mit **`TELEMETRY_API_KEY`** und **`GROW_DATA_DB`**
- [ ] **`IOT_TELEMETRY_SERVER_HOST`** = RPi (LAN), Export **`device_secrets.h`**, Geräte neu flashen
- [ ] Optional: NAS mount + **`grow_backup.sh`** + Cron
- [ ] VPS: nur **WireGuard** pflegen; keine doppelte Telemetry-API, wenn alles über den RPi läuft

## Checkliste (nur wenn API wirklich auf dem VPS)

- [ ] `.env` auf dem VPS mit **`TELEMETRY_API_KEY`**
- [ ] Uvicorn **`127.0.0.1:8765`**, Reverse-Proxy **443**
- [ ] DNS A-Record → VPS-IP, **`IOT_USE_HTTPS=1`**, Host = Domain
- [ ] Export **`device_secrets.h`** und Firmware flashen
