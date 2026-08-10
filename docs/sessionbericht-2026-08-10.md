# Sessionbericht 10.08.2026

Die Temperaturkompensation ist eingebaut. Grundlage ist die Messreihe, die seit
der Neukalibrierung am 03.08. durchgelaufen ist — sieben Tage statt der zwei,
auf denen die erste Auswertung beruhte.

**Anlagenzustand unverändert:** ein Stock produktiv („Waage eG"), Steckbrett am
Netzteil, konstante Last (das Prüfgewicht mit 2,218 kg liegt auf). An Hardware
und Verkabelung wurde **nichts** geändert.

---

## 1. Die Auswertung

**Datenbasis:** 164 Messpunkte, 03.08. 17:36 bis 10.08. 11:59 (6,8 Tage),
Temperaturhub **17,5–26,9 °C**. Ausgewertet wurde wieder der Rohwert (unabhängig
von der Kalibrierung, nicht auf 0,1 kg gerundet), Temperatur als Stundenmittel
aus dem HA-Recorder. Die Reihe beginnt bewusst erst nach der vollständigen
Neukalibrierung vom 03.08., davor gilt eine andere Umrechnung.

| | 2 Tage (03.08.) | **7 Tage (heute)** |
|---|---|---|
| Temperaturhub | 7,6 K | **9,4 K** |
| Temperaturkoeffizient | +34,5 g/K | **+32,5 g/K** (± 0,7) |
| Zeittrend | −47 g/Tag | **−10,9 g/Tag** (± 0,7) |
| R² (Temperatur + Zeit) | 0,94 | **0,965** |
| Streuung ohne Kompensation | 73 g | **62 g** (Spannweite 324 g) |
| Streuung mit Kompensation | 25 g | **15 g** |
| Hysterese | ±1 g | **±1,4 g** |

**Der Vorbehalt aus dem 03.08.-Bericht hat sich bestätigt und aufgelöst.** Dort
stand, die ±0,9 g/K unterschätzten die echte Unsicherheit, weil Temperatur- und
Zeitanteil über nur zwei Tage konfundiert sind. Genau so ist es gekommen: Der
Zeittrend ist von −47 auf −10,9 g/Tag zusammengeschrumpft, der Koeffizient von
34,5 auf 32,5 g/K. Beides passt zu **abklingendem Kriechen** der Zellen nach dem
Auflegen der Last — bei einer Verdunstung oder einem echten Massenverlust wäre
die Rate gleich geblieben.

**Die Beziehung ist weiterhin sauber linear.** Ein quadratischer Term bringt
nichts (R²adj 0,9642 → 0,9649 bei ± 0,23 auf dem Koeffizienten). Die Hysterese
zwischen Erwärmung und Abkühlung liegt bei ±1,4 g und damit weiter im Rauschen —
das war die Bedingung dafür, dass ein einzelner Sensor überhaupt kompensieren
kann. Wäre die Streuung von thermischen Gradienten zwischen den vier Zellen
dominiert, käme hier eine breite Schleife heraus.

### Wie belastbar sind die 32,5 g/K?

Die ±0,7 g/K sind der statistische Fehler und wieder zu optimistisch. Die
Teilmengen der Reihe liegen auseinander:

| Teilmenge | Koeffizient |
|---|---|
| erste Hälfte | +32,8 g/K |
| zweite Hälfte | +28,4 g/K |
| nur Erwärmungsphasen | +35,2 g/K |
| nur Abkühlungsphasen | +32,9 g/K |
| Punkte über 23 °C | +34,9 g/K |
| Punkte unter 23 °C | +30,5 g/K |
| Einzeltage (7×) | +23,4 bis +35,2 g/K |

**Realistisch ist also ± 3 g/K, nicht ± 0,7.** Über 20 K Tagesgang sind das
±60 g Restfehler — immer noch fünfmal besser als die 650 g, die ohne
Kompensation anfallen. Genau deshalb ist der Koeffizient eine Number-Entity
geworden und keine Zahl im YAML: Nachjustieren kostet damit eine Eingabe in HA
statt einen Flash, und jeder Flash kann die Kalibrierung mitnehmen.

**Einzeltage taugen nicht.** Die Tageswerte streuen zwischen 23 und 35 g/K, weil
über einen Tag Temperatur- und Zeitanteil nicht zu trennen sind und der
thermische Nachlauf innerhalb eines Tages die Steigung drückt. Wer nachmisst,
braucht mehrere Tage und muss den Zeitterm mitfitten.

### Nebenbefund: der thermische Nachlauf ist größer als gedacht

Legt man die Temperaturreihe vor dem Fit durch einen Tiefpass, verbessert sich
das Modell deutlich — mit **τ ≈ 90 min** (nicht 20 min wie aus den zwei Tagen
geschätzt) steigt R²adj von 0,964 auf 0,991, die Reststreuung fällt von 15 g auf
8 g, und der Koeffizient läge dann bei 34,7 g/K.

**Eingebaut ist das trotzdem nicht**, aus zwei Gründen:

1. Die Firmware rechnet mit dem **momentanen** Sensorwert. Der dazu passende
   Koeffizient ist der gegen die momentane Temperatur gefittete — also 32,5,
   nicht 34,7. Die 34,7 mit einem ungefilterten Messwert zu verrechnen würde
   überkorrigieren.
2. τ hängt an der thermischen Masse des Aufbaus. Der Zielaufbau steht im Garten
   und wird ein völlig anderes τ haben. Ein Filter mit einer hier gemessenen
   Zeitkonstante wäre dort schlicht falsch.

Das bleibt als Reserve: Wenn nach dem Umzug die 15 g Reststreuung stören, ist
eine Glättung der Temperatur vor der Verrechnung der nächste Hebel — er ist
etwa einen Faktor 2 wert.

---

## 2. Was eingebaut wurde

```
Gewicht = Bruttogewicht − (Temperatur − calib_temp) × Koeffizient − Tara
```

`calib_temp` ist die beim Nullpunkt festgehaltene Temperatur, sichtbar als
„Kalibriert bei". Sie wird seit dem 03.08. mitgeschrieben — genau für diesen
Zweck. Der Einbau selbst brauchte deshalb keine Neukalibrierung: Der
Bezugspunkt lag bereits vor. (Der Flash hat die Kalibrierung dann trotzdem
gekostet, aber aus dem bekannten anderen Grund — siehe Abschnitt 3a.)

### Neu in Home Assistant

| Entity | Typ | Bedeutung |
|---|---|---|
| `number.waage_eg_temperaturkoeffizient` | config | Koeffizient in g/K, Vorgabe 32,5. `0` schaltet die Kompensation ab |
| `sensor.waage_eg_temperaturkorrektur` | diagnostic | Was gerade abgezogen wird, in kg |

Alle bisherigen 20 Entity-Namen sind unverändert geblieben (nachgewiesen über
den Vergleich der aufgelösten Konfiguration, siehe Validierung).

### Entscheidungen, die nicht offensichtlich sind

**Die Formel steht in einer eigenen Header-Datei**
([`packages/waage-temperatur.h`](../packages/waage-temperatur.h)), eingebunden
über `esphome: includes:`. Gebraucht wird sie an drei Stellen — Gewicht, Tara,
Diagnose-Entity. Dreimal ausgeschrieben könnte sie auseinanderlaufen, und dann
tariert die Waage gegen eine andere Rechnung, als sie anzeigt. Das merkt man
erst Wochen später an einer krummen Tagesbilanz.

Der Pfad lautet `packages/waage-temperatur.h` und löst — wie `!secret` und wie
das `!include` des Packages — gegen das Verzeichnis der **geflashten
Stock-Datei** auf, nicht gegen `packages/`. Ohne das Präfix bricht schon
`esphome config` ab (geprüft).

**Tara zieht die Korrektur mit ab.** Sonst friert das Tara die Korrektur des
Druckzeitpunkts ein, und die Drift wandert mit jedem Grad wieder ins Gewicht
zurück: Die Waage stünde direkt nach dem Tarieren auf null, liefe über den Tag
aber um den vollen Temperaturhub weg. Das ist die Stelle, an der eine
Kompensation am ehesten falsch eingebaut wird.

**Der Rohwert bleibt unkompensiert.** Er ist die Grundlage jeder künftigen
Nachmessung des Koeffizienten; ein bereits korrigierter Wert wäre dafür
wertlos. Aus demselben Grund läuft der DS18B20 weiter auf seinem eigenen
60-s-Takt.

**Ohne Bezugspunkt wird nicht kompensiert.** Ist „Kalibriert bei" leer
(„Kalibrieren 0 kg" nie gedrückt), bleibt die Korrektur 0. Eine geratene
Korrektur wäre schlechter als gar keine, und man würde es nicht sehen.

**Ein Aussetzer des DS18B20 lässt das Gewicht nicht springen.** Die letzte
gültige Temperatur wird in einem Global gehalten (`letzte_temperatur`, bewusst
ohne `restore_value`). Ohne das würde die Kompensation bei einem
fehlgeschlagenen 1-Wire-Lesevorgang stumm ausfallen und das Gewicht um bis zu
0,3 kg springen — in HA nicht von einem echten Ereignis zu unterscheiden.

**Nach einem Neustart wartet die Waage auf die erste Temperatur**, höchstens
drei Minuten und nur, wenn die Kompensation überhaupt aktiv ist. HX711 und
DS18B20 laufen beide im 60-s-Takt und melden sich in unbestimmter Reihenfolge;
ohne die Sperre ginge nach jedem Neustart genau ein unkompensierter Wert nach
HA und stünde dort dauerhaft als Stufe in der Statistik. Ein defekter Sensor
legt die Waage damit nicht still, er kostet nur diese drei Minuten.

**Neue Stöcke starten mit `0`.** Der Koeffizient von „Waage eG" gilt für dessen
Zellen und dessen Aufbau. Das Vorgehen zum Messen steht im README.

---

## 3. Validierung

**`esphome config` gegen ESPHome 2026.6.5:** alle drei Stock-Dateien
*Configuration is valid*, keine Warnungen.

**Vergleich der aufgelösten Konfiguration** von `waage-eg` vorher/nachher (die
Technik aus dem 04.08.-Bericht): genau zwei zusätzliche `name:`-Felder
(Temperaturkoeffizient, Temperaturkorrektur), alle 20 bestehenden Entity-Namen
identisch, die Kalibrier-Globals unverändert.

**Die Formel selbst** ist als eigenständiges C++17-Programm mit
`g++ -Wall -Wextra` durchgerechnet — und zwar durch Einbinden der **echten**
Header-Datei, nicht einer Abschrift. Das Programm liegt als
[`tests/waage-temperatur-test.cpp`](../tests/waage-temperatur-test.cpp) im Repo.
**4028 Prüfungen, 0 Fehler:**

- Vorzeichen und Betrag der Korrektur, Bezugstemperatur → exakt 0
- Alle vier NAN-Fälle (Temperatur, Bezugspunkt, Koeffizient fehlt; nie
  kalibriert) → keine Korrektur statt Unsinn
- Konstante Last über 15–35 °C: Anzeige bleibt auf 30,0 kg, unkompensiert
  wandert sie über 600 g
- **Tara über einen Temperaturwechsel:** morgens bei 18 °C tariert, bei 32 °C
  noch immer 0,0 kg (ohne die Korrektur im Tara wären es 0,455 kg), eine echte
  Laständerung von 2,5 kg wird weiterhin korrekt angezeigt
- **Rückwärtskompatibilität:** mit Koeffizient 0 und bei genau der
  Bezugstemperatur kommt exakt das Alte heraus — je 2001 Stufen von 0 bis 200 kg
  in 0,1er-Schritten ohne Abweichung
- Die Warte-Logik nach dem Boot, minutenweise: erster Wert nach 1/2/3 min je
  nachdem, wann der DS18B20 liefert; bei defektem Sensor nach 4 min; bei
  ausgeschalteter Kompensation ohne Warten
- **Die echte Messreihe durchgerechnet:** Anzeigespanne bei konstanter Last
  300 g → 100 g (in 0,1-kg-Schritten, deshalb gerundet)

Ein vollständiger `esphome compile` war wie in den früheren Sessions nicht
möglich (PlatformIO-Registry durch die Egress-Policy gesperrt).

---

## 3a. Nachtrag am Abend: Flash, Neukalibrierung, Fehlalarm

Die Firmware ist um 14:45 geflasht worden, nach zwei weiteren Neustarts lief
sie ab 15:00. Drei Beobachtungen daraus:

**Die Kalibrierung hat den Flash nicht überlebt** — wie befürchtet, das neue
Global `letzte_temperatur` hat gereicht. Um 15:01/15:03 wurde neu kalibriert,
beide Schritte. Faktor jetzt −20.874 counts/kg, „Kalibriert bei" 25,8 °C.
Die Kompensation arbeitet: „Temperaturkorrektur" stand um 15:04 bei 0,002 kg
(Kalibriertemperatur ≈ aktuelle Temperatur) und um 16:42 bei 0,018 kg.

**Der Taktgeber läuft korrekt.** Kurzzeitig sah es nach einem Ausfall der
automatischen Messung aus. War keiner: Bei 60 min Intervall setzen sowohl jeder
Neustart als auch jeder Druck auf „Jetzt messen" den Zähler auf null, und
davon gab es an dem Nachmittag vier. Die erste planmäßige Messung nach dem
letzten Reset kam um **16:04:06**, exakt 3600 s nach 15:04:08. Auch die neue
Warte-Sperre hat nicht gebremst: erster Wert nach dem Boot bei 61 s Betriebszeit.

**Zwei Messungen lagen außerhalb des Takts** (16:37, 16:42), ohne dass eine
HA-Button-Entity sich geändert hätte. Erklärung: Buttons, die über die
**ESPHome-Weboberfläche auf Port 80** gedrückt werden, erreichen HA nicht — das
Gerät führt `on_press` aus und veröffentlicht, die Button-Entity in HA bleibt
auf ihrem alten Zeitstempel stehen. Das ist für die Fehlersuche wichtig und war
für die Alarm-Sperre unten ausschlaggebend.

**Der Schwarm-Alarm hat um 15:04:08 fehlausgelöst** (bestätigt über
`last_triggered`). Die Neukalibrierung ließ das Gewicht von 2,2 auf −1,6 auf
0,0 springen, der 20-min-Ableitungshelfer machte daraus −3,3 kg/h, Schwelle
ist −3 kg/h. Die seit dem 03.08. offene Sperre war also nicht nur theoretisch
fällig — und sie hätte in ihrer damals vorgeschlagenen Form **nicht geholfen**,
weil der Durchsichtmodus die ganze Zeit aus war.

### Die Sperre, wie sie jetzt eingebaut ist

`Bienen: Schwarm-Alarm` hat drei Bedingungen zusätzlich zum Zeitfenster:

| Sperre | Bedingung | Fängt ab |
|---|---|---|
| Durchsicht | `switch.waage_eg_durchsichtmodus` = off `for: 00:30:00` | Durchsicht, Honigernte |
| Neustart | `sensor.waage_eg_betriebszeit` above 1800 | Neustart, vor allem der Flash mit Kalibrierungsverlust |
| Kalibrierung | Template auf `last_changed` | Kalibrieren, Tarieren |

Alle drei sperren **30 min**, nicht 20 — länger als das Ableitungsfenster,
sonst steckt der Sprung beim Freigeben noch darin.

Die Kalibrier-Sperre prüft `last_changed` von `sensor.waage_eg_kalibrierfaktor`,
`sensor.waage_eg_kalibriert_bei` und den drei Buttons. **Die Diagnose-Sensoren
sind die wichtigeren** — sie ändern sich unabhängig davon, ob der Druck aus HA
oder aus der ESPHome-Weboberfläche kam (siehe oben). Verbleibende Lücke: ein
Tara über die Weboberfläche hinterlässt keine eigene Entity.

Dafür gibt es keine native Bedingung: `state` mit `for:` braucht einen festen
Zielzustand, der Zustand eines Buttons ist aber der Zeitstempel des letzten
Drucks. Gefragt ist „hat sich lange nicht geändert". Der Best-Practice-Prüfer
des MCP-Servers meldet das Template an; die Begründung steht als `note:` in der
Automation.

**Geprüft:** Automation lädt (`state: on`, nicht `unavailable`), alle vier
Bedingungen sind aktuell erfüllt — ein echter Schwarm käme also weiterhin durch.
Gegenprobe auf die Situation um 15:04: Betriebszeit 242 s und Kalibrierung
1 min her — zwei von drei Sperren hätten den Fehlalarm unabhängig voneinander
verhindert.

---

## 4. Nach dem Flash zu prüfen

1. **Kalibrierfaktor** — erwartet rund **−20.845 counts/kg**. Dieser Flash
   bringt ein neues Global (`letzte_temperatur`) mit, und genau das hat am
   03.08. die Kalibrierung gekostet. Steht dort der Platzhalter 3.500: **beide**
   Kalibrierschritte fahren, nicht nur den Referenzpunkt.
2. **„Kalibriert bei"** — muss **24,6 °C** zeigen. Ist das Feld leer, ist der
   Bezugspunkt weg und die Kompensation aus.
3. **„Temperaturkoeffizient"** — muss 32,5 g/K zeigen. Der Startwert greift nur
   beim allerersten Boot; nach einem Preferences-Verlust steht dort ebenfalls
   32,5, das ist hier also unauffällig.
4. **„Temperaturkorrektur"** — muss sich über den Tag sichtbar bewegen. Bei
   17,5 °C sind es −0,229 kg, bei 26,9 °C +0,076 kg. Dauerhaft 0,000 heißt:
   Kompensation aus (siehe Punkt 2).

**Die Anzeige springt beim ersten Messwert nach dem Flash** um die aktuelle
Korrektur — bei 25 °C um etwa +15 g, bei 18 °C um −0,2 kg. Das ist erwartet und
keine Fehlfunktion. Wer es sauber in der Historie haben will, flasht früh
morgens, wenn die Temperatur nahe an den 24,6 °C des Bezugspunkts liegt.

---

## Offene Punkte

**Aus dieser Session neu:**
- **Nach dem Umzug in den Garten den Koeffizienten neu bestimmen.** Dort ist die
  Voraussetzung „alle vier Zellen gleich warm" am ehesten verletzt (Sonne auf
  einer Seite). Auch die Hysterese ist dann erneut zu prüfen — bleibt sie klein,
  passt das Modell weiter.
- **Dashboard:** „Temperaturkorrektur" und „Temperaturkoeffizient" sind noch
  nicht eingebunden. Die Korrektur gehört neben das Gewicht, der Koeffizient
  unter „Einstellungen".
- Optional, wenn die 15 g Reststreuung stören: Temperatur vor der Verrechnung
  glätten (τ war hier ≈ 90 min, siehe Abschnitt 1). Bringt etwa Faktor 2.

**Aus früheren Sessions weiterhin offen:**
- ~~**Schwarm-Alarm gegen den Durchsichtmodus sperren**~~ — erledigt, siehe
  Abschnitt 3a. Dasselbe fehlt noch für `Bienen: Futtervorrat kritisch`
  (weniger dringend: der Schwellwert löst erst nach 2 h Überschreitung aus).
- **Namen der neuen Stöcke festlegen** — vor dem ersten Flash.
- **HA-Seite für die neuen Stöcke:** Helfer, Automationen, Dashboard.
- `input_number.leergewicht_beute` / `mindestgewicht_mit_futter` stehen auf
  18 bzw. 29 kg — Plausibilität prüfen.
- **Doku-Widersprüche:** an einigen Stellen stehen noch −17.900 counts/kg,
  gemessen sind −20.845.
- **Ruhestrom messen**, bevor der Akku geplant wird.
- **`reboot_timeout: 0s`** nimmt dem Gerät die Selbstheilung bei hängendem WLAN.
- Deep Sleep: Pin-Blocker beseitigt, Rest in `deep-sleep-vorbereitung.md`.

---

## Werkzeugnotizen

- Die Messdaten kamen über den HA-Recorder: der Rohwert als Zustandsverlauf
  (`history`, 218 Einträge über 7 Tage), die Temperatur als Stundenmittel
  (`statistics`, 167 Zeilen). Für die Temperatur ist das Stundenmittel
  ausreichend genau — bei einem Tagesgang von 24 h kostet der Zeitversatz von
  einer halben Stunde nur den Kosinus von 2,5°, also 0,1 %.
- **Wiederholungen filtern:** Nach jedem `unavailable` veröffentlicht HA
  denselben Wert erneut mit neuem Zeitstempel. Ungefiltert wären das im
  Datensatz 20 Scheinmesspunkte gewesen, alle in den kurzen Zeitfenstern rund um
  Neustarts — also nicht zufällig verteilt. Doppelte Werte deshalb verwerfen.
- Der `esphome config`-Vergleich vorher/nachher braucht ein Entfernen der
  ANSI-Codes (`sed 's/\x1b\[[0-9;]*m//g'`), sonst rauscht der Diff.
