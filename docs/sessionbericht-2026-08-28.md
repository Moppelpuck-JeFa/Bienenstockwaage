# Sessionbericht 28.08.2026 — Umbenennung Stockwaage → Bienenwaage

**Auftrag:** das Dashboard „Stockwaage" auf das neue Gerät „Bienenwaage"
umbauen, Anlass ist der Wechsel des ESP32-Boards.

**Ergebnis:** erledigt, aber anders als geplant. Der Auftrag ging von einem
*zweiten* Gerät aus, auf das umzuhängen wäre. Das gibt es nicht — das neue
Board hat die Identität des alten übernommen. Aus dem Umhängen wurde deshalb
eine Umbenennung: 30 Entities, das Gerät und 47 Dashboard-Referenzen. Die
Messreihe ist dabei erhalten geblieben.

---

## 1. Der Irrweg, und warum er einer war

Die naheliegende Lesart war: neues Board → neues Gerät in HA → neue Entities
`bienenwaage_*` → Dashboard umhängen. Dafür sprach handfeste Evidenz. In HA
stand ein offener ESPHome-Discovery-Flow:

```
handler  esphome (zeroconf)
key      bienenwaage._esphomelib._tcp.local.
name     "Bienenwaage (bienenwaage)"
MAC      20:50:0d:ca:b2:bc
step     discovery_confirm
```

Ein Gerät namens „Bienenwaage" meldete sich also tatsächlich im Netz an. Der
Schluss daraus — es werde gleich als eigenes Gerät entstehen — war trotzdem
falsch.

**Der Beweis kam erst beim Adoptionsversuch über die IP:**

```
Flow aborted: already_configured_updates
  title  Stockwaage
  name   stockwaage
  mac    20:50:0d:ca:b2:bc
```

HA hat das Board dem **bestehenden** Config-Entry zugeordnet, statt ein neues
anzulegen. Danach stand im Geräteregister:

| | vorher | nachher |
|---|---|---|
| MAC | `f8:b3:b7:49:59:7c` | `20:50:0d:ca:b2:bc` |
| Firmware gebaut | 24.08.2026 12:11 | 28.08.2026 20:14 |
| Config-Entry | `01M08HH8349N52K2MR1RABCV3T` | derselbe |
| Entities | 29 × `stockwaage_*` | dieselben 29 |

Die Regel dahinter, und sie gilt für jeden weiteren Boardwechsel:

> **ESPHome-Config-Entries hängen am Gerätenamen, nicht an der Hardware.**
> Bleibt `name:` gleich, übernimmt neue Hardware die bestehende Identität
> mitsamt allen Entity-IDs — HA tauscht nur MAC und IP. Ein Boardwechsel
> allein erzeugt **keine** neuen Entities.

Warum die mDNS-Ankündigung trotzdem `bienenwaage` lautete, ließ sich in dieser
Sitzung nicht abschließend klären; der Zugriff auf die Add-on-Dateien fehlt
(siehe Punkt 5). Für das Ergebnis ist es ohne Belang: maßgeblich ist der Name,
den das Gerät über die API meldet, und der war `stockwaage`.

**Nebenbefund:** Die Bestätigung über die Oberfläche war vorher an der
Namensauflösung gescheitert, nicht am Schlüssel —
`Timeout while resolving IP address for ['bienenwaage.local']`. Alle anderen
ESPHome-Geräte dieser Installation werden über feste IPs angesprochen. Über
`192.168.1.115` lief es sofort durch.

## 2. Was daraus folgte

Weil das Dashboard bereits auf die richtigen, lebenden Entities zeigte, war
nichts kaputt — es gab technisch nichts umzuhängen. Die gewünschte Benennung
war damit kein Repoint mehr, sondern ein Rename, und das ist die Operation,
vor der `CLAUDE.md` warnt.

Gewählt wurde: **nur in der HA-Registry umbenennen, die ESPHome-YAML
unangetastet lassen.** Begründung:

- Ein `entity_id`-Rename erhält die Historie samt Langzeitstatistik
  (HA ≥ 2022.4 zieht die `statistic_id` mit).
- Registry-Namen gewinnen gegen die von der Integration vorgeschlagenen. Ein
  erneuter Flash macht die Umbenennung deshalb **nicht** rückgängig.
- Der Weg über `geraete_name`/`anzeige_name` in der YAML hätte HA neue
  Entities anlegen lassen und die bisherige Messreihe an toten IDs hängen
  lassen — genau der dokumentierte Schadensfall.

## 3. Was geändert wurde

**Gerät:** „Stockwaage" → „Bienenwaage" (`name_by_user`). Damit ziehen die
Anzeigenamen aller Geräte-Entities automatisch mit — aus „Stockwaage Gewicht"
wird „Bienenwaage Gewicht".

**26 ESPHome-Entities** von `stockwaage_*` auf `bienenwaage_*`. Zwei davon
trugen zusätzlich einen Area-Präfix, der bei den übrigen 24 fehlte; er wurde
bei der Gelegenheit entfernt:

| alt | neu |
|---|---|
| `binary_sensor.garten_stockwaage_wachhalten_schalter` | `binary_sensor.bienenwaage_wachhalten_schalter` |
| `switch.garten_stockwaage_hx711_versorgung` | `switch.bienenwaage_hx711_versorgung` |

**Drei Rechenhelfer.** Erst die Quelle über den Options-Flow auf
`sensor.bienenwaage_gewicht` umgehängt, dann die entity_id umbenannt. Die
Reihenfolge ist nicht beliebig: Entity-Registry-Renames aktualisieren die im
Config-Entry gespeicherte Quelle **nicht**, der Helfer zeigte sonst dauerhaft
auf eine tote ID.

| Helfer | Typ | Fenster |
|---|---|---|
| `sensor.bienenwaage_gewichtsanderung` | derivative | 6 h → kg/d |
| `sensor.bienenwaage_gewichtsverlust_kurz` | derivative | 20 min → kg/h |
| `sensor.bienenwaage_tagesbilanz` | statistics `change` | 24 h |

Anzeigenamen wurden dabei über die Registry gesetzt, weil Options-Flows den
Schlüssel `name` nicht annehmen.

**`input_boolean.stockwaage_wachhalten`** → `input_boolean.bienenwaage_wachhalten`.

**Dashboard `bienen-stockwaage-esp32`:** alle 47 Referenzen, Titel jetzt
„Bienenwaage". Die `url_path` bleibt — sie lässt sich nicht ändern, damit
bleiben Sidebar-Eintrag und Lesezeichen heil. Erfasst wurden neben den
`entity:`-Feldern auch die vier View-Badges, die `entities`-Listen der Graph-
und Entities-Karten und die Jinja2-Ausdrücke in fünf Markdown-Karten. Badges
und Templates hängen neben den Karten, nicht darin; eine kartenorientierte
Suche übersieht sie.

## 4. Prüfung

Nach dem Verfahren aus `references/safe-refactoring.md`: erst alle Konsumenten
suchen, ändern, danach auf Reste prüfen.

- Konsumenten vorher: 47 Dashboard-Referenzen, 4 Helfer. **Keine**
  Automationen, Skripte oder Szenen — die vier in `waage-eg-notes.md`
  beschriebenen Automationen existieren nicht mehr.
- Dashboard-Suche nach `stockwaage` nachher: **0 Treffer.**
- `input_boolean.stockwaage_wachhalten` liefert 404, ist also wirklich weg.
- Alle neuen IDs sind in der State Machine mit korrekten Anzeigenamen.

Verbleibende Nennungen von „Stockwaage" gibt es noch in den **Config-Entry-Titeln**
der drei Helfer (Einstellungen → Helfer). Das ist kosmetisch: Entity-IDs und
Anzeigenamen sind umgestellt. Options-Flows nehmen keinen neuen Titel an,
Umbenennen ginge dort nur über Löschen und Neuanlegen — und das kostete die
Historie. Bewusst so gelassen.

## 5. Zustand des neuen Geräts

Verbunden seit 21:11:59. **Die Kalibrierung steht auf den Platzhaltern:**

| Größe | Wert | Bewertung |
|---|---|---|
| Kalibrierfaktor | `3500.0` | Platzhalter — erwartet werden −18.000 bis −21.000 |
| Kalibriert bei | leer | Temperaturkompensation ist damit **stumm aus** |
| Gewicht | `unknown` | Folge der fehlenden Kalibrierung |
| Rohwert | −717.422,69 | plausibel, HX711 liefert |
| WLAN-Signal | −76 dBm | schwächer als am alten Board (−67) |

Bei einem neuen Board mit frischem NVS ist das erwartbar. **Beide
Kalibrierschritte fahren, nicht nur den Referenzpunkt** — der Faktor des alten
Boards (−20.780,66 bei 22,5 °C, kalibriert mit 33,62 kg) gilt hier nicht.

Die beiden Ableitungen stehen bis dahin auf `unavailable`, weil ihre Quelle
`unknown` ist. Das löst sich mit der Kalibrierung von selbst.

`binary_sensor.bienenwaage_wachhalten_schalter` steht auf **an** — der
Kippschalter an der Hardware ist umgelegt, das Gerät geht nicht in den
Tiefschlaf. Praktisch fürs Kalibrieren, aber danach umlegen, sonst läuft der
Akku leer.

## 6. Offene Punkte

- **`bienenwaage.yaml` ist nicht im Repo.** Sie konnte auch nicht geholt
  werden: der MCP-Token hat keinen Supervisor-Zugriff (`/addons` →
  *Unauthorized*), damit sind weder die Add-on-Dateiliste noch die
  WebSocket-Schnittstelle des Device Builders erreichbar. Von Hand einchecken,
  sonst beschreibt das Repo dauerhaft ein Gerät, das es nicht mehr gibt.
- **Die Umbenennung lebt nur in der HA-Registry.** Die YAML sagt weiterhin
  `stockwaage`. Das ist stabil und überlebt Flashes, aber wer nur die YAML
  liest, findet den Namen nicht wieder. Beim nächsten Anfassen der Datei einen
  Kommentar hinterlassen.
- **`waage-eg.yaml` beschreibt Hardware, die nicht mehr läuft** — der
  Config-Entry „Waage" steht auf `unavailable`. Kennzeichnen oder entfernen.
- Die Pin-Belegung in `waage-eg.yaml` ist D1-Mini-Notation und passt nicht zu
  einem ESP32. Der Durchsicht-Taster liegt laut Dashboard-Text auf GPIO34 mit
  externem 10-kΩ-Pull-up.
- Das WLAN-Signal am neuen Standort im Auge behalten.
