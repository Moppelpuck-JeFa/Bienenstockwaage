# Projekt: ESPHome-Stockwaage "Waage eG"

## Kontext
Bienenstockwaage auf ESP8266-Basis (D1 Mini), gebaut mit ESPHome.
Steht im Erdgeschoss (Gerätename `waage-eg`). Läuft über das
ESPHome Device Builder Add-on in Home Assistant.

## Hardware
- **Chip:** ESP8266, Board `d1_mini`
- **Wägezellen-Setup:** 4 Wägezellen (statt einer einzelnen), parallel
  verschaltet - entweder direkt oder über eine externe Summier-/Junction-Box
  (z. B. die "HX711 Junction Box" der Hiveeyes-Community). Elektrisch kommt
  am ESP trotzdem nur **1 HX711-Signal** an, daher ändert sich am Code nichts.
  Details, Fallstricke und Alternativen: siehe
  [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md)
- **HX711-Pins am ESP:** `DOUT = D1`, `CLK = D2`
- **Empfohlene Wägezellen-Specs** (noch zu beschaffen):
  - Genauigkeitsklasse **C3 nach OIML R60**, Empfindlichkeit **2mV/V**
  - Vollbrücken-Zellen (4 Anschlüsse: E+/E-/A+/A-), nicht Halbbrücke
  - Kapazität z. B. 4× 50kg (Gesamt ~200kg Reserve)
  - **Eingangswiderstand möglichst 1000 Ω** - 4× 350 Ω parallel ergibt 87,5 Ω
    und überlastet die HX711-interne Brückenspeisung (siehe docs/)
  - Marken-Beispiele: Soehnle/HBM (SEB42, SP4M) für höhere Qualität;
    günstiger z. B. YZC-161-Typ (aber Halbbrücke, Temperaturdrift höher)

## Funktionale Anforderungen (final)
- Ausgabe in **kg**, gerundet auf **0,1 kg genau**
  (war ursprünglich 0,5 kg, auf Wunsch verfeinert - siehe Abschnitt
  "Auflösung 0,1 kg: was das realistisch bedeutet")
- **Kein festes `calibrate_linear`** im YAML - Kalibrierung läuft komplett
  dynamisch über zwei Buttons in Home Assistant:
  - **"Waage eG Kalibrieren 0kg"** - Waage leer stellen, dann drücken
  - **"Waage eG Kalibrieren Referenzgewicht"** - Prüfgewicht auflegen, dann
    drücken. Dessen Masse ist über die Number-Entity **"Waage eG
    Referenzgewicht"** frei einstellbar (0,1-50 kg, Voreinstellung 0,5 kg -
    war ursprünglich fest auf 0,5 kg verdrahtet)
- **"Waage eG Tara"** - separater Button, nullt nur das aktuell aufliegende
  Gewicht (z. B. Behälter), ändert NICHTS an der Kalibrierung selbst
- Messintervall **frei aus HA einstellbar** über die Number-Entity
  "Waage eG Messintervall" (1 min bis 10080 min = 7 Tage, Voreinstellung
  360 min = 6 h). Siehe Abschnitt "Sampling-Strategie" - der HX711 selbst
  wird davon unabhängig im Sekundentakt abgefragt.
- Rausch-Filterung: `median` + `sliding_window_moving_average` auf dem
  rohen HX711-Signal, bevor die Kalibrierrechnung angewendet wird
- `api.reboot_timeout: 0s` - verhindert unnötige Neustarts, da bei langem
  Messintervall zwischendurch mal kurz keine HA-Verbindung bestehen kann
- Alle Kalibrierwerte (`calib_raw_zero`, `calib_raw_half`) und der Tara-Offset
  sind `globals` mit `restore_value: yes` - übersteht Neustarts

## Sampling-Strategie (geändert - vorher fehlerhaft)

Ursprünglich stand `update_interval: 6h` direkt am HX711-Sensor. Das hatte zwei
Fehler, die erst bei der realen Kalibrierung aufgefallen wären:

1. **Der 6h-Takt kam nie zustande.** Der `median`-Filter hatte `send_every: 4`,
   gab also nur bei jeder 4. Messung einen Wert weiter → bei 6h-Rohintervall ein
   Wert **alle 24h**, und wegen `send_first_at: 3` der erste erst ~18h nach dem
   Boot. Bestätigt in `esphome/components/sensor/filter.cpp`:
   `SlidingWindowFilter` startet mit `send_at_ = send_every − send_first_at` und
   sendet erst, wenn `++send_at_ >= send_every`.
2. **Die Buttons schrieben den alten Wert.** `component.update: hx711_raw_counts`
   löst zwar eine Rohmessung aus, die läuft aber durch dieselbe Filterkette -
   in 3 von 4 Fällen kommt hinten nichts raus, `.state` ist nach den 200 ms
   unverändert. Alle drei Buttons hätten still den vorherigen Rohwert gespeichert.

**Jetzt:** Der HX711 wird mit `update_interval: 1s` abgefragt und gefiltert
(Median über 5 Werte → Ausgabe alle 5s, danach gleitender Mittelwert über 12
dieser Werte ≈ 60s Fenster). Der interne Rohwert ist damit **immer aktuell**
(max. 5s alt) und gut gemittelt. Die Taktung nach HA ist davon vollständig
entkoppelt (siehe "Einstellbares Messintervall"). Die Buttons brauchen dadurch
kein `component.update` + `delay` mehr - sie lesen einfach den gefilterten
Zustand.

**Bedienhinweis:** Weil ~60s gemittelt wird, gilt für alle drei Buttons:
erst auflegen/abräumen, **~1 Minute warten**, dann drücken.

Netzbetrieb vorausgesetzt kostet das schnellere Sampling nichts. Bei einem
späteren Umstieg auf Deep Sleep müsste diese Strategie neu gedacht werden.

## Auflösung 0,1 kg: was das realistisch bedeutet

Die Anzeige rundet auf 0,1 kg. Elektrisch ist das unkritisch, praktisch ist es
eine Aussage über die *Auflösung*, nicht über die *Genauigkeit* - der
Unterschied ist hier wichtig.

**Elektrisch: reichlich Reserve.** Bei 4 × 50 kg (200 kg Vollausschlag,
2 mV/V, ~4,3 V Speisung) entspricht 0,1 kg rund **4,3 µV** Brückensignal. Der
HX711 rauscht laut Datenblatt mit ~50 nV(rms) bei `gain: 128` und 10 SPS - das
sind grob **21 counts** pro Einzelmessung gegenüber ~1800 counts pro
0,1-kg-Schritt. Nach der 60s-Mittelung bleibt davon nochmal deutlich weniger
übrig. Der ADC ist also mit Abstand nicht der begrenzende Faktor; auch 0,01 kg
wäre elektrisch darstellbar.

**Praktisch begrenzen drei andere Dinge**, alle größer als 0,1 kg:

1. **Temperaturdrift.** C3-Zellen driften typisch in der Größenordnung
   0,02 %/10 K der Nennlast. Auf 200 kg sind das ~40 g pro 10 K. Draußen sind
   20-30 K Tagesgang normal → **80-120 g**, also rund ein Anzeigeschritt.
   Die Temperaturkompensation wurde auf Wunsch entfernt (siehe unten), damit
   bleibt dieser Anteil unkorrigiert.
2. **Eckenfehler** ohne getrimmte Junction-Box: 0,5-2 %, bei 50 kg Auflage
   also **250 g bis 1 kg**. Das ist der mit Abstand größte Posten - Details in
   `docs/waegezellen-verkabelung.md`.
3. **Kriechen (Creep)** nach Lastwechsel: bei C3 ~0,0166 % über 30 min, auf
   200 kg also **~33 g**.

**Einordnung:** Die 0,1 kg sind sinnvoll für die *Veränderung* über Stunden und
Tage - Trachteintrag, Futterverbrauch, Schwarmabgang. Genau darum geht es bei
einer Stockwaage. Der *Absolutwert* wird realistisch auf ±0,1 bis ±0,5 kg genau
sein, abhängig davon, ob die Junction-Box getrimmt ist. Die feinere Anzeige
schadet dabei nicht, man sollte die letzte Stelle nur nicht als absolute
Wahrheit lesen.

**Der wirksamste Hebel ist das Referenzgewicht - inzwischen umgesetzt.** Siehe
Abschnitt "Frei wählbares Referenzgewicht".

### Umsetzung im Code
- `accuracy_decimals: 1` (passt für 0,1)
- Rundung als `std::round(kg * 10.0f) / 10.0f` - bewusst nicht `/0.1 * 0.1`,
  weil 0,1 binär nicht exakt darstellbar ist
- Das frühere Deadband `if (kg > -0.05 && kg < 0.05) kg = 0.0;` ist **entfallen**
  - bei 0,1er-Rundung war es exakt deckungsgleich mit der Rundung selbst und
  damit wirkungslos. Geblieben ist nur die Normalisierung von `-0.0` auf `0.0`,
  damit HA keine negative Null anzeigt.
- Folge davon: eine leere Waage kann jetzt statt exakt 0,0 auch mal ±0,1
  anzeigen, wenn sie thermisch weggedriftet ist. Das ist ehrlicher als ein
  Deadband, das echte kleine Gewichte verschluckt.

## Frei wählbares Referenzgewicht (ersetzt die fest verdrahteten 0,5 kg)

Der Kalibrier-Button hieß "Kalibrieren 0,5kg" und die 0,5 stand als Literal in
drei Lambdas. Jetzt:

- **Number-Entity "Waage eG Referenzgewicht"**, kg, 0,1 bis 50, `step: 0.001`
  (also grammgenau eintragbar), Voreinstellung 0,5 → altes Verhalten ab Werk.
- Der Button heißt jetzt **"Waage eG Kalibrieren Referenzgewicht"**.
  ⚠️ Das ändert die entity_id in HA - unkritisch, weil das Gerät noch nicht
  in Betrieb ist.
- **Diagnose-Entity "Waage eG Kalibriert mit"** zeigt, mit welchem Gewicht
  zuletzt tatsächlich kalibriert wurde.

### Der entscheidende Punkt: zwei getrennte Werte

Die Number ist **reine Eingabe für die nächste Kalibrierung**. Die Anzeige
rechnet gegen das Global `calib_kg_ref`, das beim Druck auf den Kalibrier-Button
aus der Number übernommen wird.

Würde die Anzeige direkt gegen die Number rechnen, hätte ein späteres Ändern
der Zahl alle Messwerte still umskaliert, ohne dass je neu kalibriert wurde -
ein Faktor-20-Fehler wäre ein Vertipper. Genau dieser Fall ist getestet
(siehe Validierung). Die Diagnose-Entity macht die Unterscheidung nach außen
sichtbar, sonst wäre nicht erkennbar, welcher der beiden Werte gerade gilt.

### Warum schwerer besser ist

Der Span wird auf bis zu 200 kg hochgerechnet, bei 0,5 kg Referenz also mit
Faktor 400 - Fehler beim Setzen des Punkts skalieren genauso mit. Bei realistisch
~18000 counts/kg und einem Ablesefehler von 20 counts, hochgerechnet auf 100 kg:

| Referenzgewicht | Span | Fehler bei 100 kg |
|---|---|---|
| 0,5 kg | ~9.000 counts | ~222 g |
| 10 kg | ~180.000 counts | ~11 g |

Der zweite, in der Praxis oft größere Effekt: ein kleines 500-g-Gewicht liegt
auf einer Stockplatte fast zwangsläufig außermittig und trifft damit den
Eckenfehler voll. Deshalb steht im README ausdrücklich "möglichst mittig auflegen".

### Absicherungen im Button
- Referenzgewicht `NaN` oder `<= 0` → Abbruch mit Log-Warnung
  (`min_value: 0.1` verhindert das über HA schon, aber der Wert kommt auch aus
  `restore_value` zurück)
- Rohwert identisch zum Nullpunkt → Abbruch (Gewicht vergessen aufzulegen)
- In beiden Fällen bleiben `calib_raw_ref` und `calib_kg_ref` unangetastet,
  eine funktionierende Kalibrierung wird also nie durch einen Fehlgriff zerstört
- `on_value` der Number loggt explizit, dass die Änderung erst beim nächsten
  Druck auf den Kalibrier-Button wirksam wird, und nennt den aktuell gültigen Wert

### Umbenanntes Global
`calib_raw_half` → `calib_raw_ref`, weil "half" (für 0,5 kg) nicht mehr stimmt.

## Einstellbares Messintervall (ersetzt das feste 6h)

Das fest kompilierte `update_interval: 6h` ist raus. Stattdessen:

- **Number-Entity "Waage eG Messintervall"** (`platform: template`,
  `mode: box`, also Zahl eintippen statt Schieberegler), Einheit **Minuten**,
  Bereich **1 bis 10080** (7 Tage), `restore_value: true`, Voreinstellung
  **360** - damit verhält sich das Gerät ab Werk wie vorher.
  `entity_category: config`, landet in HA also bei den Einstellungen und nicht
  zwischen den Messwerten.
- **`waage_gewicht` und `waage_uptime` haben `update_interval: never`.**
  Veröffentlicht wird nur noch, wenn jemand `update()` aufruft.
- **`interval: 60s`-Block** als Taktgeber: zählt `minuten_seit_messung` hoch
  und ruft bei Erreichen des eingestellten Werts `id(waage_gewicht).update()`
  und `id(waage_uptime).update()` auf.

Warum ein Minutenzähler und nicht `millis()`: der Zähler ist trivial
nachvollziehbar und hat kein Überlaufproblem - `millis()` läuft auf dem ESP8266
nach ~49 Tagen über, und die Waage soll monatelang durchlaufen.

Zwei bewusste Details:

- **`minuten_seit_messung` ist NICHT `restore_value`.** Nach einem Neustart
  soll sofort wieder gemessen werden, nicht erst nach dem Restintervall.
- **Solange noch nie ein Gewicht veröffentlicht wurde** (`isnan` auf
  `waage_gewicht.state`), misst der Block jede Minute erneut. Dadurch steht
  nach einem Boot der erste Wert nach ~1 min in HA statt erst nach bis zu
  7 Tagen. Das ist auch genau der Moment, in dem der 60s-Filter eingeschwungen ist.
- **`on_value` der Number** setzt den Zähler zurück und misst einmal sofort -
  so sieht man in HA direkt, dass die Änderung angekommen ist.

Die Entkopplung hat einen angenehmen Nebeneffekt: Das Intervall beeinflusst nur
noch, wie oft ein Wert nach HA geht, nicht mehr die Messqualität. Ein kurzes
Intervall kostet keine Genauigkeit, ein langes verschlechtert sie nicht.

## Weitere Änderungen gegenüber dem ersten Stand
- **NaN-Schutz:** Vor dem ersten gefilterten Messwert (direkt nach dem Boot)
  gibt der Template-Sensor `{}` zurück und veröffentlicht damit gar nichts,
  statt `nan` nach HA zu schicken. Die Buttons brechen in dem Fall mit einer
  Warnung im Log ab.
- **"Kalibrieren 0kg" setzt jetzt `tare_offset` auf 0.** Ein frisch gesetzter
  Nullpunkt macht ein altes Tara sinnlos - sonst zeigt die leere Waage direkt
  nach der Nullpunktkalibrierung `-tare_offset` statt 0 an.
- **Plausibilitätsprüfung beim Referenzpunkt:** Ist der Rohwert identisch
  mit dem Nullpunkt, wird abgebrochen (typischer Fehler: Referenzgewicht
  vergessen aufzulegen). Sonst wäre `span == 0` und die Waage tot.
- **Logging:** Alle drei Buttons loggen, was sie gespeichert haben - macht die
  reale Kalibrierung nachvollziehbar.

## Explizit NICHT (mehr) enthalten
- **Temperaturkompensation** wurde eingebaut (DS18B20 auf D4, Number-Entity
  zum Feintunen des °C-Koeffizienten aus HA) und auf expliziten Wunsch
  **wieder komplett entfernt**. Falls das nochmal gewünscht wird: Konzept war
  ein linearer Korrekturfaktor `kg -= (temp_aktuell - temp_bei_kalibrierung) * koeffizient`,
  Koeffizient über eine `number:`-Template-Entity aus HA einstellbar.
- Kein `hx711.tare` als Action verwendet (existiert in ESPHome nicht -
  war ein Fehler in einer früheren Version, korrigiert per Lambda+Globals)
- Physischer Tara-Taster wurde bewusst verworfen, stattdessen virtueller
  Button gewünscht
- `state_class: measurement` ist weiterhin **nicht** gesetzt (steht unter
  "optional/später"). Ohne das legt HA keine Langzeitstatistik an - für die
  angedachte Schwarmalarm-Automation und Trachtauswertung wäre es sinnvoll.

## Validierung (durchgeführt)
- `esphome config waage-eg.yaml` gegen **ESPHome 2026.6.5**: *Configuration is
  valid*, keine Warnungen, keine Deprecations.
- Ein voller `esphome compile` war nicht möglich - die PlatformIO-Registry ist
  in der Build-Umgebung durch die Egress-Policy blockiert (403 auf
  `api.registry.platformio.org`). Das ist eine Einschränkung der Umgebung,
  kein Problem der Konfiguration.
- Ersatzweise geprüft:
  - Rückgabetyp des Template-Sensor-Lambdas ist `cg.optional.template(float)`
    (aus `components/template/sensor/__init__.py`) → `return {};` ist gültig
    und bedeutet "nichts veröffentlichen"
  - Die Lambda-Körper wurden 1:1 als eigenständiges C++17-Programm mit
    `g++ -Wall -Wextra` kompiliert (fehlerfrei) und durchgerechnet:
    leer → 0,0 | 0,5kg-Punkt → 0,5 | 20 kg → 20,0 | 47,3 kg → 47,3 |
    Rundungsgrenzen 1,24 → 1,2 und 1,26 → 1,3, ebenso 0,04 → 0,0,
    0,06 → 0,1 und symmetrisch ins Negative | kein `-0.0`
    (signbit geprüft) | Tara bei 3,4 kg → 0,0, danach 13,7 kg brutto → 10,3 |
    **alle 2001 Stufen von 0 bis 200 kg in 0,1er-Schritten ohne Abweichung**
    (bestätigt nebenbei, dass die `float`-Genauigkeit über den ganzen
    Messbereich reicht)
  - Das frei wählbare Referenzgewicht wurde separat durchgerechnet, mit
    realistischen 18035 counts/kg: Kalibrierung auf 10 kg → leer 0,0 |
    Referenzpunkt 10,0 | 47,3 | 132,6 | 200,0 kg jeweils exakt.
    **Der kritische Fall ist explizit geprüft:** Number nach der Kalibrierung
    von 10 auf 0,5 kg geändert → Anzeige bleibt bei 47,30 kg, "Kalibriert mit"
    bleibt 10,000 kg. Abgewiesen werden 0 kg, negative Werte, `NaN` und
    "Gewicht vergessen" (Rohwert == Nullpunkt), wobei `calib_kg_ref`
    unberührt bleibt. Tara rechnet ebenfalls gegen das gespeicherte
    Referenzgewicht (Tara bei 23,4 kg → 0,0, danach 31,9 kg brutto → 8,5 kg).
  - Filterverhalten gegen `components/sensor/filter.cpp` verifiziert
    (siehe "Sampling-Strategie")
  - `update()` ist bei beiden Sensoren public aufrufbar: `TemplateSensor` und
    `UptimeSecondsSensor` erben beide von `PollingComponent`, wo
    `virtual void update()` im public-Abschnitt steht - `id(...).update()`
    aus einem Lambda ist damit zulässig
  - Der `interval:`-Block wurde ebenfalls als C++-Programm minutenweise
    durchsimuliert: Boot mit noch leerer Number → Messung nach 1 min |
    360 min → Abstand exakt 360 min | Umstellung auf 15 min im laufenden
    Betrieb → ab da exakt alle 15 min | Minimum 1 min → jede Minute |
    Maximum 10080 min über 14 simulierte Tage → exakt 2 Messungen,
    kein Zählerüberlauf

## Offene Punkte für die Fortsetzung
1. Wägezellen (4×, C3-Klasse) noch beschaffen - **vorher entscheiden:**
   Eingangswiderstand 1000 Ω vs. 350 Ω (siehe docs/)
2. Summier-/Junction-Box wählen: Fertigteil mit Trimmpotis (Eckenabgleich) vs.
   Eigenbau ohne. Oder ganz anders: 4× HX711 mit Software-Summe statt
   Parallelschaltung - Abwägung in `docs/waegezellen-verkabelung.md`
3. Kalibrierung nach Hardware-Aufbau real durchführen: Referenzgewicht-Number
   auf die Masse des Prüfgewichts stellen → leer räumen → 1 min warten →
   "Kalibrieren 0kg" → Gewicht mittig auflegen → 1 min warten →
   "Kalibrieren Referenzgewicht". Möglichst schweres Prüfgewicht nehmen
   (siehe Abschnitt "Frei wählbares Referenzgewicht")
4. Secrets in `secrets.yaml` ergänzen - Vorlage liegt als `secrets.yaml.example`
   bei: `wifi_ssid`, `wifi_password`, `ap_fallback_password`,
   `api_encryption_key`, `ota_password`
5. Optional/später denkbar (nur angesprochen, nicht umgesetzt):
   - Deep Sleep für Batteriebetrieb (Trade-off: Buttons dann nicht
     jederzeit erreichbar - und die Sampling-Strategie müsste umgebaut werden)
   - Schwarm-Alarm-Automation in HA (plötzlicher Gewichtsverlust)
   - `state_class: measurement` für Langzeit-Statistiken in HA
   - Saisonale Messintervalle: die Voraussetzung dafür steht jetzt (die
     Number-Entity ist aus HA beschreibbar), es fehlt nur noch eine
     HA-Automation, die "Waage eG Messintervall" z. B. im Frühjahr auf 60
     und im Winter auf 1440 setzt

## Umgebungs-Infos (Home Assistant-seitig)
- ESPHome Device Builder Add-on: Slug `5c53de3b_esphome`, Port `6052`,
  `home_assistant_dashboard_integration: true`, `leave_front_door_open: true`
- Bereits vorhandene ESPHome-Geräte (nicht Teil dieses Projekts, nur zur Info):
  `D1-Mini-Tester` (esp01_1m, DHT+I2C+INA226) und `Display3` (nodemcuv2, LCD/PCF8574)

## Aktueller YAML-Stand
Siehe [`waage-eg.yaml`](waage-eg.yaml) - der 1:1 aktuelle Stand.
