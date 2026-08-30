# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Sprache

**Alles auf Deutsch** — Code-Kommentare, YAML-Labels, Entity-Namen, Doku,
Commit-Messages. Das ist durchgehend so und soll so bleiben.

## Worum es geht

ESPHome-Bienenstockwaage (ESP32 DOIT DevKit V1 + HX711 + 4 Wägezellen),
angebunden an Home Assistant. **Ein Gerät läuft produktiv** — `bienenwaage.yaml`,
in ESPHome und HA unter dem Gerätenamen `stockwaage` geführt.

Seit dem 28.08.2026 ist es das einzige. Die ESP8266-Dateien `waage-eg.yaml`,
`waage-stock2.yaml` und `waage-stock3.yaml` sind entfernt, ebenso der
`esp8266:`-Block aus der Basis. `waage-eg-notes.md` und
`waage-eg-claude-code-kontext.md` bleiben als **Archiv** stehen: die
Begründungen darin gelten weiter, die Istwerte nicht.

**Gerätename und Entity-IDs laufen auseinander, und zwar mit Absicht.**
`geraete_name` steht auf `stockwaage`, die Entities in HA heißen
`bienenwaage_*`. Umbenannt wurde nur in der HA-Entity-Registry, damit die
Messreihe erhalten bleibt — `geraete_name` nachzuziehen legt neue Entities an.
Die einzige Stelle, an der die Firmware umgekehrt eine HA-Entity referenziert,
ist `entity_id: input_boolean.bienenwaage_wachhalten` in `bienenwaage.yaml`;
wird der Helfer umbenannt, fällt das Wachhalten über HA stumm aus.

Daraus folgen zwei Dinge, die fast jede Änderung betreffen:

- **`packages/waage-basis.yaml` hat nur noch einen Abnehmer**, aber es ist der
  produktive. Die Aufteilung bleibt trotzdem: das Device-Builder-Add-on listet
  nur YAML direkt in `/config/esphome/` als flashbares Gerät.
- **Ein Flash kann die Kalibrierung kosten** — siehe unten. Auf dem ESP32 ist
  die Hauptursache strukturell entfallen, die Prüfung bleibt.

## Prüfen ohne Hardware

Ein vollständiger `esphome compile` ist hier nicht möglich (PlatformIO-Registry
durch die Egress-Policy gesperrt). Deshalb diese zwei Wege:

```bash
pip install esphome                    # zuletzt gegen 2026.6.5 geprüft
esphome config bienenwaage.yaml

cd tests
g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

`esphome config` braucht eine `secrets.yaml` im Repo-Root (Vorlage:
`secrets.yaml.example`, per `.gitignore` ausgeschlossen — zum Prüfen anlegen,
danach wieder löschen).

**Die Standardprüfung für Änderungen an der Basis** ist der Vergleich der
*aufgelösten* Konfiguration, nicht das Lesen der YAML:

```bash
esphome config bienenwaage.yaml | sed 's/\x1b\[[0-9;]*m//g' > vorher.txt
# ... Änderung ...
esphome config bienenwaage.yaml | sed 's/\x1b\[[0-9;]*m//g' > nachher.txt
diff vorher.txt nachher.txt
```

Damit lässt sich belegen, dass Entity-Namen und Globals unangetastet bleiben.
Das ANSI-Entfernen ist nötig, sonst rauscht der Diff über maskierte Secrets.

**Der Diff ist nicht deterministisch.** Am 28.08.2026 festgestellt: zwei Läufe
von `esphome config` über denselben, unveränderten Baum liefern
unterschiedliche Ausgaben — die Schlüssel `tag` und `level` in den
`logger.log`-Aktionen tauschen die Reihenfolge. Das sind Falschmeldungen. Wer
sie für eine echte Änderung hält, sucht stundenlang nach nichts.

Deshalb bei jedem Verdacht gegenprüfen, ob das Rauschen ist — zweimal auf
demselben Baum laufen lassen — oder gleich reihenfolgeunabhängig vergleichen:

```bash
diff <(sort vorher.txt) <(sort nachher.txt)
```

Der g++-Test bindet die **echten** Header-Dateien aus `packages/` ein
(`waage-temperatur.h`, `waage-mittelwert.h`), prüft also den ausgelieferten
Code. Die Lambda-Körper aus `waage-basis.yaml` sind dort nachgebaut — wer sie
ändert, muss den Nachbau mitziehen. Das gilt auch für die Filterkette und das
Messfenster, die Punkt 10 für die Genauigkeitsaussage simuliert.

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
„Messintervall".

**Das Messintervall ist zugleich das Mittelungsfenster.** Jeder gefilterte
Rohwert (alle 5 s) und jeder Temperaturwert (alle 60 s) läuft in Summen-Globals
(`fenster_*`, `temp_fenster_*`); beim Veröffentlichen werden daraus
`mittel_rohwert`, `mittel_streuung` und `mittel_temperatur`. **Alle Entities
rechnen gegen diese drei Globals, nie mehr direkt gegen `hx711_raw_counts`.**
Wer das umgeht, veröffentlicht einen Wert aus einem anderen Zeitraum als der
Rest. Die Rechnung steht in `packages/waage-mittelwert.h`.

Daraus folgen drei Regeln:

- **Ein neuer Sensor gehört ins Skript `messwerte_veroeffentlichen`** — dort
  steht die Liste jetzt genau einmal (vorher dreimal, und regelmäßig blieb eine
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
- **Der Taktgeber fragt `erste_messung_erfolgt`, nicht mehr
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
`bienen-stockwaage` leben nur in Home Assistant und sind in
`waage-eg-notes.md` dokumentiert. Änderungen dort über die ha-mcp-Tools, nicht
über YAML-Dateien.

## Harte Regeln

- **`restore_from_flash: true` gilt nur für den ESP8266** und ist am
  28.08.2026 mit dem `esp8266:`-Block entfallen. Auf dem ESP8266 war die Zeile
  zwingend: ohne sie landeten alle `restore_value`-Globals nur im RTC-RAM und
  waren bei jedem Stromausfall weg. Auf dem ESP32 gibt es keine Entsprechung
  und es braucht keine — dort bekommt jedes Global einen eigenen NVS-Schlüssel
  im Flash. **Wer wieder ein ESP8266-Gerät anlegt, muss beides zurückholen**,
  und zwar in dessen Gerätedatei, nicht in der Basis.
- **Kein `calibrate_linear`.**
- **Der `binary_sensor` auf dem Weckpin ist funktional, nicht kosmetisch.**
  `deep_sleep` ruft für seinen `wakeup_pin` nie `pin_->setup()` auf — den Pin
  richtet allein dieser `binary_sensor` ein. Ohne ihn liest `KEEP_AWAKE` einen
  unkonfigurierten Pad, hält ihn für „Schalter an" und das Gerät schläft nie
  wieder ein. Am 29.08.2026 am Gerät passiert. Allgemeiner: **bevor eine
  GPIO-Komponente als „nur Anzeige" entfernt wird, prüfen, was ihr `setup()`
  nebenbei einrichtet.**
- **`minimum_chip_revision: "3.1"` bindet die Firmware an dieses Board.** Auf
  einem älteren ESP32 startet das Bild nicht. Bei einem Boardtausch die
  Revision im Startlog prüfen und den Wert notfalls senken. Dasselbe gilt für
  `sram1_as_iram: true`: erlaubt ist es nur, solange das Gerät im Log
  „Bootloader supports SRAM1 as IRAM" meldet — bei „Bootloader too old" erst
  einmal über USB flashen, ein OTA erneuert den Bootloader nicht.
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

1. **Kalibrierfaktor** — Größenordnung **−10.000 bis −30.000 counts/kg**,
   und zwar **negativ**. Exakt `3.500` heißt: auf die Platzhalter
   zurückgefallen. Rund die **Hälfte** des zuletzt gemessenen Werts heißt: nur
   der Referenzpunkt wurde gesetzt, der Nullpunkt fehlt. **Positiv** oder
   sechsstellig heißt: Nullpunkt und Referenzpunkt stammen aus verschiedenen
   Zuständen — meist lag ein Neustart dazwischen.

   > Der frühere enge Bereich −18.000 bis −21.000 stammte vom ESP8266-Aufbau
   > und **gilt nicht mehr**. Gemessen wurden inzwischen −20.756 (ESP8266),
   > −14.081 (ESP32-Board 2, nie gegengeprüft) und **−24.356** (ESP32-Board 3,
   > am 30.08.2026 mit bekanntem Gewicht verifiziert). Der Faktor ist also nur
   > ein Grobfilter.

   **Die eigentliche Prüfung ist das bekannte Gewicht:** Prüfgewicht auflegen,
   die Anzeige muss dessen Masse zeigen. Am 30.08.2026: 22,20 kg angezeigt bei
   22,221 kg aufgelegt — 21 g Abweichung. Das ist der Beleg, den kein
   Faktorbereich ersetzt.
2. **„Kalibriert bei" darf nicht leer sein** — sonst fehlt der Bezugspunkt und
   die Temperaturkompensation schaltet sich stumm ab.

Im Zweifel **beide** Kalibrierschritte fahren, nicht nur den Referenzpunkt.
Auslöser ist typischerweise ein Flash, der ein neues `globals`-Element
mitbringt; `restore_from_flash` schützt dagegen nicht.

Genauer, aus `esphome/components/esp8266/preferences.cpp` (2026.6.5): Der
Speicherplatz eines `restore_value`-Globals ergibt sich aus der **Reihenfolge**
der `make_preference()`-Aufrufe, nicht aus seinem Namen. Ein neues Global mit
`restore_value: yes` verschiebt deshalb alles, was danach kommt — die CRC
schlägt fehl und die Werte fallen auf `initial_value` zurück. Ein Global mit
`restore_value: no` fordert gar keinen Speicher an und ist damit unkritisch.
Das erklärt beide Vorfälle und ist der Grund, warum neue Globals nach Möglichkeit
`restore_value: no` bekommen sollten.

**Am 12.08.2026 erstmals am Gerät bestätigt:** Ein Flash mit **drei** neuen
Globals (`verworfene_gewichte`, `erste_messung_erfolgt`,
`kalibrier_restminuten`), alle `restore_value: no`, hat die Kalibrierung
unangetastet gelassen — Faktor vorher und nachher −20.755,91, „Kalibriert bei"
22,2 °C.

**Am 29.08.2026 auf dem ESP32 bestätigt:** Ein Flash mit zwei neuen
sdkconfig-Optionen (`minimum_chip_revision`, `sram1_as_iram`) und geänderten
WLAN-Parametern hat den Faktor bei −14.081,15 und „Kalibriert bei" bei 20,9 °C
gelassen. Auf dem ESP32 ist die strukturelle Ursache ohnehin entfallen — jedes
Global hat dort einen eigenen NVS-Schlüssel. Zwei Datenpunkte sind kein
Beweis: **trotzdem nach jedem Flash prüfen.**

## Wo was steht

| Datei | Inhalt |
|---|---|
| [`waage-eg-claude-code-kontext.md`](waage-eg-claude-code-kontext.md) | **Zuerst lesen.** Vollständige Übergabe: Istwerte, alle Design-Entscheidungen mit Begründung, Werkzeug-Fallstricke, offene Punkte |
| [`waage-eg-notes.md`](waage-eg-notes.md) | Ausführliche Begründungen und die komplette HA-Seite (Helfer, Automationen, Dashboard) |
| [`README.md`](README.md) | Bedienung: Inbetriebnahme, Kalibrieren, Messintervall, Durchsichtmodus, Temperaturkompensation, Fehlersuche |
| [`docs/sessionbericht-*.md`](docs) | Chronologie. Wenn eine Entscheidung unbegründet wirkt, steht das Warum meist hier |
| [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md) | Abschnitt 0 ist der relevante (Halbbrücken-Ring), 1–6 sind Referenz für ein späteres Upgrade |
| [`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md) | Abschnitt 9 gilt für den ESP32 und ist umgesetzt; 1–8 sind für den ESP8266 geschrieben und stimmen teils nicht mehr |
| [`bienenwaage.yaml`](bienenwaage.yaml) | Die geflashte Gerätedatei: Pins, Deep Sleep, HX711-Lastschalter, alle ESP32-Abweichungen von der Basis |

## Konventionen dieses Repos

- **Kommentare erklären das Warum, nicht das Was**, und stehen direkt an der
  Stelle. `waage-basis.yaml` ist bewusst kommentarlastig — das ist der Stil hier,
  nicht Redundanz.
- **Pro Arbeitssitzung ein Sessionbericht** in `docs/`, mit Messwerten,
  Entscheidungen und den offenen Punkten am Ende.
- **Ergebnisse belegen statt behaupten.** Zahlen kommen aus Messung oder
  Simulation; wo etwas geschätzt ist, steht das dabei.
