# Sessionbericht 03.08.2026

Zusammenfassung einer Arbeitssitzung an der Stockwaage „Waage eG". Vier Themen:
die ausgewertete Temperaturdrift, die Vorbereitung auf Deep Sleep, der neue
Durchsichtmodus — und ein Kalibrierungsverlust, der dabei aufgefallen ist.

**Anlagenzustand:** Steckbrett am Netzteil. Der Zielaufbau ist ein Standort im
Garten, versorgt aus einem NiMH-Akku (2600 mAh) über eine Ladelogik am
Solarpanel.

---

## 1. Temperaturdrift: ausgewertet, Kompensation lohnt sich

Die offene Messung aus den Projektnotizen ist beantwortet. Die Frage lautete
„saubere Gerade oder breit streuende Punktwolke" — es ist eindeutig eine
**Gerade**.

**Datenbasis:** 177 Messpunkte, 01.08. 12:30 bis 03.08. 10:00 (45,5 h),
Temperaturhub 17,4–25,0 °C. Ausgewertet wurde der **Rohwert**, nicht das
Gewicht: er ist unabhängig von der Kalibrierung und nicht auf 0,1 kg gerundet.
Quelle sind die 5-Minuten-Statistiken aus dem HA-Recorder.

| | |
|---|---|
| Temperaturkoeffizient | **+34,5 g/K** (± 0,9) |
| Bestimmtheitsmaß R² | 0,94 (Modell Temperatur + Zeit) |
| Streuung ohne Kompensation | 73 g (Spannweite 258 g) |
| Streuung mit Kompensation | 25 g |
| Hysterese Erwärmung ↔ Abkühlung | ±1 g |
| Thermischer Nachlauf | τ ≈ 20 min |

Entscheidend sind die letzten beiden Zeilen: **Hysterese praktisch null,
Nachlauf gering.** Genau das war die Bedingung aus den Projektnotizen. Hätten
thermische Gradienten zwischen den vier Zellen dominiert, wäre eine breite
Schleife herausgekommen, die ein einzelner Sensor nicht korrigieren kann. Der
DS18B20 sitzt thermisch dicht genug an den Zellen. Ein quadratischer Term bringt
nichts (R²adj 0,9408 → 0,9411), die Beziehung ist linear.

### Ein zweiter, unabhängiger Effekt

Die späten Messpunkte liegen systematisch unter der Regressionsgeraden. Dahinter
steckt ein Trend von **−47 g/Tag** (± 3,6), der nichts mit der Temperatur zu tun
hat — ein Viertel der gesamten Signalamplitude.

Das hat Folgen für den Koeffizienten: Fittet man nur gegen die Temperatur,
schiebt dieser Trend das Ergebnis auf **+39,4 g/K**, also 5 g/K zu hoch. Die
34,5 g/K aus der Zwei-Parameter-Regression sind der belastbarere Wert.

Zur Natur des Trends: Ein √t- bzw. log-Verlauf passt besser als ein linearer
(R²adj 0,9486 / 0,9478 gegen 0,9408), und die Rate wird gegen Ende kleiner. Das
spricht für **abklingendes Kriechen der Zellen** nach dem Auflegen der Last, nicht
für konstanten Massenverlust. Bei zwei Tagen ist das aber nicht gesichert.

### Empfehlung

Kompensation einbauen — aber den Koeffizienten noch nicht festnageln. Über nur
zwei Tage sind Temperatur- und Zeitanteil teilweise konfundiert; die ±0,9 g/K
unterschätzen die echte Unsicherheit. Drei bis vier weitere Tage, in denen
derselbe Temperaturbereich mehrfach durchlaufen wird, trennen die Anteile sauber.

Formel für das YAML (`calib_temp` ist der beim Nullpunkt gespeicherte Bezug):

```
kg -= (temp_jetzt - calib_temp) * 0.0345
```

**Vorbehalt zur Extrapolation:** Gemessen sind 7,6 K Hub. Draußen sind 20–30 K
Tagesgang normal, das wären 0,7–1,0 kg. Der Koeffizient wird dort also sehr
wichtig — ist aber nur über ein Drittel dieser Spanne bestimmt.

### Nebenbefund zur Messung

Der Minutentakt endete am 02.08. um exakt 03:00 Uhr. Das ist die Automation
`Bienen: Messintervall nach Saison`, die für August auf 60 min stellt — die in
den Notizen beschriebene Nebenwirkung. Tag 2 hat deshalb nur 19 statt 158
Messpunkte und ein schwächeres R² (0,78 gegen 0,92). Für eine Fortsetzung der
Messung muss die Automation vorübergehend aus.

---

## 2. Deep Sleep vorbereitet: DOUT von D0 auf D6

Vollständig in [`deep-sleep-vorbereitung.md`](deep-sleep-vorbereitung.md).
Die Kurzfassung:

**Der bekannte Blocker war GPIO16** — der einzige Pin, über den der ESP8266 aus
dem Deep Sleep aufwachen kann. Dort hing `DOUT`. Umgezogen auf **D6 (GPIO12)**,
eine Zeile im YAML. Die Kalibrierung bleibt davon unberührt, weil der Rohwert
nicht davon abhängt, welcher GPIO ihn einliest.

**Der eigentliche Blocker ist ein anderer.** Der ESPHome-HX711-Treiber kennt
keinen Power-Down (geprüft in `components/hx711/hx711.cpp`). HX711 und
Wägezellenbrücke ziehen deshalb **~5,8 mA dauerhaft**, auch wenn der ESP
schläft — das Zehnfache des schlafenden ESP. Deep Sleep allein bringt von einem
Tag Laufzeit auf 13 Tage, erst mit HX711-Power-Down auf rund fünf Monate.

Zwei Funde, die sonst Zeit gekostet hätten:

- **Die 71-Minuten-Grenze wird nicht validiert.** Der RTC-Timer nimmt die
  Schlafdauer als 32-Bit-Mikrosekundenwert. ESPHomes Validator begrenzt nur
  BK72XX und Zephyr; `sleep_duration: 360min` läuft anstandslos durch
  `esphome config` und verhält sich am Gerät falsch.
- **Der Durchsicht-Taster funktioniert unter Deep Sleep nicht.** Der ESP8266
  wacht nur über den RTC-Timer auf. Für den Solarbetrieb wäre ein *rastender*
  Schalter mit direkt daran verdrahteter LED die richtige Lösung.

---

## 3. Durchsichtmodus (neu)

Während einer Durchsicht wiegt die Waage Zargen, Hände und Stockmeißel. Diese
Werte haben `state_class: measurement` und landen dauerhaft in der
Langzeitstatistik.

| | Pin | GPIO |
|---|---|---|
| Taster gegen GND, interner Pull-up | `D2` | 4 |
| LED gegen GND, 1 kΩ Vorwiderstand | `D7` | 13 |

Ein Druck schaltet um, die LED zeigt den Zustand und blinkt in den letzten fünf
Minuten. Bedienbar auch als Schalter in HA; die Dauer stellt die Number-Entity
„Durchsichtdauer" (5–240 min).

**Gesperrt wird die Veröffentlichung, nicht die Messung** — der HX711 läuft mit
seiner Filterkette weiter und ist am Ende sofort wieder eingeschwungen. Danach
folgen zwei Minuten Nachlauf, damit das 60-Sekunden-Mittel die Manipulation
ausspült.

Damit ist der D1 Mini praktisch voll: GPIO 4, 5, 12, 13, 14 belegt, GPIO16 für
den Weckpfad reserviert, GPIO2 für den geplanten MOSFET am HX711.

**Offen auf der HA-Seite:** Nach der Durchsicht springt das Gewicht in einem
Schritt. Der 20-min-Ableitungshelfer liest das als Absturz — `Bienen:
Schwarm-Alarm` löst nach jeder Durchsicht mit Honigernte fälschlich aus. Die
Automation braucht eine Sperre, die **länger** ist als das Ableitungsfenster:

```yaml
condition:
  - condition: state
    entity_id: switch.waage_eg_durchsichtmodus
    state: "off"
    for: "00:30:00"
```

Sinngemäß dasselbe für `Bienen: Futtervorrat kritisch`.

---

## 4. Dashboard

Im Dashboard `bienen-stockwaage`, Ansicht **Technik**, Abschnitt
**Einstellungen** kam eine Untergruppe „Durchsicht" dazu: Schalter als Tile mit
Toggle-Feature, die Dauer als `entities`-Karte (Tippfeld — das Tile-Feature
`numeric-input` kennt nur `buttons|slider`), dazu ein Erklärungstext.

---

## 5. Der Kalibrierungsverlust

Beim Flashen des Durchsichtmodus ist die Kalibrierung verlorengegangen. Belegt
in der HA-Historie:

| Zeit | Kalibrierfaktor | Kalibriert mit |
|---|---|---|
| bis 12:16 | −20.839,9 | 2,218 kg |
| 12:16:58 | *unavailable* (Flash) | |
| **12:18:14** | **3.500,0** | **0,5 kg** |
| 13:13:08 | −8.770,6 | 2,218 kg |

Die **3.500** sind die im README beschriebene Platzhalter-Signatur. Auslöser:
Der Flash brachte zwei neue Globals mit. **`restore_from_flash: true` schützt
gegen Stromausfall, nicht gegen eine veränderte Preferences-Belegung.**

Die Neukalibrierung um 13:13 war nur halb — es wurde nur „Kalibrieren
Referenzgewicht" gedrückt. `calib_raw_zero` blieb auf dem Platzhalter 0, und
damit kollabiert die Umrechnung zu `kg = raw / Faktor`. Exakt messbar: Gewicht
2,3 kg bei Rohwert −20.079,8 und Faktor −8.770,6.

**Die Hardware ist in Ordnung.** Der echte Nullpunkt lässt sich zurückrechnen:
`−20.080 + 2,218 × 20.840 = +26.143 counts` — praktisch die 25.830 counts aus
den Projektnotizen. Der Pin-Wechsel hat nichts verändert.

**Zu tun:** Waage leer räumen → 1 min warten → „Kalibrieren 0 kg" → Prüfgewicht
mittig auflegen → 1 min warten → „Kalibrieren Referenzgewicht". Danach sollte
der Faktor wieder bei rund −20.840 stehen und „Kalibriert bei" eine Temperatur
zeigen.

Ausführlich in [`../waage-eg-notes.md`](../waage-eg-notes.md), Abschnitte
„Nachtrag: `restore_from_flash` schuetzt NICHT…" und „Die halbe
Neukalibrierung".

---

## Offene Punkte

1. **Neu kalibrieren** (beide Schritte) — die Waage misst aktuell falsch.
2. **Schwarm-Alarm gegen den Durchsichtmodus sperren**, sonst Fehlalarm nach
   jeder Durchsicht mit Honigernte.
3. **Driftmessung fortsetzen**, drei bis vier weitere Tage, um Temperatur- und
   Zeitanteil zu trennen. Dafür `Bienen: Messintervall nach Saison`
   vorübergehend deaktivieren.
4. **Was liegt als Prüfgewicht auf?** Entscheidet, ob die −47 g/Tag Kriechen
   sind oder Verdunstung.
5. **Ruhestrom messen**, bevor der Akku geplant wird — die Zahl entscheidet über
   die gesamte Solarauslegung.
6. **Doku-Widersprüche bereinigen:** README und Notizen nennen an mehreren
   Stellen −17.900 counts/kg, gemessen wurden zuletzt −20.840. Dazu stehen in
   `waage-eg.yaml` (Zeile 212 und 391) noch ~18.000 als Erwartungswert und in
   `docs/waegezellen-verkabelung.md` ~9.000.
7. **`input_number.leergewicht_beute` / `mindestgewicht_mit_futter`** stehen
   inzwischen auf 18 bzw. 29 kg — Plausibilität prüfen.
8. **`reboot_timeout: 0s`** nimmt dem Gerät die Selbstheilung bei hängendem
   WLAN. Für den Netzbetrieb wäre ein endlicher Wert besser.

## Werkzeugnotizen

- `esphome config` lief gegen **ESPHome 2026.6.5**. Ein vollständiger
  `esphome compile` war wie schon früher nicht möglich (PlatformIO-Registry in
  der Build-Umgebung gesperrt). Ersatzweise wurde die Zustandslogik des
  Durchsichtmodus als eigenständiges C++17-Programm mit `g++ -Wall -Wextra`
  nachgebaut und minutenweise durchsimuliert: 19 Prüfungen ohne Fehler.
- Dashboard-Edits über `ha_config_set_dashboard(python_transform=…,
  config_hash=…)` statt vollem Config-Replace — funktioniert zuverlässig.
- Der `BestPracticeKey` für gated Write-Tools **rotiert stündlich**. Ein früher
  geholter Schlüssel wird abgelehnt; dann neu über `ha_get_skill_guide` holen.
