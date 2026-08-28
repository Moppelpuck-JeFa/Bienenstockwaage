# Sessionbericht 28.08.2026 — Dashboard-Migration Stockwaage → Bienenwaage

**Auftrag:** das Dashboard „Stockwaage" auf das neue Gerät „Bienenwaage"
umbauen, Anlass ist der Wechsel des ESP32-Boards.

**Stand am Ende der Sitzung:** die Bestandsaufnahme ist vollständig und belegt,
der Umbau selbst steht noch aus. Er hängt an genau einem Schritt, der von außen
kommen muss — dem Bestätigen der ESPHome-Discovery in Home Assistant. Warum das
so ist, steht unter Punkt 3.

---

## 1. Was tatsächlich vorliegt

Die Ausgangslage war nicht die, die der Repo-Stand vermuten lässt. Im Root
liegt nur `waage-eg.yaml` mit `geraete_name: waage-eg`; in Home Assistant läuft
aber seit Wochen ein ESP32. Die Datei dazu ist **nicht eingecheckt**.

| | altes Board | neues Board |
|---|---|---|
| ESPHome-Gerätename | `stockwaage` | `bienenwaage` |
| Anzeigename in HA | Stockwaage | Bienenwaage |
| Board | `esp32doit-devkit-v1` | noch unbekannt |
| MAC | `f8:b3:b7:49:59:7c` | `20:50:0d:ca:b2:bc` |
| Firmware | 2026.8.1, gebaut 24.08.2026 | — |
| Stand in HA | Config-Entry „Stockwaage", 29 Entities | nur Discovery, nicht bestätigt |

**Beide Geräte stammen aus derselben Datei `bienenwaage.yaml`.** Der
Gerätename darin ist über die Zeit gewandert, der Dateiname nicht. Das lässt
sich unabhängig belegen: der FRITZ!Box-Tracker des *alten* Boards heißt
`device_tracker.bienenwaage_esp32`, trägt aber den Anzeigenamen „stockwaage".
Die entity_id konserviert den Hostnamen zum Zeitpunkt der Anlage — das Board
hieß also einmal `bienenwaage-esp32` und wurde später auf `stockwaage`
umbenannt.

Daraus folgt eine Regel, die auch für den nächsten Wechsel gilt:

> **Der Dateiname im ESPHome-Add-on sagt nichts über den Gerätenamen.**
> Maßgeblich ist `geraete_name`/`anzeige_name` in den `substitutions`, und die
> bilden zusammen mit den `name:`-Feldern die entity_id. Wer nur die Datei
> ansieht, liest den falschen Namen.

## 2. Der Boardwechsel im Zeitverlauf

Die Zustandszeitstempel zeichnen den Wechsel nach — auf die Minute, alles am
28.08.2026:

| Zeit | Ereignis |
|---|---|
| 19:53 | letzte Messung des alten Boards: 34,5 kg, Betriebszeit 90 s |
| 19:55 | `binary_sensor.stockwaage_verbindung` → aus (Tiefschlaf) |
| 20:03 | Host `stockwaage` → `not_home`, kam nicht zurück |
| 20:36 | Host `bienenwaage` → `home` |

Der letzte gemessene Zustand des alten Boards, zur Sicherung:

| Größe | Wert |
|---|---|
| Gewicht | 34,5 kg |
| Kalibrierfaktor | −20.780,66 counts/kg |
| Kalibriert bei | 22,5 °C |
| Kalibriert mit | 33,62 kg |
| Rohwert | −734.241,75 |
| Rohwert Streuung | 10,84 |
| Temperatur | 22,44 °C |

Der Faktor liegt im Erwartungsbereich −18.000 bis −21.000. Für das neue Board
gilt er **nicht** — andere Mechanik-Verschraubung, andere Kalibrierung. Nach
der Adoption sind beide Kalibrierschritte zu fahren, nicht nur der
Referenzpunkt.

## 3. Warum der Umbau hier endet

Das neue Gerät ist in HA **entdeckt, aber nicht bestätigt**. Es hängt ein
offener Config-Flow:

```
flow_id  01M14S8M4WVSRFTXD0009ZP1M9
handler  esphome (zeroconf)
key      bienenwaage._esphomelib._tcp.local.
name     "Bienenwaage (bienenwaage)"
MAC      20:50:0d:ca:b2:bc
step     discovery_confirm
```

Deshalb taucht das Gerät weder in der Geräteliste noch unter den
ESPHome-Config-Entries auf, und deshalb existiert bislang **keine einzige**
`bienenwaage_*`-Entity. Eine persistente Meldung gibt es dazu nicht, nur die
Karte auf der Integrationsseite — das ist der Grund, warum der Zustand leicht
übersehen wird.

Zwei Wege, das von außen zu erledigen, wurden versucht und scheiterten:

- `config_entries/flow/configure` über die WebSocket-Schnittstelle → *Unknown
  command*, der Kanal ist für Flow-Fortsetzung gesperrt.
- Einen frischen Flow mit `host: bienenwaage.local` anstoßen → Timeout nach
  30 s. **Das ist erwartbar und kein Fehler:** das Gerät schläft und ist nur
  im Weckfenster erreichbar. Der Probe-Flow wurde von HA sauber verworfen, die
  ursprüngliche Discovery blieb unversehrt.

Der API-Schlüssel wäre der zweite Stolperstein: `packages/waage-basis.yaml:188`
setzt `api: encryption: key: !secret api_encryption_key`. Beim Bestätigen über
die Oberfläche holt HA den Noise-Key selbst aus dem Device-Builder-Add-on
(`home_assistant_dashboard_integration: true`), von außen ginge das nicht.

**Der offene Schritt ist deshalb ein Klick:** Einstellungen → Geräte & Dienste
→ die entdeckte „Bienenwaage" bestätigen.

## 4. Was danach zu tun ist

Die Bestandsaufnahme der Konsumenten ist vollständig — gesucht wurde über alle
Dashboards, Automationen, Skripte, Szenen und Helfer.

**Dashboard `bienen-stockwaage-esp32`** („Stockwaage", Storage-Mode, drei
Ansichten, ~22 KB): **47 Referenzen** auf `stockwaage_*`. Kein anderes
Dashboard ist betroffen. Die Treffer verteilen sich auf `entity:`-Felder,
vier View-Badges und fünf Markdown-Karten mit Jinja2 — die Badges und die
Templates hängen nicht an den Karten und werden von einer kartenorientierten
Suche übersehen.

Der Umbau erfolgt in place: die `url_path` lässt sich nicht ändern, damit
bleiben Sidebar-Eintrag und Lesezeichen erhalten. Der Dashboard-Titel und die
sichtbaren Beschriftungen ziehen auf „Bienenwaage" mit.

**Keine Automationen, Skripte oder Szenen** referenzieren die Waage. Die vier
in `waage-eg-notes.md` beschriebenen Automationen existieren nicht mehr.

**Vier Helfer** hängen an der alten Waage:

| Helfer | Typ | Konfiguration |
|---|---|---|
| `input_boolean.stockwaage_wachhalten` | input_boolean | geräteunabhängig, nur Namensfrage |
| `01M08F0DAF4RPTXSGKXM4MBA3C` „Gewichtsänderung" | derivative | Quelle `sensor.stockwaage_gewicht`, Fenster 6 h, kg/d, round 2 |
| `01M08F0MCN7Z80DNQTZ8KY05EF` „Gewichtsverlust kurz" | derivative | Quelle `sensor.stockwaage_gewicht`, Fenster 20 min, kg/h, round 2 |
| `01M08F0SYDFEHG4RPWZBPQR28A` „Tagesbilanz" | statistics | Quelle `sensor.stockwaage_gewicht`, `change`, 24 h, 2000 Samples |

Die drei Rechenhelfer laufen ins Leere, sobald `sensor.stockwaage_gewicht`
nicht mehr aktualisiert wird. Ihre Quelle ist über den Options-Flow
umzuhängen. Umbenennen geht dabei **nicht** mit: Options-Flows nehmen den
Schlüssel `name` nicht an. Wer konsistente `bienenwaage_*`-IDs will, benennt
zusätzlich über die Entity-Registry um — das erhält die Historie, anders als
Löschen und Neuanlegen.

Für die Historie gilt der Vorbehalt aus `CLAUDE.md`: ein Gewichtsverlauf ist
nur innerhalb **einer** Kalibrierung vergleichbar. Beim Umhängen der Quelle
läuft die alte und die neue Waage in derselben Reihe — für die beiden
Ableitungen ist das verkraftbar (ein Sprung an der Nahtstelle, danach wieder
sauber), für einen Absolutverlauf wäre es das nicht.

## 5. Offene Punkte

- **`bienenwaage.yaml` ist nicht im Repo.** Sie konnte in dieser Sitzung auch
  nicht geholt werden: der MCP-Token hat keinen Supervisor-Zugriff
  (`/addons` → *Unauthorized*), damit ist weder die Add-on-Dateiliste noch die
  WebSocket-Schnittstelle des Device Builders erreichbar. Die Datei muss von
  Hand eingecheckt werden, sonst beschreibt das Repo dauerhaft ein Gerät, das
  es nicht mehr gibt.
- **`waage-eg.yaml` beschreibt Hardware, die nicht mehr läuft.** Der zugehörige
  Config-Entry „Waage" steht in HA auf `unavailable`. Entweder als Historie
  kennzeichnen oder entfernen.
- **Board-Typ des neuen Geräts unbekannt**, bis die Adoption durch ist.
- Die Pin-Belegung in `waage-eg.yaml` (D1-Mini-Notation) passt nicht mehr zu
  einem ESP32. Der Durchsicht-Taster liegt laut Dashboard-Text inzwischen auf
  GPIO34 mit externem 10-kΩ-Pull-up.
