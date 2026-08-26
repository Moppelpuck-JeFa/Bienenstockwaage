# Sessionbericht 26.08.2026 — Lastschalter IRLML6402

Kurze Sitzung mit einem Ergebnis: der HX711-Lastschalter schaltet jetzt
wirklich. Vorgeschichte in [Sessionbericht 19.08.](sessionbericht-2026-08-19.md),
Abschnitt 2 und in [deep-sleep-vorbereitung.md](deep-sleep-vorbereitung.md) 9.2.

## 1. Warum der BS250 weg musste

Er ist **kein Logic-Level-Typ**: V_GS(th) mit −1 bis −3,5 V spezifiziert,
R_DS(on) bei −10 V. Am 3,3-V-Rail stehen aber nur −3,3 V zur Verfügung. Ein
Exemplar am oberen Rand der Streuung ist damit schlicht **aus** — das ist kein
Streuungsrisiko, das man ausmisst, sondern Betrieb außerhalb der Spezifikation.

Ersetzt durch **IRLML6402**: V_GS(th) max −1,2 V (typ. −0,55 V), also rund 2 V
Reserve. R_DS(on) 80 mΩ bei −2,5 V — bei 6 mA Laststrom ohnehin bedeutungslos
(0,5 mV Abfall).

**An der Konfiguration ändert sich nichts.** Gleiche Topologie, gleiche Logik,
`inverted: true` bleibt richtig. Kein Flash nötig.

## 2. Die Gegenprobe, und warum die erste falsch war

Am 18.08. hatte ich die Boot-Warnung als Abschaltbeweis gelesen:

```
[W][hx711:063]: HX711 DOUT pin not high after reading (data 0x0)!
```

Falsch. `HX711Sensor::setup()` ruft `read_sensor_()` einmal auf, bevor
irgendetwas bereit ist — die Meldung kommt bei **jedem** Start, unabhängig vom
Lastschalter. Die umgebenden `[C]`-Zeilen waren der Boot-Konfigurationsdump;
das hätte auffallen müssen.

Richtig ist der Vergleich der **laufenden** Werte, über
`switch.garten_stockwaage_hx711_versorgung` geschaltet:

| | Versorgung an | Versorgung aus | wieder an |
|---|---|---|---|
| `Got value`-Zeilen je 10 s | 63 | **0** | 63 |
| HX711-Warnungen je 10 s | 0 | **64** | 0 |

Rohwert vor dem Abschalten −737.390, nach dem Wiedereinschalten −736.245 —
rund 1.150 counts (≈ 55 g) daneben nach nur 25 s Abschaltzeit. Erwartbar, und
genau der Grund für die 90 s Einschwingzeit aus 9.7.

**Damit ist der Schalter in beide Richtungen belegt.**

## 3. Was das freischaltet

Bisher war der Lastschalter wirkungslos, der HX711 lief durch. Erst jetzt:

- verschwinden die **~5,8 mA Grundlast** von HX711 und Brücke im Schlaf — das
  ist der ganze Sinn des Tiefschlafs
- wird die **Einschwingzeit real**: der HX711 startet bei jedem Aufwachen kalt

## 4. Offene Punkte

- **Ruhestrom im Deep Sleep messen**, nicht im Wachbetrieb. Erst diese Zahl
  sagt, ob aus Variante C (~17 Tage) wirklich Variante D (~200 Tage) wird.
  Erwartung: der schlafende ESP32 allein, rund 0,01 mA.
- **Einschwingzeit gegenprüfen.** Die 90 s stammen vom 18.08.; jetzt, wo der
  HX711 bei jedem Aufwachen wirklich kalt startet, über mehrere echte Zyklen
  nachsehen. Kriterium: bleibt `Rohwert Streuung` im Bereich einiger hundert
  counts, stimmt es. Springt sie auf Zehntausende, war 90 s zu kurz.
- **Messintervall steht auf 30 min.** Für die 72 h sind **4320** einzustellen;
  die Schlafdauer folgt automatisch und ist im Boot-Dump nachlesbar.
- **Gegenprobe zum Softwareschalter** steht weiter aus: beide Schalter aus,
  dann muss das Gerät binnen einer Sekunde einschlafen.
