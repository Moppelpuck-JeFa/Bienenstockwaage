# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Sprache

**Alles auf Deutsch** — Code-Kommentare, YAML-Labels, Entity-Namen, Doku,
Commit-Messages. Das ist durchgehend so und soll so bleiben.

## Worum es geht

ESPHome-Bienenstockwaage (HX711 + 4 Wägezellen), angebunden an Home Assistant.

**Es gibt genau ein Gerät: `stockwaage`** — ESP32 DOIT DevKit V1, mit Deep
Sleep. Geflasht wird `stockwaage.yaml`, die Logik kommt aus
`packages/waage-basis.yaml`.

**`waage-eg` (ESP8266 D1 Mini) existiert nicht mehr** (Stand 19.08.2026 — weder
im ESPHome Device Builder noch als Entities in HA). Die Datei `waage-eg.yaml`
und die beiden nie gebauten `waage-stock2/3.yaml` liegen noch im Repo, ebenso
`waage-eg-notes.md` und `waage-eg-claude-code-kontext.md`. **Das ist Archiv, kein
laufender Betrieb.** Alles darin, was im Präsens von einem produktiven Gerät oder
von einer laufenden Messreihe spricht, ist überholt.

Zwei Dinge, die früher fast jede Änderung bestimmt haben und **jetzt nicht mehr
gelten**:

- „Eine Änderung an der Basis wirkt auf alle Stöcke" — es gibt nur noch einen
  Abnehmer. Die Basis darf geändert werden, ohne ein zweites Gerät zu gefährden.
- „Ein Flash kann die Kalibrierung kosten" — das war ein
  ESP8266-Preferences-Problem (siehe unten). Auf dem ESP32 fällt es strukturell
  weg.

Was **bleibt**: `stockwaage` sammelt eine Messreihe, ihre entity_ids hängen an
Dashboard und Helfern, und ein Ausreißer landet dauerhaft in der
Langzeitstatistik. Die Vorsicht gilt weiter — sie gilt jetzt diesem Gerät.

## Prüfen ohne Hardware

Ein vollständiger `esphome compile` ist hier nicht möglich (PlatformIO-Registry
durch die Egress-Policy gesperrt). Deshalb diese zwei Wege:

```bash
pip install esphome                    # zuletzt gegen 2026.7.4 geprüft
esphome config stockwaage.yaml

cd tests
g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

`esphome config` braucht eine `secrets.yaml` im Repo-Root (Vorlage:
`secrets.yaml.example`, per `.gitignore` ausgeschlossen — zum Prüfen anlegen,
danach wieder löschen).

**Die Standardprüfung für Änderungen an der Basis** ist der Vergleich der
*aufgelösten* Konfiguration, nicht das Lesen der YAML:

```bash
esphome config stockwaage.yaml | sed 's/\x1b\[[0-9;]*m//g' > vorher.txt
# ... Änderung ...
esphome config stockwaage.yaml | sed 's/\x1b\[[0-9;]*m//g' > nachher.txt
diff vorher.txt nachher.txt
```

Damit lässt sich belegen, dass Entity-Namen und Globals unangetastet bleiben.
Das ANSI-Entfernen ist nötig, sonst rauscht der Diff über maskierte Secrets.

Der g++-Test bindet die **echten** Header-Dateien aus `packages/` ein
(`waage-temperatur.h`, `waage-mittelwert.h`), prüft also den ausgelieferten
Code. Die Lambda-Körper aus `waage-basis.yaml` sind dort nachgebaut — wer sie
ändert, muss den Nachbau mitziehen. Das gilt auch für die Filterkette und das
Messfenster, die Punkt 10 für die Genauigkeitsaussage simuliert.

**Was `esphome config` NICHT fängt:** Optionen, deren Vorgabe der
Codegenerator setzt statt das YAML. Siehe `trigger_on_initial_state` unter
„Deep Sleep" — der Fehler war in der aufgelösten Konfiguration unsichtbar und
nur im ESPHome-Quelltext zu finden. Bei unerklärlichem Verhalten deshalb in
`site-packages/esphome/components/<komponente>/__init__.py` nachsehen, nicht
nur in der Doku.

## Architektur

**Package/Stock-Aufteilung.** `packages/waage-basis.yaml` enthält die gesamte
Logik und wird **nicht** geflasht. Geflasht wird `stockwaage.yaml` im Root, die
nur `substitutions` + `packages:`-Include + die ESP32- und Deep-Sleep-Teile
enthält. Die Aufteilung ist nicht kosmetisch: Das ESPHome Device Builder Add-on
listet ausschließlich YAML direkt in `/config/esphome/` als flashbare Geräte.
Läge die Basis im Root, tauchte sie als Pseudo-Gerät auf.

Folgen davon, die leicht überraschen:

- `!secret` in einem Package löst gegen das Verzeichnis der **geflashten**
  Stock-Datei auf. `secrets.yaml` bleibt im Root, und
  `esphome config packages/waage-basis.yaml` direkt aufzurufen schlägt fehl —
  das ist erwartet.
- Dasselbe gilt für `esphome: includes:`. Der Pfad muss
  `packages/waage-temperatur.h` lauten; ohne Präfix bricht schon
  `esphome config` ab.
- Werte aus der Stock-Datei haben Vorrang vor denen aus dem Package.
- Substitutions lassen sich **nicht** in `!secret` hineinschreiben.
- Listen aus Packages werden **aneinandergehängt**, nicht über die ID
  zusammengeführt (`merge_config` in `esphome/config_helpers.py`: `old + new`).
  Ein Eintrag mit derselben ID ist deshalb ein Duplikat-Fehler — dafür gibt es
  `!extend <id>`. Zum Löschen eines geerbten Schlüssels: `!remove`.

**Die Basis trägt noch einen `esp8266:`-Block**, den `stockwaage.yaml` per
`!remove` entfernt und durch `esp32:` ersetzt. Das war nötig, solange beide
Geräte dieselbe Basis teilten. Jetzt ist es nur noch Ballast — auflösen ist
zulässig, aber eine bewusste Aufräumaktion, kein Nebenbei-Edit.

**Zwei entkoppelte Takte.** Der HX711 läuft mit eigener Abtastung samt
Filterkette (Median 5 → gleitender Mittelwert), damit Kalibrier- und
Tara-Buttons immer einen frischen, eingeschwungenen Wert lesen. Alle in HA
sichtbaren Sensoren stehen auf `update_interval: never`; veröffentlicht wird
ausschließlich aus einem `interval: 60s`-Block, gesteuert von der Number-Entity
„Messintervall".

**Das Messintervall ist zugleich das Mittelungsfenster.** Jeder gefilterte
Rohwert und jeder Temperaturwert läuft in Summen-Globals (`fenster_*`,
`temp_fenster_*`); beim Veröffentlichen werden daraus `mittel_rohwert`,
`mittel_streuung` und `mittel_temperatur`. **Alle Entities rechnen gegen diese
drei Globals, nie mehr direkt gegen `hx711_raw_counts`.** Wer das umgeht,
veröffentlicht einen Wert aus einem anderen Zeitraum als der Rest. Die Rechnung
steht in `packages/waage-mittelwert.h`.

Daraus folgen drei Regeln:

- **Ein neuer Sensor gehört ins Skript `messwerte_veroeffentlichen`** — dort
  steht die Liste genau einmal (vorher dreimal, und regelmäßig blieb eine
  Stelle zurück; der Sensor war dann nach einem Neustart dauerhaft leer).
- **Rohwert und Temperatur müssen über dasselbe Fenster gemittelt werden.** Ein
  6-h-Mittel mit der Temperatur des Sendezeitpunkts zu korrigieren kostet
  ~98 g (Simulation, `tests/`, Punkt 10) und sieht dabei völlig plausibel aus.
- **Alles, was die Grundlage der Messung ändert, verwirft das Fenster** —
  Tara, beide Kalibrier-Buttons, Intervallwechsel, Durchsicht. Dafür gibt es
  `messfenster_frisch`, das zugleich auf den Momentanwert umschaltet.

**Kalibrierung lebt in `globals`** mit `restore_value`, nicht in
`calibrate_linear`. Zwei-Punkt-Kalibrierung über Buttons aus HA.

**Das Gewicht läuft vor dem Veröffentlichen durch ein Plausibilitätsfenster**
(`plausibel_kg_min`/`plausibel_kg_max`, Vorgabe −1 bis 150 kg; Regel in
`packages/waage-grenzen.h`). Der Grund ist nicht Kosmetik: `waage_gewicht` hat
`state_class: measurement`, ein Ausreißer steht damit **dauerhaft** in der
Langzeitstatistik, während der Zustandsverlauf nach ~10 Tagen wegfällt. Am
11.08.2026 zog eine kaputte Kalibrierung (+2.299 / −3.749 kg) das Stundenmittel
auf +70,8 kg. Daraus folgt:

- **Unplausibles wird verworfen, nicht gekappt** — ein Wert von 150,0 kg wäre
  genauso falsch, nur unauffälliger. Die Entity behält ihren letzten Stand.
- **Nur das Gewicht hängt am Fenster.** Rohwert, Streuung und Temperatur gehen
  weiter raus; ohne sie ließe sich nicht klären, warum das Gewicht fehlt.
- **Der Taktgeber fragt `erste_messung_erfolgt`, nicht
  `isnan(waage_gewicht.state)`.** Sonst käme er bei dauerhaft unplausiblen
  Werten nie aus dem Anlauf und schlösse jede Minute ein Messfenster ab.
- **Die Untergrenze liegt bei −1 kg, nicht bei 0.** Nach einem Tara rauscht
  die Anzeige um die Null; eine Grenze bei 0 schnitte die untere Hälfte ab und
  zöge den Mittelwert nach oben, ohne dass man es der Statistik ansieht.
  Belegt in `tests/`, Punkt 11: bis zu einem Stock bei −0,9 kg wird nichts
  verworfen.

**Tara und Kalibrieren sperren die Übertragung für `kalibrier_sperre` Minuten**
(Vorgabe 10, `"0"` schaltet ab). Das ist kein Duplikat des
Plausibilitätsfensters, sondern dessen Ergänzung: **ein aufgelegtes
Referenzgewicht von 26 kg ist von einem echten Stockgewicht nicht zu
unterscheiden.** Das Fenster fängt den kaputten Rechenweg, die Sperre den
falschen Messgegenstand. Daraus folgt:

- **Wer eine Regel gegen falsche Messwerte baut, muss zuerst fragen: „ist
  dieser Zeitraum überhaupt eine Messung?" — erst dann: „ist dieser Wert
  plausibel?"** Am 12.08.2026 andersherum gemacht und dafür die halbe
  Messreihe verloren.
- Während der Sperre gehen die Entities **mit** `state_class` nicht raus, die
  Diagnose **ohne** `state_class` schon — sonst fehlte beim Kalibrieren die
  Rückmeldung.
- **„Jetzt messen" beendet die Sperre.** Sie schützt gegen Vergessen, nicht
  gegen Absicht.
- Das Messfenster sammelt nicht, der Intervallzähler ruht. Beim Ablauf wird
  sofort veröffentlicht — wie beim Durchsicht-Nachlauf.

**Die Temperaturkompensation steht in `packages/waage-temperatur.h`** — genau
einmal, weil sie an drei Stellen gebraucht wird (Gewicht, Tara,
Diagnose-Entity). Die Funktion nimmt reine Zahlen und kennt keine `id()`; nur
deshalb ist sie mit g++ testbar.

**Die HA-Seite liegt nicht im Repo.** Helfer, Automationen und das Dashboard
`bienen-stockwaage-esp32` („Stockwaage", drei Ansichten) leben nur in Home
Assistant. Änderungen dort über die ha-mcp-Tools, nicht über YAML-Dateien.
Das ältere Dashboard `bienen-stockwaage` und die Automationen von `waage-eg`
sind in `waage-eg-notes.md` dokumentiert — Archiv.

## Deep Sleep

Nur in `stockwaage.yaml`, nicht in der Basis. Hardwareschalter über
`wakeup_pin` + `wakeup_pin_mode: KEEP_AWAKE`, Softwareschalter über einen
`homeassistant`-binary_sensor auf `input_boolean.stockwaage_wachhalten`.

**`trigger_on_initial_state: true` ist an einem `homeassistant`-Sensor
Pflicht, sobald `on_state` beim Aufwachen wirken soll.** Der Codegenerator
(`binary_sensor/__init__.py`) ruft `set_trigger_on_initial_state()` **immer**
und setzt ohne den Schlüssel `false` — die C++-Vorgabe `true` ist wirkungslos.
Der erste Wert aus HA kommt über `publish_initial_state()`, das vorher
`invalidate_state()` ruft, also mit `had_state == false`. Ergebnis:

> **`on_state` feuert erst ab dem zweiten Wert.**

An einem dauerhaft laufenden Gerät fällt das nie auf. An einem
Deep-Sleep-Gerät ist der erste Wert der einzige, auf den es ankommt. Gekostet
hat das den 19.08.2026 — siehe `docs/sessionbericht-2026-08-19.md`.

**`on_boot` darf sich nicht auf `api.connected` verlassen**, wenn es einen
`homeassistant`-Wert braucht: der Zustand trifft erst danach ein. Richtig ist
`wait_until` auf `id(<sensor>).has_state()`.

## Harte Regeln

- **Kein `calibrate_linear`.**
- **`geraete_name` + `anzeige_name` + die `name:`-Felder bilden die entity_id in
  HA.** An `stockwaage` nicht ändern, sonst legt HA neue Entities an und
  Verlauf, Helfer und Dashboard hängen an toten IDs.
- **Der Rohwert bleibt unkompensiert.** Er ist die Grundlage jeder künftigen
  Nachmessung des Temperaturkoeffizienten.
- **Tara muss die Temperaturkorrektur mit abziehen.** Sonst wandert die Drift
  nach dem Tarieren wieder ins Gewicht zurück — fällt beim Ausprobieren nicht auf.
- **Pins des ESP32 (siehe Kopf von `stockwaage.yaml`):** GPIO0/2/5/12/15 sind
  Boot-Strapping, GPIO6–11 hängen am Flash, GPIO1/3 an UART0, GPIO34–39 sind
  input-only ohne interne Pull-Widerstände.
- **Repo und Add-on-Kopie auseinanderlaufen zu lassen ist der teuerste Fehler
  dieses Projekts.** Er ist dreimal passiert. Vor jeder Fehlersuche am Gerät
  erst `expected_config_hash` gegen `deployed_config_hash` prüfen
  (`/devices` der Add-on-API, Port 6052 — Ingress gibt 403) und den geflashten
  Inhalt über `devices/get_config` gegenlesen.

**Nur noch historisch, für ESP8266-Geräte:** `restore_from_flash: true` war
dort Pflicht, GPIO16 (D0) musste als Deep-Sleep-Weckpin frei bleiben, und ein
neues `restore_value`-Global verschob den Speicherplatz aller nachfolgenden
(`esp8266/preferences.cpp`: `current_flash_offset += total_words`, der
Namens-Hash geht nur in die CRC) — das hat am 03.08. und 10.08.2026 die
Kalibrierung gekostet. **Auf dem ESP32 gilt das nicht:** jedes Global bekommt
einen eigenen NVS-Schlüssel (`1944399030 ^ name_hash`), Reihenfolge egal.
Dafür löscht `erase_flash` bzw. „Clean Build Files" das NVS mit.

## Nach jedem Flash prüfen

1. **Kalibrierfaktor** — Erwartungsbereich **−18.000 bis −21.000 counts/kg**
   (zuletzt −20.780,66). Exakt `3.500` heißt: auf die Platzhalter
   zurückgefallen. Rund die Hälfte des Erwartungswerts heißt: nur der
   Referenzpunkt wurde gesetzt, der Nullpunkt fehlt.
2. **„Kalibriert bei" darf nicht leer sein** — sonst fehlt der Bezugspunkt und
   die Temperaturkompensation schaltet sich stumm ab.

Im Zweifel **beide** Kalibrierschritte fahren, nicht nur den Referenzpunkt.
Auf dem ESP32 ist ein neues Global kein Auslöser mehr, ein NVS-Erase schon.

## Wo was steht

| Datei | Inhalt |
|---|---|
| [`stockwaage.yaml`](stockwaage.yaml) | **Das Gerät.** Pins, Deep Sleep, ESP32-Abweichungen — durchgehend kommentiert |
| [`docs/sessionbericht-*.md`](docs) | Chronologie. Wenn eine Entscheidung unbegründet wirkt, steht das Warum meist hier |
| [`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md) | Abschnitt 9 ist der ESP32-Teil; 1–8 sind für den ESP8266 geschrieben |
| [`README.md`](README.md) | Bedienung: Inbetriebnahme, Kalibrieren, Messintervall, Durchsichtmodus, Temperaturkompensation, Fehlersuche |
| [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md) | Abschnitt 0 ist der relevante (Halbbrücken-Ring), 1–6 sind Referenz für ein späteres Upgrade |
| [`waage-eg-claude-code-kontext.md`](waage-eg-claude-code-kontext.md) | **Archiv.** Übergabe für das abgebaute ESP8266-Gerät; Design-Begründungen gelten weiter, Istwerte nicht |
| [`waage-eg-notes.md`](waage-eg-notes.md) | **Archiv.** Ausführliche Begründungen und die HA-Seite von `waage-eg` |

## Konventionen dieses Repos

- **Kommentare erklären das Warum, nicht das Was**, und stehen direkt an der
  Stelle. `waage-basis.yaml` und `stockwaage.yaml` sind bewusst
  kommentarlastig — das ist der Stil hier, nicht Redundanz.
- **Pro Arbeitssitzung ein Sessionbericht** in `docs/`, mit Messwerten,
  Entscheidungen und den offenen Punkten am Ende.
- **Ergebnisse belegen statt behaupten.** Zahlen kommen aus Messung oder
  Simulation; wo etwas geschätzt ist, steht das dabei.
