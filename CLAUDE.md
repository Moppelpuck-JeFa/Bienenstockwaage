# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Sprache

**Alles auf Deutsch** — Code-Kommentare, YAML-Labels, Entity-Namen, Doku,
Commit-Messages. Das ist durchgehend so und soll so bleiben.

## Worum es geht

ESPHome-Bienenstockwaage (ESP8266 D1 Mini + HX711 + 4 Wägezellen), angebunden an
Home Assistant. **Ein Gerät läuft produktiv** (`waage-eg`) und sammelt seit
Wochen eine durchgehende Messreihe; zwei weitere Stock-Dateien sind vorbereitet,
aber ohne Hardware.

Daraus folgen zwei Dinge, die fast jede Änderung betreffen:

- **Eine Änderung an `packages/waage-basis.yaml` wirkt beim nächsten Flash auf
  alle Stöcke.** Vorher gegen jeden prüfen.
- **Ein Flash kann die Kalibrierung kosten** — siehe unten. Das ist kein
  Randfall, sondern zweimal real passiert.

## Prüfen ohne Hardware

Ein vollständiger `esphome compile` ist hier nicht möglich (PlatformIO-Registry
durch die Egress-Policy gesperrt). Deshalb diese zwei Wege:

```bash
pip install esphome                    # zuletzt gegen 2026.6.5 geprüft
esphome config waage-eg.yaml           # ebenso waage-stock2.yaml, waage-stock3.yaml

cd tests
g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

`esphome config` braucht eine `secrets.yaml` im Repo-Root (Vorlage:
`secrets.yaml.example`, per `.gitignore` ausgeschlossen — zum Prüfen anlegen,
danach wieder löschen).

**Die Standardprüfung für Änderungen an der Basis** ist der Vergleich der
*aufgelösten* Konfiguration, nicht das Lesen der YAML:

```bash
esphome config waage-eg.yaml | sed 's/\x1b\[[0-9;]*m//g' > vorher.txt
# ... Änderung ...
esphome config waage-eg.yaml | sed 's/\x1b\[[0-9;]*m//g' > nachher.txt
diff vorher.txt nachher.txt
```

Damit lässt sich belegen, dass Entity-Namen und Globals unangetastet bleiben.
Das ANSI-Entfernen ist nötig, sonst rauscht der Diff über maskierte Secrets.

Der g++-Test bindet die **echte** Header-Datei aus `packages/` ein, prüft also
den ausgelieferten Code. Die Lambda-Körper aus `waage-basis.yaml` sind dort
nachgebaut — wer sie ändert, muss den Nachbau mitziehen.

## Architektur

**Package/Stock-Aufteilung.** `packages/waage-basis.yaml` enthält die gesamte
Logik und wird **nicht** geflasht. Geflasht werden die Stock-Dateien im Root, die
nur `substitutions` + `packages:`-Include enthalten. Die Aufteilung ist nicht
kosmetisch: Das ESPHome Device Builder Add-on listet ausschließlich YAML direkt
in `/config/esphome/` als flashbare Geräte. Läge die Basis im Root, tauchte sie
als Pseudo-Gerät auf.

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

**Zwei entkoppelte Takte.** Der HX711 läuft mit `update_interval: 1s` samt
Filterkette (Median 5 → gleitender Mittelwert ~60 s), damit Kalibrier- und
Tara-Buttons immer einen frischen, eingeschwungenen Wert lesen. Alle in HA
sichtbaren Sensoren stehen auf `update_interval: never`; veröffentlicht wird
ausschließlich aus einem `interval: 60s`-Block, gesteuert von der Number-Entity
„Messintervall". Wer einen neuen Sensor ergänzt, muss ihn dort **und** in den
Buttons mit aktualisieren, sonst bleibt er nach einem Neustart leer.

**Kalibrierung lebt in `globals`** mit `restore_value`, nicht in
`calibrate_linear`. Zwei-Punkt-Kalibrierung über Buttons aus HA.

**Die Temperaturkompensation steht in `packages/waage-temperatur.h`** — genau
einmal, weil sie an drei Stellen gebraucht wird (Gewicht, Tara,
Diagnose-Entity). Die Funktion nimmt reine Zahlen und kennt keine `id()`; nur
deshalb ist sie mit g++ testbar.

**Die HA-Seite liegt nicht im Repo.** Helfer, Automationen und das Dashboard
`bienen-stockwaage` leben nur in Home Assistant und sind in
`waage-eg-notes.md` dokumentiert. Änderungen dort über die ha-mcp-Tools, nicht
über YAML-Dateien.

## Harte Regeln

- **`restore_from_flash: true` nicht entfernen.** Ohne die Zeile landen alle
  `restore_value`-Globals nur im RTC-RAM und sind bei jedem Stromausfall weg.
- **Kein `calibrate_linear`.**
- **GPIO16 (D0) bleibt frei** — einziger Deep-Sleep-Weckpin des ESP8266.
  D4 (GPIO2) ist Boot-Strapping-Pin und für den geplanten MOSFET reserviert.
- **`geraete_name` + `anzeige_name` + die `name:`-Felder bilden die entity_id in
  HA.** An `waage-eg` nicht ändern, sonst legt HA neue Entities an und Verlauf,
  Helfer, Automationen und Dashboard hängen an toten IDs.
- **Der Rohwert bleibt unkompensiert.** Er ist die Grundlage jeder künftigen
  Nachmessung des Temperaturkoeffizienten.
- **Tara muss die Temperaturkorrektur mit abziehen.** Sonst wandert die Drift
  nach dem Tarieren wieder ins Gewicht zurück — fällt beim Ausprobieren nicht auf.

## Nach jedem Flash prüfen

1. **Kalibrierfaktor** — Erwartungsbereich **−18.000 bis −21.000 counts/kg**
   (der genaue Wert streut über Kalibrierungen hinweg). Exakt `3.500` heißt:
   auf die Platzhalter zurückgefallen. Rund die Hälfte des Erwartungswerts
   heißt: nur der Referenzpunkt wurde gesetzt, der Nullpunkt fehlt.
2. **„Kalibriert bei" darf nicht leer sein** — sonst fehlt der Bezugspunkt und
   die Temperaturkompensation schaltet sich stumm ab.

Im Zweifel **beide** Kalibrierschritte fahren, nicht nur den Referenzpunkt.
Auslöser ist typischerweise ein Flash, der ein neues `globals`-Element
mitbringt; `restore_from_flash` schützt dagegen nicht.

## Wo was steht

| Datei | Inhalt |
|---|---|
| [`waage-eg-claude-code-kontext.md`](waage-eg-claude-code-kontext.md) | **Zuerst lesen.** Vollständige Übergabe: Istwerte, alle Design-Entscheidungen mit Begründung, Werkzeug-Fallstricke, offene Punkte |
| [`waage-eg-notes.md`](waage-eg-notes.md) | Ausführliche Begründungen und die komplette HA-Seite (Helfer, Automationen, Dashboard) |
| [`README.md`](README.md) | Bedienung: Inbetriebnahme, Kalibrieren, Messintervall, Durchsichtmodus, Temperaturkompensation, Fehlersuche |
| [`docs/sessionbericht-*.md`](docs) | Chronologie. Wenn eine Entscheidung unbegründet wirkt, steht das Warum meist hier |
| [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md) | Abschnitt 0 ist der relevante (Halbbrücken-Ring), 1–6 sind Referenz für ein späteres Upgrade |
| [`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md) | Ausgearbeitet, noch nicht umgesetzt |

## Konventionen dieses Repos

- **Kommentare erklären das Warum, nicht das Was**, und stehen direkt an der
  Stelle. `waage-basis.yaml` ist bewusst kommentarlastig — das ist der Stil hier,
  nicht Redundanz.
- **Pro Arbeitssitzung ein Sessionbericht** in `docs/`, mit Messwerten,
  Entscheidungen und den offenen Punkten am Ende.
- **Ergebnisse belegen statt behaupten.** Zahlen kommen aus Messung oder
  Simulation; wo etwas geschätzt ist, steht das dabei.
