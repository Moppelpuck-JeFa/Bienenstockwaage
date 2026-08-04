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

**Repo:** `github.com/Moppelpuck-JeFa/Bienenstockwaage` (privat), Branch `main`.

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

**Gemessene Istwerte:**
- Kalibrierfaktor: **−20.840 counts/kg** (negativ = invertierte Signalpolarität, funktional unkritisch)
- Kalibriert mit 2,218 kg Referenzgewicht bei 21,5 °C
- Rohwert leer: 25.830 counts → ~470 kg rechnerischer ADC-Vorrat (mechanisches Limit von 200 kg greift vorher)
- WLAN-Signal: −70 dBm (Grenzbereich, ggf. beobachten)

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
- Minutenzähler statt `millis()`, weil `millis()` nach ~49 Tagen überläuft.

### Kritische Plattform-Fallstricke (ESP8266 / ESPHome)
- **`restore_from_flash: true` ist ZWINGEND.** Ohne diese Zeile landen alle
  `restore_value`-Globals nur im RTC-RAM → gehen bei jedem Stromausfall verloren,
  Kalibrierung fällt auf Platzhalter zurück. War Ursache eines realen Fehlerbilds.
- **Aber es schuetzt NICHT gegen einen Flash, der neue `globals` hinzufuegt.**
  Am 03.08. real passiert: Faktor fiel unmittelbar nach dem Flash auf den
  Platzhalter 3.500. **Nach jedem Flash den Kalibrierfaktor pruefen** und im
  Zweifel BEIDE Kalibrierschritte fahren — nur den Referenzpunkt zu setzen
  ergibt einen etwa halbierten Faktor bei plausibel aussehender Anzeige.
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
  0,5–2 % (250 g – 1 kg bei 50 kg), Temperaturdrift, Kriechen.

### Temperatur
- DS18B20 **misst**, es wird **nicht kompensiert** — Koeffizient ist unbekannt, ein
  geratener Koeffizient würde die Messung unbemerkt verschlechtern.
- `calib_temp` wird bei Nullpunkt-Kalibrierung mitgespeichert (nicht nachträglich
  rekonstruierbar).

### Durchsichtmodus
- Hardware-Taster am Stock sperrt die **Veroeffentlichung** (nicht die Messung)
  fuer eine einstellbare Zeit, Voreinstellung 60 min. LED zeigt den Zustand,
  blinkt in den letzten 5 Minuten. Zusaetzlich als Schalter in HA.
- Der HX711 laeuft bewusst weiter, damit die Filterkette eingeschwungen bleibt.
- `durchsicht_restminuten` ist **nicht** `restore_value`: ein Neustart beendet
  den Modus. Ein unbemerkt haengender Durchsichtmodus waere schlimmer.
- **2 min Nachlauf** nach dem Ende, damit das 60-s-Mittelungsfenster die
  Manipulation ausspuelt, danach sofort ein Wert (nicht erst nach dem Intervall).
- **Offener Punkt auf der HA-Seite:** Nach der Durchsicht springt das Gewicht in
  einem Schritt. Der 20-min-Ableitungshelfer liest das als Absturz →
  `Bienen: Schwarm-Alarm` loest faelschlich aus. Die Automation braucht
  `switch.waage_eg_durchsichtmodus` == off `for: 00:30:00` als Bedingung
  (laenger als das Ableitungsfenster). Noch nicht eingebaut.

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

---

## 5. Code-Struktur des Repos

```
Bienenstockwaage/
├── packages/waage-basis.yaml          # GESAMTE gemeinsame Logik - die eigentliche
│                                       #   Codedatei. Wird nicht direkt geflasht.
├── waage-eg.yaml                      # Stock 1: nur substitutions + package-Include
├── waage-stock2.yaml                  # Stock 2, dito
├── waage-stock3.yaml                  # Stock 3, dito
├── secrets.yaml.example               # Vorlage: wifi_ssid, wifi_password,
│                                       #   ap_fallback_password, api_encryption_key, ota_password
├── waage-eg-notes.md                  # Entscheidungen, Begründungen, HA-Seite
├── docs/waegezellen-verkabelung.md    # Abschnitt 0 = Halbbrücken-Ring (relevant),
│                                       #   Abschnitte 1-6 = Vollbrücken-Referenz
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

Die vollständigen YAML-Codefragmente (Globals, HX711-Sensor, Kalibrier-Button, Taktgeber)
sowie die Tabellen der HA-Helfer und -Automationen liegen in den Originaldateien
`claude_waage-eg-kontext.md` bzw. `Leergewicht__mindestgewicht_mit_Futter` — diese
sollten 1:1 als Referenz mit ins neue Arbeitsverzeichnis übernommen werden, da hier nur
die Entscheidungen zusammengefasst sind, nicht jede Codezeile.

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
- **NEU KALIBRIEREN, beide Schritte.** Beim Flash am 03.08. ging die
  Kalibrierung verloren (Faktor fiel auf den Platzhalter 3.500); die
  anschliessende Neukalibrierung setzte nur den Referenzpunkt, nicht den
  Nullpunkt. Der Faktor steht dadurch bei −8.770 statt −20.840 und die Waage
  misst falsch. Erkennungszeichen: "Kalibriert bei" ist leer.
  Details in [`docs/sessionbericht-2026-08-03.md`](docs/sessionbericht-2026-08-03.md).
- **Schwarm-Alarm gegen den Durchsichtmodus sperren** — sonst loest er nach
  jeder Durchsicht aus, bei der Gewicht abgenommen wurde. Bedingung siehe
  Abschnitt "Durchsichtmodus".
- `input_number.leergewicht_beute` und `input_number.mindestgewicht_mit_futter` stehen
  beide noch auf 0,0 kg. Reale Werte eintragen (Beute leer wiegen; Mindestgewicht =
  Beute + Bienen + Mindestfutterreserve). Bis dahin liefert `binary_sensor.futtervorrat_kritisch`
  fälschlich "nicht kritisch".
- Schwarm-Schwellwert von −3 kg/h nach der ersten echten Saison nachjustieren.

**Offene Messung:**
- Temperaturdrift bestimmen: konstante bekannte Last auflegen, Messintervall auf 15 min,
  einige Tage laufen lassen, dann Rohwert gegen Temperatur in der Dashboard-Ansicht
  "Auswertung" vergleichen.
  - Parallele Kurven → systematische Drift, Kompensation würde sich lohnen.
  - Breit streuende Punktwolke → Eckengradienten dominieren, ein einzelner Sensor kann
    das nicht korrigieren → dann auf Tagesbilanz verlassen.

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

**Aus früheren Sessions noch offen:**
- Frage zur `secrets.yaml`-Nutzung in ESPHome war zuletzt noch nicht abschließend geklärt.

---

## 8. Vorschlag: Einstiegs-Prompt für Claude Code

Kopiere diesen Block als ersten Prompt in Claude Code, nachdem du das Repo geklont hast:

```
Ich arbeite an einer ESPHome-Bienenstockwaage (ESP8266 D1 Mini + HX711), die bereits
produktiv läuft und in Home Assistant eingebunden ist. Im Repo-Root liegt die Datei
waage-eg-claude-code-kontext.md mit der vollständigen Zusammenfassung aller bisherigen
Entscheidungen, Fallstricke und offenen Punkte — bitte lies sie zuerst komplett.

Aktuell will ich als Nächstes: [HIER EINTRAGEN, z. B. "die Temperaturdrift-Messung
auswerten" oder "input_number.leergewicht_beute korrekt setzen" oder "die
secrets.yaml-Frage klären"].

Bitte halte dich an die dort dokumentierten Konventionen (deutsche Sprache in Code/
Doku, keine calibrate_linear-Kalibrierung, restore_from_flash: true nicht entfernen,
GPIO16/D0 bleibt für den Deep-Sleep-Weckpfad frei).
```

Trag im Platzhalter einfach ein, womit du als Nächstes weitermachen willst.
