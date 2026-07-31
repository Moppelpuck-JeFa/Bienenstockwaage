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
- Ausgabe in **kg**, gerundet auf **0,5 kg genau**
- **Kein festes `calibrate_linear`** im YAML - Kalibrierung läuft komplett
  dynamisch über zwei Buttons in Home Assistant:
  - **"Waage eG Kalibrieren 0kg"** - Waage leer stellen, dann drücken
  - **"Waage eG Kalibrieren 0,5kg"** - 500g-Referenzgewicht auflegen, dann drücken
- **"Waage eG Tara"** - separater Button, nullt nur das aktuell aufliegende
  Gewicht (z. B. Behälter), ändert NICHTS an der Kalibrierung selbst
- Messintervall **in Home Assistant: alle 6 Stunden** (siehe Abschnitt
  "Sampling-Strategie" - der HX711 selbst wird bewusst schneller abgefragt)
- Rausch-Filterung: `median` + `sliding_window_moving_average` auf dem
  rohen HX711-Signal, bevor die Kalibrierrechnung angewendet wird
- `api.reboot_timeout: 0s` - verhindert unnötige Neustarts, da bei 6h-Intervall
  zwischendurch mal kurz keine HA-Verbindung bestehen kann
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
(max. 5s alt) und gut gemittelt. Die 6h-Taktung sitzt ausschließlich am
sichtbaren Template-Sensor. Die Buttons brauchen dadurch kein
`component.update` + `delay` mehr - sie lesen einfach den gefilterten Zustand.

**Bedienhinweis:** Weil ~60s gemittelt wird, gilt für alle drei Buttons:
erst auflegen/abräumen, **~1 Minute warten**, dann drücken.

Netzbetrieb vorausgesetzt kostet das schnellere Sampling nichts. Bei einem
späteren Umstieg auf Deep Sleep müsste diese Strategie neu gedacht werden.

## Weitere Änderungen gegenüber dem ersten Stand
- **NaN-Schutz:** Vor dem ersten gefilterten Messwert (direkt nach dem Boot)
  gibt der Template-Sensor `{}` zurück und veröffentlicht damit gar nichts,
  statt `nan` nach HA zu schicken. Die Buttons brechen in dem Fall mit einer
  Warnung im Log ab.
- **"Kalibrieren 0kg" setzt jetzt `tare_offset` auf 0.** Ein frisch gesetzter
  Nullpunkt macht ein altes Tara sinnlos - sonst zeigt die leere Waage direkt
  nach der Nullpunktkalibrierung `-tare_offset` statt 0 an.
- **Plausibilitätsprüfung bei "Kalibrieren 0,5kg":** Ist der Rohwert identisch
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
    leer → 0,0 kg | 0,5kg-Punkt → 0,5 kg | 20 kg → 20,0 kg |
    1,2 kg → 1,0 kg | 1,3 kg → 1,5 kg (Rundung korrekt) |
    Tara bei 3 kg → 0,0 kg, danach 13 kg brutto → 10,0 kg
  - Filterverhalten gegen `components/sensor/filter.cpp` verifiziert
    (siehe "Sampling-Strategie")

## Offene Punkte für die Fortsetzung
1. Wägezellen (4×, C3-Klasse) noch beschaffen - **vorher entscheiden:**
   Eingangswiderstand 1000 Ω vs. 350 Ω (siehe docs/)
2. Summier-/Junction-Box wählen: Fertigteil mit Trimmpotis (Eckenabgleich) vs.
   Eigenbau ohne. Oder ganz anders: 4× HX711 mit Software-Summe statt
   Parallelschaltung - Abwägung in `docs/waegezellen-verkabelung.md`
3. Kalibrierung nach Hardware-Aufbau real durchführen (leer → 1 min warten →
   "Kalibrieren 0kg", 500g-Gewicht auflegen → 1 min warten → "Kalibrieren 0,5kg")
4. Secrets in `secrets.yaml` ergänzen - Vorlage liegt als `secrets.yaml.example`
   bei: `wifi_ssid`, `wifi_password`, `ap_fallback_password`,
   `api_encryption_key`, `ota_password`
5. Optional/später denkbar (nur angesprochen, nicht umgesetzt):
   - Deep Sleep für Batteriebetrieb (Trade-off: Buttons dann nicht
     jederzeit erreichbar - und die Sampling-Strategie müsste umgebaut werden)
   - Schwarm-Alarm-Automation in HA (plötzlicher Gewichtsverlust)
   - `state_class: measurement` für Langzeit-Statistiken in HA
   - Variable Messintervalle je Saison (Frühling engmaschiger)

## Umgebungs-Infos (Home Assistant-seitig)
- ESPHome Device Builder Add-on: Slug `5c53de3b_esphome`, Port `6052`,
  `home_assistant_dashboard_integration: true`, `leave_front_door_open: true`
- Bereits vorhandene ESPHome-Geräte (nicht Teil dieses Projekts, nur zur Info):
  `D1-Mini-Tester` (esp01_1m, DHT+I2C+INA226) und `Display3` (nodemcuv2, LCD/PCF8574)

## Aktueller YAML-Stand
Siehe [`waage-eg.yaml`](waage-eg.yaml) - der 1:1 aktuelle Stand.
