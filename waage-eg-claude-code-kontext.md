# Bienenstockwaage "waage-eg" — Übergabe-Kontext für Claude Code

> Diese Datei fasst den aktuellen Stand des Projekts zusammen. Sie ist so geschrieben,
> dass eine neue Claude-Instanz (z. B. in Claude Code) direkt produktiv weiterarbeiten kann,
> ohne den gesamten bisherigen Chatverlauf zu kennen.

---

## 1. Worum geht es

Eine Bienenstockwaage auf Basis eines ESP8266 (D1 Mini) mit ESPHome, angebunden an
Home Assistant (HA). Sie wiegt einen Bienenstock kontinuierlich, damit man Futtervorrat,
Gewichtsverlauf und Schwarm-Ereignisse (plötzlicher Gewichtsabfall) erkennen kann.

**Status: Läuft bereits produktiv.** Gerät ist geflasht und kalibriert, die komplette
HA-Seite (7 Helfer, 4 Automationen, 3-Ansichten-Dashboard) ist eingerichtet. Es handelt
sich jetzt um Feinschliff, offene Messungen und mögliche Erweiterungen — nicht mehr um
einen Neuaufbau von Null.

**Repo:** `github.com/Moppelpuck-JeFa/Bienenstockwaage` (privat). **Stand: 11.08.2026.**
Temperaturkompensation (PR #1) und Messfenster-Mittelung sind beide nach `main`
gemerged; Entwicklungsbranches gibt es aktuell keine.

**Geflasht ist der Stand vom 11.08. 19:21 Uhr.** Der Fix „Beim Booten keine
Platzhalter-Kalibrierwerte nach HA senden" kam danach und ist **noch nicht auf
dem Gerät**. Ebenfalls offen: **die Kalibrierung ist kaputt** (Faktor +753,58
statt −18.000…−21.000, gebrochen am 11.08. um 13:59 und damit vor dem Flash) —
beide Kalibrierschritte müssen neu gefahren werden. Details im
[Sessionbericht 11.08., Abschnitt 10](docs/sessionbericht-2026-08-11.md).

**Chronologie in den Sessionberichten** — bei "warum ist das so?" zuerst dort nachsehen:

| Datum | Thema |
|---|---|
| [03.08.](docs/sessionbericht-2026-08-03.md) | Temperaturdrift erstmals ausgewertet, Deep-Sleep-Vorbereitung, Durchsichtmodus, Kalibrierungsverlust |
| [04.08.](docs/sessionbericht-2026-08-04.md) | Umstellung auf substitutions/packages, Namensschema |
| [10.08.](docs/sessionbericht-2026-08-10.md) | Temperaturkompensation eingebaut, Schwarm-Alarm gesperrt |
| [11.08.](docs/sessionbericht-2026-08-11.md) | Rohwerte über das Messintervall gemittelt, Diagnose "Rohwert Streuung" und "Temperatur Mittel" |
| [12.08.](docs/sessionbericht-2026-08-12.md) | Plausibilitätsfenster 0–150 kg, Zähler "Gewicht verworfen", Bestandsaufnahme der Ausreißer in der Langzeitstatistik |

**Wichtig:** Workflow-Sprache ist Deutsch — Code-Kommentare, YAML-Labels, Entity-Namen
und Doku sind alle auf Deutsch gehalten. Bitte das beibehalten.

---

## 2. Hardware (verbaut)

| Teil | Wert |
|---|---|
| Board | ESP8266, `d1_mini` |
| HX711 | `dout_pin: D6` (GPIO12), `clk_pin: D1` (GPIO5), gain 128 |
| Wägezellen | 4× 50 kg **YZC-161-Typ = Halbbrücken (3-adrig)**, im Ring zu **einer** Vollbrücke verschaltet |
| DS18B20 (Temperatur) | Pin `D5` (GPIO14), externer Pull-up 4,7 kΩ gegen **3V3** |
| Durchsicht-Taster | `D2` (GPIO4) gegen GND, interner Pull-up |
| Durchsicht-LED | `D7` (GPIO13) gegen GND, 1 kΩ Vorwiderstand |

**Gemessene Istwerte (Stand 10.08.2026, nach der Neukalibrierung um 15:01/15:03):**
- Kalibrierfaktor: **−20.874 counts/kg** (negativ = invertierte Signalpolarität, funktional unkritisch)
- Kalibriert mit 2,218 kg Referenzgewicht, "Kalibriert bei" **25,8 °C**
- Temperaturkoeffizient: **+32,5 g/K**
- Rohwert leer: ~26.700 counts → rund 400 kg rechnerischer ADC-Vorrat (mechanisches Limit von 200 kg greift vorher)
- WLAN-Signal: −74 bis −76 dBm (Grenzbereich, beobachten)

> **Der Kalibrierfaktor streut über die Kalibrierungen hinweg** — dieser Aufbau
> lieferte nacheinander −17.900, −20.840 und jetzt −20.874. Das ist die
> Kalibrierung, nicht die Hardware. Für die Fehlersuche gilt deshalb ein
> **Erwartungsbereich von −18.000 bis −21.000**, nicht ein fester Wert. Wo in
> README und Notizen noch ältere Zahlen stehen, sind sie ausdrücklich als
> historisch gekennzeichnet.

---

## 3. Zentrale Design-Entscheidungen (und warum)

### Kalibrierung
- **Kein `calibrate_linear` im YAML.** Stattdessen Zwei-Punkt-Kalibrierung komplett aus
  HA heraus über zwei virtuelle Buttons, Werte liegen in `globals` mit `restore_value: yes`.
- `calib_kg_ref` (die tatsächlich beim Kalibrieren gespeicherte Masse) ist **bewusst
  getrennt** von der Number-Entity `Referenzgewicht` (nur Eingabefeld für die *nächste*
  Kalibrierung). Sonst würde ein späteres Ändern der Zahl rückwirkend alle Messwerte
  umskalieren.
- Span-Prüfung: bricht ab, wenn Rohwert-Differenz < 500 counts (häufigster Fehler:
  Gewicht nicht aufgelegt oder die 60 s Filterlaufzeit nicht abgewartet).
- Referenzgewicht bewusst **schwer wählen** — je kleiner die Referenzmasse, desto größer
  der Ablesefehler-Hebel (Beispiel: 20 counts Fehler → 222 g Fehler bei 0,5 kg Referenz,
  aber nur 11 g bei 10 kg Referenz).

### Sampling / Taktung
- HX711 läuft mit `update_interval: 1s` (schnell), damit die Kalibrier-/Tara-Buttons
  immer einen frischen gefilterten Wert lesen — auch wenn HA selbst nur alle paar
  Stunden einen Wert bekommt.
- HA-Taktung ist über einen `interval: 60s`-Block entkoppelt, gesteuert durch die
  Number-Entity `Messintervall`. Sichtbare Sensoren stehen auf `update_interval: never`.
- Filterkette: `median` (5 Werte) → `sliding_window_moving_average` (12 × 5 s ≈ 60 s).
- **Das Messintervall ist zugleich das Mittelungsfenster (seit 11.08.2026).** Jeder
  gefilterte Rohwert (alle 5 s) und jeder Temperaturwert (alle 60 s) läuft in
  Summen-Globals; beim Veröffentlichen entstehen daraus `mittel_rohwert`,
  `mittel_streuung`, `mittel_temperatur`, und **alle** Entities rechnen gegen diese
  drei — nie mehr direkt gegen `hx711_raw_counts`. Bei 6 h gehen ~4.300 Werte in eine
  Zahl ein statt 12; simulierte Reststreuung 2,2 g → 0,1 g. Details im
  [Sessionbericht 11.08.](docs/sessionbericht-2026-08-11.md).
- **Momentanwert statt Fenster** liefern: "Jetzt messen", Tara, beide Kalibrier-Buttons,
  ein Intervallwechsel und der erste Wert nach dem Durchsicht-Nachlauf. Alle rufen
  dafür das Skript `messfenster_frisch` auf, das das laufende Fenster verwirft.
- **Die Temperatur muss über dasselbe Fenster gemittelt werden** wie der Rohwert. Mit
  der Temperatur des Sendezeitpunkts zu korrigieren kostet bei 6 h Intervall ~98 g
  und sieht dabei plausibel aus. Dafür die Diagnose-Entity "Temperatur Mittel";
  "Temperatur" selbst bleibt der Momentanwert im 60-s-Takt.
- Minutenzähler statt `millis()`, weil `millis()` nach ~49 Tagen überläuft.
- **Der Minutenzähler wird zurückgesetzt von: jedem Neustart, "Jetzt messen", einer
  Änderung des Messintervalls und dem Ende des Durchsicht-Nachlaufs.** Das sieht
  in HA leicht nach "die Waage aktualisiert nicht mehr" aus — bei 60 min Intervall
  und ein paar Neustarts hintereinander kommt schlicht lange kein planmäßiger Wert.
  Am 10.08. genau so aufgetreten und als Fehlalarm entlarvt: erste planmäßige
  Messung um 16:04:06, exakt 3600 s nach dem letzten Reset. **Zum Prüfen die
  Betriebszeit ansehen, nicht die Uhr** — sie wird zusammen mit dem Gewicht
  veröffentlicht, ihr Zeitstempel ist also der Zeitpunkt der letzten Messung.
- Seit der Temperaturkompensation wartet der Taktgeber nach einem Neustart bis zu
  3 min auf den ersten Temperaturwert, bevor er den ersten Wert nach HA schickt
  (nur wenn die Kompensation aktiv ist). Ein defekter DS18B20 kostet damit
  3 Minuten, legt die Waage aber nicht still.

### Plausibilitätsfenster des Gewichts (seit 12.08.2026)
- Veröffentlicht wird nur, was **zwischen 0 und 150 kg** liegt
  (`plausibel_kg_min`/`plausibel_kg_max` als substitutions, Regel in
  `packages/waage-grenzen.h`). Alles andere wird **verworfen, nicht gekappt**.
- **Der Grund ist die Langzeitstatistik.** `state_class: measurement` heißt:
  jeder Wert bleibt für immer. Am 11.08.2026 gingen während der kaputten
  Kalibrierung +2.299,4 / +1.035,3 / −3.749,7 / −48,6 kg nach HA; das
  Stundenmittel von 18:00 UTC steht seitdem bei +70,8 kg statt ~26,7 kg.
- **Nur das Gewicht hängt am Fenster**, die Diagnose-Entities nicht — sonst
  ließe sich nicht mehr klären, warum der Wert fehlt.
- Neuer Zähler **„Gewicht verworfen"** (ohne `state_class`, ohne
  `restore_value`), sonst wäre das Verwerfen von einer ruhigen Waage nicht zu
  unterscheiden.
- Neues Global `erste_messung_erfolgt` ersetzt im Taktgeber die Prüfung
  `isnan(waage_gewicht.state)`. Ohne diesen Ersatz bliebe der Taktgeber bei
  dauerhaft unplausiblen Werten für immer im Anlauf und schlösse **jede
  Minute** ein Messfenster ab.
- Beide neuen Globals haben `restore_value: no` und sollten die Kalibrierung
  daher nicht kosten — **trotzdem nach dem Flash prüfen.**
- **Die Altlast in der Statistik ist am 12.08.2026 bereinigt worden**: zwölf
  belastete Stundenzeilen, neun aus den Rohdaten neu gerechnet, drei (03.08.,
  Rohdaten weg) ersatzlos gestrichen. Weg: `recorder/clear_statistics` +
  `recorder/import_statistics` über die ws-Schnittstelle, alle sauberen Zeilen
  unverändert zurückgeschrieben. **HA mittelt je 5-Minuten-Topf zeitgewichtet
  und nimmt das arithmetische Mittel der Töpfe** — wer Statistikzeilen
  nachrechnet, muss das treffen, sonst schreibt er still falsche Geschichte.
  Einzelne Statistikzeilen löschen kann HA nicht.
- Preis der Untergrenze 0: eine frisch tarierte Waage kann nicht mehr −0,1 kg
  anzeigen. Simuliert in `tests/` (Punkt 11): 0,01 % der Werte bei einem Stock
  auf 0,0 kg, ab 0,1 kg Last keine. Für eine Driftmessung mit leerer Waage
  `plausibel_kg_min: "-5"` setzen.

### Kritische Plattform-Fallstricke (ESP8266 / ESPHome)
- **`restore_from_flash: true` ist ZWINGEND.** Ohne diese Zeile landen alle
  `restore_value`-Globals nur im RTC-RAM → gehen bei jedem Stromausfall verloren,
  Kalibrierung fällt auf Platzhalter zurück. War Ursache eines realen Fehlerbilds.
- **Aber es schuetzt NICHT gegen einen Flash, der neue `globals` hinzufuegt.**
  Zweimal real passiert: am 03.08. (Faktor fiel auf den Platzhalter 3.500) und
  am 10.08. beim Flash der Temperaturkompensation, die das Global
  `letzte_temperatur` mitbrachte. **Das ist die Regel, nicht die Ausnahme —
  ein Flash mit neuem Global kostet die Kalibrierung.** Nach jedem Flash den
  Kalibrierfaktor pruefen und im Zweifel BEIDE Kalibrierschritte fahren; nur den
  Referenzpunkt zu setzen ergibt einen etwa halbierten Faktor bei plausibel
  aussehender Anzeige. Erkennungszeichen dafuer: "Kalibriert bei" ist leer.
  Seit der Kompensation haengt daran mehr als frueher — ohne "Kalibriert bei"
  fehlt der Bezugspunkt und die Kompensation schaltet sich stumm ab.
- **Warum das so ist (11.08.2026 im Quelltext nachgesehen,
  `esphome/components/esp8266/preferences.cpp`, 2026.6.5):** Der Speicherplatz
  eines `restore_value`-Globals ergibt sich aus der **Reihenfolge** der
  `make_preference()`-Aufrufe, nicht aus seinem Namen — der Namens-Hash geht nur
  in die CRC ein. Ein neues Global mit `restore_value: yes` verschiebt deshalb
  alles Nachfolgende, die CRC schlägt fehl und die Werte fallen auf
  `initial_value` zurück. Ein Global mit `restore_value: no` fordert gar keinen
  Speicher an und verschiebt nichts. **Konsequenz für neue Änderungen:** neue
  Globals nach Möglichkeit `restore_value: no` geben. Die neun Globals der
  Messfenster-Mittelung sind alle so angelegt — dieser Flash sollte die
  Kalibrierung also nicht kosten. Abgeleitet aus dem Quelltext, nicht am Gerät
  getestet: **trotzdem nach dem Flash prüfen.**
- **GPIO16 (D0) ist der einzige Deep-Sleep-Weckpin** des ESP8266 (RTC-Timer zieht
  ihn auf Masse, muss deshalb an RST liegen). DOUT lag ursprünglich dort und hat
  Batteriebetrieb blockiert; **inzwischen auf D6 (GPIO12) umgezogen**, GPIO16 ist
  frei. GPIO16 kann außerdem keine Interrupts — für den HX711 war das egal, weil
  der Treiber aktiv pollt.
- **D4 (GPIO2) gemieden** — Boot-Strapping-Pin.

### Auflösung
- 0,1 kg, Rundung als `round(kg*10)/10` (nicht `/0.1*0.1`, da 0,1 binär nicht exakt
  darstellbar ist).
- Elektrisch unkritisch (0,1 kg ≈ 1.800 counts vs. ~21 counts HX711-Rauschen).
- **Begrenzend ist die Mechanik**, nicht der ADC: Eckenfehler ohne getrimmte Junction-Box
  0,5–2 % (250 g – 1 kg bei 50 kg), Kriechen. Die Temperaturdrift war der größte
  Posten und wird seit dem 10.08.2026 herausgerechnet.

### Temperatur — wird kompensiert (seit 10.08.2026)
- **+32,5 g/K**, gemessen über 6,8 Tage und 9,4 K (164 Punkte). Herleitung:
  [`docs/sessionbericht-2026-08-10.md`](docs/sessionbericht-2026-08-10.md).
- Gerechnet wird `kg -= (Temperatur − calib_temp) × Koeffizient`, danach erst Tara.
  `calib_temp` ist die bei der Nullpunkt-Kalibrierung festgehaltene Temperatur
  ("Kalibriert bei"), dort ist die Korrektur null. Fehlt sie, wird **nicht**
  kompensiert.
- Die Formel steht **einmal**, in `packages/waage-temperatur.h` (über
  `esphome: includes:`), weil sie an drei Stellen gebraucht wird: Gewicht, Tara,
  Diagnose-Entity. Pfad dort muss `packages/…` lauten — er löst gegen das
  Verzeichnis der geflashten Stock-Datei auf.
- **Tara zieht die Korrektur mit ab.** Sonst wandert die Drift nach dem Tarieren
  wieder ins Gewicht zurück. Klassischer Fehler, fällt beim Ausprobieren nicht auf.
- Koeffizient ist eine **Number-Entity** (g/K, `0` = aus), nicht fest im YAML:
  er ist eine Messgröße und muss am Zielstandort neu bestimmt werden — und jeder
  Flash kann die Kalibrierung mitnehmen.
- **Der Rohwert bleibt unkompensiert**, sonst wäre er für eine Nachmessung wertlos.
  Seit dem 11.08.2026 ist er ein Intervallmittel — für die Nachmessung ein Gewinn,
  weil "Rohwert" und "Temperatur Mittel" denselben Zeitraum abdecken.
- Neue Stöcke starten mit `0`. Der Wert von waage-eg gilt nur für dessen Aufbau.
- Nicht kompensiert: Kriechen (−10,9 g/Tag, abklingend) und Gradienten zwischen den
  vier Zellen (kann ein einzelner Sensor prinzipiell nicht).
- Prüfprogramm ohne Hardware: `tests/waage-temperatur-test.cpp` (`g++`, 4045 Prüfungen).
  Bindet die echten Header-Dateien ein, prüft also den ausgelieferten Code. Wer die
  Lambdas in `waage-basis.yaml` ändert, muss den Nachbau dort mitziehen — seit dem
  11.08. gilt das auch für die Filterkette und das Messfenster (Punkt 10).
- **Reserve, bewusst nicht eingebaut:** Glättet man die Temperatur vor der
  Verrechnung mit τ ≈ 90 min, fällt die Reststreuung von 15 g auf 8 g. Nicht
  gemacht, weil die Firmware mit dem *momentanen* Sensorwert rechnet — dazu
  gehören die 32,5 g/K, nicht die 34,7 aus dem gefilterten Fit — und weil τ an
  der thermischen Masse des Aufbaus hängt und im Garten anders ausfällt.
- **Belastbarkeit:** ±0,7 g/K ist nur der statistische Fehler. Teilmengen der
  Messreihe lagen zwischen 28 und 35 g/K, Einzeltage zwischen 23 und 35.
  Realistisch ist ±3 g/K. Über 20 K Tagesgang also ±60 g Restfehler statt 650 g
  unkompensiert.

### Durchsichtmodus
- Hardware-Taster am Stock sperrt die **Veroeffentlichung** (nicht die Messung)
  fuer eine einstellbare Zeit, Voreinstellung 60 min. LED zeigt den Zustand,
  blinkt in den letzten 5 Minuten. Zusaetzlich als Schalter in HA.
- Der HX711 laeuft bewusst weiter, damit die Filterkette eingeschwungen bleibt.
- `durchsicht_restminuten` ist **nicht** `restore_value`: ein Neustart beendet
  den Modus. Ein unbemerkt haengender Durchsichtmodus waere schlimmer.
- **2 min Nachlauf** nach dem Ende, damit das 60-s-Mittelungsfenster die
  Manipulation ausspuelt, danach sofort ein Wert (nicht erst nach dem Intervall).
- **HA-Seite erledigt (10.08.2026):** Nach der Durchsicht springt das Gewicht in
  einem Schritt, der 20-min-Ableitungshelfer liest das als Absturz.
  `Bienen: Schwarm-Alarm` hat jetzt drei Sperren (je 30 min, laenger als das
  Ableitungsfenster): Durchsichtmodus off `for: 00:30:00`, Betriebszeit > 1800,
  und ein Template auf `last_changed` von Kalibrierfaktor/Kalibriert bei/den
  drei Buttons. Details im README, Abschnitt "Durchsichtmodus".

### Namensgebung
- Entity-Namen **ohne** `"Waage eG "`-Präfix im YAML, weil ESPHome den `friendly_name`
  ohnehin voranstellt (sonst "Waage eG Waage eG Gewicht" in HA).

### HA-Seite (Sensoren/Logik)
- **Zwei getrennte Derivative-Fenster:** 6 h → kg/d (Trend) und 20 min → kg/h (Schwarm).
  Ein einzelner Helfer kann nicht beides leisten, weil der geglättete Sensor kurzfristige
  Schwarm-Sprünge (1,5–3 kg in Minuten) wegbügelt.
- **Tagesbilanz als `statistics`-Helfer** (`state_characteristic: change`, `max_age: 24h`)
  statt `input_number` + Automation — vergleicht automatisch "jetzt gegen dieselbe
  Uhrzeit gestern".
- **Futterkontrolle als Template-Binary-Sensor statt `threshold`-Helfer** (bewusste
  Ausnahme von der Regel "nimm den nativen Helfer"): `threshold` akzeptiert nur feste
  Zahlen, keine Entity-Referenz für die Grenze. Hysterese (0,5 kg) wird im Template über
  `this.state` selbst nachgebaut.
- **Futtervorrat-Anzeige getrennt von Kritisch-Erkennung:** `sensor.futtervorrat`
  (Gewicht minus Leergewicht Beute) ist nur die Anzeige; die Kritisch-Prüfung selbst
  vergleicht direkt das Rohgewicht gegen `input_number.mindestgewicht_mit_futter`
  (beide auf derselben Basis: Gesamtgewicht inkl. Beute).
- **`Bienen: Schwarm-Alarm` hat drei Sperren** (seit 10.08.2026), zusätzlich zum
  Zeitfenster 9–18 Uhr. Alle drei sperren **30 min** — länger als das
  20-min-Ableitungsfenster, sonst steckt der Sprung beim Freigeben noch darin:

  | Sperre | Bedingung | Fängt ab |
  |---|---|---|
  | Durchsicht | `switch.waage_eg_durchsichtmodus` = off `for: 00:30:00` | Durchsicht, Honigernte |
  | Neustart | `sensor.waage_eg_betriebszeit` above 1800 | Neustart, vor allem der Flash mit Kalibrierungsverlust |
  | Kalibrierung | Template auf `last_changed` | Kalibrieren, Tarieren |

  **Grund für die Erweiterung:** Der ursprünglich geplante Durchsicht-Filter
  allein hätte den realen Fehlalarm vom 10.08. um 15:04 **nicht** verhindert —
  der Durchsichtmodus war aus, Auslöser war eine Neukalibrierung (−3,3 kg/h bei
  Schwelle −3). Die Kalibrier-Sperre prüft deshalb `last_changed` von
  `Kalibrierfaktor`, `Kalibriert bei` und den drei Buttons. Für
  "hat sich lange nicht geändert" gibt es keine native HA-Bedingung: `state` mit
  `for:` braucht einen festen Zielzustand, der Zustand eines Buttons *ist* aber
  der Zeitstempel des letzten Drucks. Der Best-Practice-Prüfer des MCP-Servers
  meldet das Template deshalb an — begründet abgewiesen, Begründung als `note:`
  in der Automation. Verbleibende Lücke: ein Tara über die ESPHome-Weboberfläche.
- Dieselbe Sperre fehlt noch bei `Bienen: Futtervorrat kritisch` (weniger
  dringend: Schwellwert löst erst nach 2 h Überschreitung aus).

---

## 4. HA-Werkzeug-Fallstricke (für die Arbeit mit ha-mcp / Home Assistant)

- `ha_config_set_helper(action="update")` validiert gegen eine falsche Entity-Liste →
  Helfer **löschen und neu anlegen**, `create` funktioniert zuverlässig.
- Dashboard: `grid_options.columns: "full"` = volle Breite **der Sektion**, nicht der
  Ansicht → für volle Breite `column_span` auf der Sektion setzen.
- Tile-Feature `numeric-input` kennt nur `style: buttons|slider`. Für **Tippfelder**
  eine `entities`-Karte verwenden (rendert Number-Entities gemäß `mode: box`).
- Screenshot-Add-on **`0f1cc410_puppet`** (balloob) ist das echte; `81f33d0f_puppet` ist
  eine Test-Attrappe mit synthetischen PNGs.
- Template-Helfer per ha-mcp: `next_step_id` (Sub-Typ) und die eigentlichen Felder
  können in **einem** Aufruf übergeben werden.
- Gated Write-Tools verlangen im strict-BPS-Modus ein `BestPracticeKey`
  (Attestation-Phrase aus dem Skill-Inhalt, rotiert stündlich) — vorher
  `ha_get_skill_guide` lesen und Key entnehmen. Mit `MandatoryBPS: false` entfällt das
  für den Rest der Session.
- Dashboard-Edits: `ha_config_get_dashboard` liefert `config_hash`, dann
  `ha_config_set_dashboard(python_transform=..., config_hash=...)` statt vollem
  Config-Replace verwenden.
- `hx711.tare` existiert **nicht** als ESPHome-Action — Tara wird über einen
  globals-basierten Lambda-Workaround gelöst.
- **ESPHome-Packages (seit 04.08.):** Das Device Builder Add-on listet nur
  YAML-Dateien direkt in `/config/esphome/` als Geräte — Stock-Dateien gehören
  deshalb in den Root, die Basis nach `packages/`. `!secret` in einem Package
  löst gegen das Verzeichnis der geflashten Stock-Datei auf, `secrets.yaml`
  bleibt also im Root; `esphome config packages/waage-basis.yaml` direkt
  aufzurufen schlägt deshalb fehl (erwartet). Werte aus der Stock-Datei haben
  Vorrang vor denen aus dem Package — so bekommt ein einzelner Stock bei Bedarf
  einen eigenen API-Key. Substitutions lassen sich **nicht** in `!secret`
  hineinschreiben.
- **ESPHome lokal zum Prüfen installieren** (`pip install esphome`):
  `esphome config <datei>.yaml` löst Packages, Substitutions und Secrets
  vollständig auf, ohne Hardware und ohne Compiler-Lauf. Damit lässt sich eine
  Änderung an der Basis vor dem Flashen gegen jeden Stock absichern —
  `esphome config` vorher/nachher in Dateien schreiben und `diff`en. Genau so
  wurde die Package-Umstellung als verhaltensgleich nachgewiesen. Secret-Werte
  sind in der Ausgabe ANSI-maskiert, bei Bedarf `--show-secrets`.
- ESPHome Device Builder Add-on (Slug `5c53de3b_esphome`, Port `6052`) braucht für
  Dashboard-API-Zugriff zwei nicht offensichtliche Einstellungen: Port `6052/tcp` auf
  Host-Port `6052` gemappt, und `leave_front_door_open: true` — ohne beides: HTTP 403.
- WebSocket-Log-Streaming über `ha_manage_addon` bricht nach ~2,5 s ab (kein Gerätefehler)
  — für aktuelle Sensorwerte stattdessen `ha_search` mit Gerätename nutzen.
- `ha_get_device(integration: esphome)` liefert leer, wenn Geräte über MQTT statt der
  ESPHome-HA-Integration verbunden sind → dann `integration: mqtt` verwenden.
- **Buttons, die über die ESPHome-Weboberfläche (Port 80) gedrückt werden,
  erreichen HA nicht.** Das Gerät führt `on_press` aus und veröffentlicht, aber
  die Button-Entity in HA behält ihren alten Zeitstempel. Am 10.08. zunächst als
  rätselhafte "Messung außerhalb des Takts" aufgefallen. Zwei Konsequenzen: bei
  der Fehlersuche nie aus einer unveränderten Button-Entity schließen, dass
  nichts gedrückt wurde — und Automationen, die auf Kalibrierung reagieren
  sollen, an die Diagnose-Sensoren hängen (`Kalibrierfaktor`, `Kalibriert bei`),
  nicht an die Buttons. Die Sensoren ändern sich unabhängig davon, woher der
  Druck kam.
- **`esphome: includes:` prüft den Pfad schon bei `esphome config`.** Steht dort
  `waage-temperatur.h` statt `packages/waage-temperatur.h`, bricht die
  Validierung mit Exit-Code 2 ab. Der Pfad löst gegen das Verzeichnis der
  geflashten Stock-Datei auf — genauso wie `!secret` und das `!include` des
  Packages.
- **Messdaten für eine Driftanalyse aus dem Recorder holen:** Rohwert als
  Zustandsverlauf (`ha_get_history`, source `history`), Temperatur als
  Stundenmittel (source `statistics`, period `hour`) — für die Temperatur reicht
  das Stundenmittel, der Zeitversatz von einer halben Stunde kostet bei einem
  24-h-Tagesgang nur 0,1 %. **Wiederholungen filtern:** nach jedem `unavailable`
  veröffentlicht HA denselben Wert erneut mit neuem Zeitstempel; ungefiltert
  waren das im 7-Tage-Datensatz 20 Scheinmesspunkte, alle rund um Neustarts
  gehäuft. Doppelte Werte verwerfen.
- `ha_eval_template` eignet sich, um eine Template-Bedingung **vor** dem
  Schreiben gegen die echten Entities zu testen — inklusive der Frage, was bei
  einer fehlenden Entity passiert.

---

## 5. Code-Struktur des Repos

```
Bienenstockwaage/
├── CLAUDE.md                          # Kurzanleitung fuer Claude Code, wird
│                                       #   automatisch geladen. Zeigt hierher.
├── packages/waage-basis.yaml          # GESAMTE gemeinsame Logik - die eigentliche
│                                       #   Codedatei. Wird nicht direkt geflasht.
├── packages/waage-temperatur.h        # Formel der Temperaturkompensation, einmal
│                                       #   statt dreimal (esphome: includes:)
├── packages/waage-mittelwert.h        # Mittelwert + Streuung des Messfensters,
│                                       #   ebenfalls per esphome: includes:
├── packages/waage-grenzen.h           # Plausibilitaetsfenster: welches Gewicht
│                                       #   ueberhaupt nach HA darf
├── tests/waage-temperatur-test.cpp    # Prueft beide Header mit g++, ohne Hardware
├── waage-eg.yaml                      # Stock 1: nur substitutions + package-Include
├── waage-stock2.yaml                  # Stock 2, dito
├── waage-stock3.yaml                  # Stock 3, dito
├── secrets.yaml.example               # Vorlage: wifi_ssid, wifi_password,
│                                       #   ap_fallback_password, api_encryption_key, ota_password
├── waage-eg-notes.md                  # Entscheidungen, Begründungen, HA-Seite
├── docs/waegezellen-verkabelung.md    # Abschnitt 0 = Halbbrücken-Ring (relevant),
│                                       #   Abschnitte 1-6 = Vollbrücken-Referenz
├── docs/sessionbericht-*.md           # Was wann warum geaendert wurde
├── README.md                          # Inbetriebnahme, Kalibrieren, Fehlersuche
└── .gitignore                         # secrets.yaml, .esphome/, *.bin
```

**Aufteilung Basis/Stock (seit 04.08.2026):** Alles Gemeinsame liegt in
`packages/waage-basis.yaml`, alles Stock-Spezifische in `substitutions` der
jeweiligen Stock-Datei (`geraete_name`, `anzeige_name`, `ap_ssid`, die fünf
Pins, die Kalibrier-Startwerte, die Startwerte der Einstell-Entities). Eine
Änderung an der Basis wirkt nach dem nächsten Flash auf alle Stöcke.

Zwei Dinge, die dabei leicht schiefgehen:

- **Die Stock-Dateien müssen im Root bleiben.** Das ESPHome Device Builder
  Add-on listet nur YAML-Dateien direkt in `/config/esphome/` als Geräte.
  `packages/` liegt bewusst darunter, damit die Basis nicht als nicht
  flashbares Pseudo-Gerät im Dashboard auftaucht. Beim Kopieren ins
  Add-on-Verzeichnis muss `packages/` mit.
- **`geraete_name` + `anzeige_name` sind die entity_id in HA.** Für
  `waage-eg` unverändert gelassen (`waage-eg` / `Waage eG`), sonst legt HA
  neue Entities an und Helfer/Automationen/Dashboard zeigen ins Leere.
  Bei neuen Stöcken vor dem ersten Flash festlegen — danach ist Umbenennen
  teuer. Empfehlung: Standortnamen statt Durchnummerierung (Begründung im
  README, Abschnitt "Mehrere Stöcke").

Nachweis der Verhaltensgleichheit: `esphome config waage-eg.yaml` vor und
nach der Umstellung liefert dieselbe aufgelöste Konfiguration, bis auf den
zusätzlichen `substitutions:`-Block.

**Nicht im Repo (lebt nur in HA):** Helfer, Automationen, Dashboard `bienen-stockwaage`.

**Wo der eigentliche Code steht:** Diese Datei fasst nur Entscheidungen zusammen.
Die vollständige Logik (Globals, HX711 samt Filterkette, Kalibrier-Buttons,
Wägezellen-Template, Durchsichtmodus, Taktgeber) steht in
`packages/waage-basis.yaml` — 800+ Zeilen mit ausführlichen Kommentaren, die das
Warum jeweils an Ort und Stelle erklären. Die Tabellen der HA-Helfer und
-Automationen stehen in `waage-eg-notes.md`, Abschnitt "Home-Assistant-Seite".

*(Frühere Fassungen dieser Datei verwiesen auf `claude_waage-eg-kontext.md` und
`Leergewicht__mindestgewicht_mit_Futter` als Referenzdateien. Die gibt es im Repo
nicht — der Inhalt ist längst in die oben genannten Dateien eingeflossen.)*

**Prüfen ohne Hardware:**

```bash
esphome config waage-eg.yaml            # loest Packages/Substitutions/Secrets auf
cd tests && g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

---

## 6. Wägezellen-Verkabelung (Halbbrücken → Vollbrücke)

Äußere Adern der vier Zellen zu einem **Ring** verbinden (Z1→Z2→Z3→Z4→Z1). Die vier
**mittleren** Adern sind die Brückenecken und gehen alternierend an den HX711:

| Zelle | mittlere Ader an |
|---|---|
| 1 | E+ |
| 2 | A+ |
| 3 | E− |
| 4 | A− |

Gegenprobe mit Ohmmeter: E+/E− und A+/A− müssen etwa gleich sein (~1 kΩ).

---

## 7. Offene Punkte / nächste Schritte

**Mehrere Stöcke (Stand 04.08.):**
- YAML-Basis steht: `packages/waage-basis.yaml` plus eine Stock-Datei je Gerät,
  `waage-stock2` und `waage-stock3` sind angelegt und validiert. Hardware für
  die neuen Stöcke ist noch nicht aufgebaut.
- **Namen vor dem ersten Flash festlegen.** Empfehlung: Standort statt
  Durchnummerierung (`waage-garten` statt `waage-stock2`), weil
  `geraete_name`+`anzeige_name` die entity_id bilden und ein späteres
  Umbenennen die HA-Historie kostet. Solange nicht geflasht wurde, sind es
  drei Zeilen.
- **HA-Seite für die neuen Stöcke fehlt komplett** — Helfer, Automationen,
  Dashboard. Bewusst als eigener Schritt vertagt. Die bestehenden hängen an
  `waage-eg`-Entities und müssen pro Stock dupliziert oder auf eine
  Blueprint-/Template-Lösung umgestellt werden.
- Details: [`docs/sessionbericht-2026-08-04.md`](docs/sessionbericht-2026-08-04.md)

**Sofort anzupassen (Platzhalter):**
- ~~**NEU KALIBRIEREN, beide Schritte.**~~ **Zweimal erledigt** — am 03.08. um
  16:36/16:41 (Faktor −20.845, 24,6 °C) und erneut am 10.08. um 15:01/15:03,
  nachdem der Flash der Temperaturkompensation die Kalibrierung wieder gekostet
  hatte. **Aktuell gültig: −20.874 counts/kg, "Kalibriert bei" 25,8 °C,
  "Kalibriert mit" 2,218 kg.** Der Ablauf des ersten Verlusts (Faktor auf dem
  Platzhalter 3.500, dann eine halbe Neukalibrierung mit −8.770) steht in
  [`docs/sessionbericht-2026-08-03.md`](docs/sessionbericht-2026-08-03.md).
  **Nach jedem Flash den Kalibrierfaktor prüfen** — das bleibt dauerhaft gültig,
  siehe den Fallstrick zu `restore_from_flash` in Abschnitt 3.
- ~~**Schwarm-Alarm gegen den Durchsichtmodus sperren**~~ — **erledigt am
  10.08.2026**, und gleich gegen Neustart und Kalibrierung mit. Siehe Abschnitt
  "Durchsichtmodus". Offen bleibt dasselbe fuer `Bienen: Futtervorrat kritisch`
  (weniger dringend, Schwellwert loest erst nach 2 h aus).
- `input_number.leergewicht_beute` und `input_number.mindestgewicht_mit_futter` stehen
  beide noch auf 0,0 kg. Reale Werte eintragen (Beute leer wiegen; Mindestgewicht =
  Beute + Bienen + Mindestfutterreserve). Bis dahin liefert `binary_sensor.futtervorrat_kritisch`
  fälschlich "nicht kritisch".
- Schwarm-Schwellwert von −3 kg/h nach der ersten echten Saison nachjustieren.

**Temperaturdrift — erledigt, aber am Zielstandort zu wiederholen:**
- Ergebnis: +32,5 g/K, Hysterese ±1,4 g (also keine dominierenden Eckengradienten),
  Kompensation eingebaut und aktiv.
- **Nach dem Umzug in den Garten neu bestimmen.** Vorgehen: konstante Last auflegen,
  Messintervall auf 15–60 min, `Bienen: Messintervall nach Saison` vorübergehend
  ausschalten, mindestens fünf Tage laufen lassen, dann Rohwert gegen
  **"Temperatur Mittel"** fitten (nicht gegen "Temperatur" — seit dem 11.08. ist der
  Rohwert ein Intervallmittel und passt zeitlich nur zum Temperaturmittel) — **mit einem Zeitglied als zweitem Term**, sonst schiebt das Kriechen den
  Koeffizienten nach oben. Einzeltage taugen nicht (streuten zwischen 23 und 35 g/K).

**Deep Sleep / Batteriebetrieb (Zielaufbau: Garten, Solar, NiMH 2600 mAh):**
- Der Pin-Blocker ist beseitigt: DOUT liegt jetzt auf D6 (GPIO12), GPIO16 bleibt
  für den Weckpfad frei. **Gilt erst nach dem Umstecken der Verdrahtung** — YAML
  und Hardware gehören zusammen geflasht.
- Noch offen: `D0`↔`RST` überbrücken (470 Ω), Pull-up 10 kΩ auf SCK, Power-Down
  des HX711 (der Treiber kann das nicht von sich aus), kürzere Filterkette,
  Wachhalten-Schalter für OTA. Alles ausgearbeitet und mit `esphome config`
  geprüft in [`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md).
- Der entscheidende Posten ist nicht der ESP: HX711 und Wägezellenbrücke ziehen
  ~5,8 mA dauerhaft, der schlafende ESP nur ~0,02 mA.

**Erwägenswert (nicht entschieden):**
- Junction-Box mit Trimmpotis für Eckenabgleich (größter Einzelfehler).
- Alternative Topologie: 4× HX711 mit gemeinsamem CLK und Software-Summe statt
  Parallelschaltung.
- WLAN-Pegel −70 dBm beobachten, ggf. Antennenposition prüfen.

**Erledigt, hier nur noch als Antwort auf wiederkehrende Fragen:**
- **`secrets.yaml`-Nutzung** (war lange offen, seit 04.08. geklärt): Die Datei
  bleibt neben den Stock-Dateien im Root (`/config/esphome/secrets.yaml`), nicht
  in `packages/` — `!secret` in einem Package löst gegen das Verzeichnis der
  geflashten Datei auf. Secret-Namen lassen sich **nicht** über Substitutions
  parametrisieren (`!secret ${x}` geht nicht); deshalb teilen sich alle Stöcke
  standardmäßig WLAN, API-Key und OTA-Passwort. Wer pro Gerät trennen will,
  überschreibt den `api:`-Block in der Stock-Datei — in `waage-stock2/3.yaml`
  ist das auskommentiert vorbereitet.

---

## 8. Einstiegs-Prompt für Claude Code

**Seit dem 10.08.2026 liegt eine [`CLAUDE.md`](CLAUDE.md) im Root.** Claude Code
lädt sie bei jeder Session automatisch — Prüfbefehle, Architektur, harte Regeln
und ein Verweis auf diese Datei hier sind damit immer im Kontext. Für die
meisten Aufgaben reicht es also, einfach loszulegen.

Der Block unten bleibt trotzdem nützlich: für andere Werkzeuge ohne
CLAUDE.md-Unterstützung, und wenn du diese Datei ausdrücklich vollständig
gelesen haben willst (die CLAUDE.md verweist nur darauf, sie enthält sie nicht).

```
Ich arbeite an einer ESPHome-Bienenstockwaage (ESP8266 D1 Mini + HX711), die bereits
produktiv läuft und in Home Assistant eingebunden ist. Im Repo-Root liegt die Datei
waage-eg-claude-code-kontext.md mit der vollständigen Zusammenfassung aller bisherigen
Entscheidungen, Fallstricke und offenen Punkte — bitte lies sie zuerst komplett.
Die Sessionberichte in docs/ sind die Chronologie dazu; wenn dir eine Entscheidung
unbegründet vorkommt, steht das Warum meist dort.

Prüfen lässt sich alles ohne Hardware: "pip install esphome" und
"esphome config waage-eg.yaml", plus das g++-Testprogramm in tests/.

Aktuell will ich als Nächstes: [HIER EINTRAGEN, z. B. "den Schwarm-Alarm gegen den
Durchsichtmodus sperren" oder "input_number.leergewicht_beute korrekt setzen" oder
"die neuen Stöcke benennen und die HA-Seite dafür anlegen"].

Bitte halte dich an die dort dokumentierten Konventionen (deutsche Sprache in Code/
Doku, keine calibrate_linear-Kalibrierung, restore_from_flash: true nicht entfernen,
GPIO16/D0 bleibt für den Deep-Sleep-Weckpfad frei).
```

Trag im Platzhalter einfach ein, womit du als Nächstes weitermachen willst.
