# Sessionbericht 12.08.2026 — Plausibilitätsfenster gegen Ausreißer

**Auftrag:** die Gewichtsstatistik von Ausreißern unter 0 kg und über 150 kg
bereinigen.

Das zerfällt in zwei Dinge, die nichts miteinander zu tun haben:

1. **Dass keine neuen dazukommen** — eine Änderung an der Firmware. Erledigt,
   geprüft, in diesem Bericht beschrieben.
2. **Die, die schon in der Langzeitstatistik stehen** — eine Änderung an der
   Recorder-Datenbank von Home Assistant. Vollständig aufgenommen, aber
   **nicht ausgeführt**; die Begründung steht unten unter „Die Altlast in der
   Statistik".

---

## 1. Warum das überhaupt wehtut

`sensor.waage_eg_gewicht` hat `state_class: measurement`. Damit gilt:

| | Aufbewahrung |
|---|---|
| Zustandsverlauf (`states`) | ~10 Tage, danach räumt der Recorder auf |
| Langzeitstatistik (`statistics`) | **für immer** |

Ein Ausreißer im Zustandsverlauf verschwindet also von selbst. Derselbe
Ausreißer in der Statistik bleibt und verdirbt Stunden-, Tages-, Wochen- und
Monatswerte gleichermaßen, weil die gröberen Perioden aus den Stundenzeilen
gerechnet werden.

**Das ist am 11.08.2026 real passiert.** Während der kaputten Kalibrierung
(Faktor +753,58 statt −18.000…−21.000) gingen zwischen 20:27 und 20:33 Uhr
Ortszeit nacheinander diese Werte nach HA:

```
20:25:49   18,4 kg
20:27:49   2299,4 kg
20:29:49   1035,3 kg
20:31:49  -3749,7 kg
20:32:54      0,0 kg
20:33:33    -48,6 kg
20:34:18   26,2 kg
```

Die Stundenzeile 18:00 UTC steht seitdem bei **Mittel +70,8 kg, Min −3.749,7,
Max +2.299,4**. Die tatsächliche Last in dieser Stunde lag bei rund 26–33 kg.
Jede `statistics-graph`-Karte skaliert seither auf ±3.750 kg, die echte
Messreihe ist darin eine flache Linie.

---

## 2. Was gebaut wurde

### `packages/waage-grenzen.h` (neu)

Eine Funktion, `waage_gewicht_plausibel(kg, untergrenze, obergrenze)`. Wie
`waage-temperatur.h` und `waage-mittelwert.h`: reine Zahlen, kein `id()`,
deshalb mit g++ prüfbar.

Zwei Festlegungen stecken darin:

- **NAN ist nie plausibel.** Es gibt nichts zu veröffentlichen.
- **Obergrenze ≤ Untergrenze schaltet die Prüfung ab.** Damit kommt ein Stock
  ohne Fenster aus, ohne dass jemand den Code anfasst. Bewusst so herum
  geschrieben (`if (!(obergrenze > untergrenze))`), dass auch NAN-Grenzen die
  Prüfung *ab*schalten — eine kaputte Konfiguration soll nicht jeden Messwert
  verschlucken.

### `packages/waage-basis.yaml`

- **Neue substitutions** `plausibel_kg_min: "0"` und `plausibel_kg_max: "150"`.
  Pro Stock überschreibbar. Compile-Zeit-Konstanten, bewusst **keine**
  Number-Entity: eine Number mit `restore_value: true` fordert Speicherplatz
  in den Preferences an und würde beim Flash die Kalibrierung kosten.
- **`waage_gewicht`** prüft als letzte Station vor HA und gibt bei
  Unplausiblem nichts zurück, zählt hoch und loggt mit Rohwert.
- **Neuer Zähler „Gewicht verworfen"** (`entity_category: diagnostic`, ohne
  `state_class`). Ohne ihn wäre das Verwerfen unsichtbar: die Entity behält
  ihren letzten Wert, und das sieht in HA aus wie eine ruhige Waage.
- **Neues Global `erste_messung_erfolgt`** ersetzt im Taktgeber die Prüfung
  `isnan(id(waage_gewicht).state)`.

Der letzte Punkt ist der einzige nicht offensichtliche. Der Taktgeber hat eine
Anlaufregel: *solange noch nie ein Gewicht veröffentlicht wurde, jede Minute
neu versuchen.* Sie fragte bisher, ob die Entity leer ist. Seit dem Fenster
kann die Entity aber auch **dauerhaft** leer sein — genau dann, wenn die
Kalibrierung kaputt ist. Ohne den Ersatz hätte die Waage in dieser Lage für
immer jede Minute ein Messfenster abgeschlossen und alle Diagnose-Entities
veröffentlicht: der meiste Funk und die meiste Rechnung ausgerechnet in dem
Zustand, in dem sie ohnehin nicht misst. Das neue Global wird gesetzt, sobald
ein Rohwert-Mittel vorliegt — also unabhängig davon, ob das Gewicht durch die
Prüfung kommt.

Beide neuen Globals haben `restore_value: no` und fordern damit keinen
Preferences-Speicher an; nach der Herleitung in `CLAUDE.md` sollte dieser Flash
die Kalibrierung also **nicht** kosten. Die Herleitung stammt aus dem
Quelltext, nicht aus einem Test am Gerät — **nach dem Flash trotzdem prüfen.**

### Warum verwerfen und nicht kappen

Ein auf 150,0 kg gekappter Wert wäre eine Behauptung über das Volk, die genauso
falsch ist wie die 2.299 — nur unauffälliger, und er stünde dann wieder für
immer in der Statistik. Ein fehlender Wert ist ehrlich.

### Was weiterhin gesendet wird

Rohwert, Rohwert-Streuung, Temperatur, Temperatur Mittel, Kalibrierfaktor,
„Kalibriert bei"/„mit", Temperaturkorrektur — alle unverändert. Nur das
Gewicht hängt am Fenster. Ohne die Diagnosewerte ließe sich nicht klären,
*warum* das Gewicht fehlt, und der Rohwert bleibt die Grundlage jeder
Nachmessung des Temperaturkoeffizienten.

---

## 3. Der Preis der Untergrenze 0

Das ist die einzige Stelle, an der das Fenster echte Messwerte kostet, und sie
ist es wert, hier zu stehen:

**Eine frisch tarierte Waage kann nicht mehr −0,1 kg anzeigen.** Steht der
Stock bei null und rauscht die Anzeige um die Null, fällt die untere Hälfte des
Rauschens aus.

Zwei Dinge entschärfen das:

- Werte zwischen −0,05 und 0 sind nicht betroffen. Sie runden auf −0,0 und
  werden schon in der Anzeige zu 0,0 (die Zeile gab es vorher schon, sie sollte
  „−0.0" in HA verhindern).
- Im Betrieb liegt immer eine Beute auf. Die Untergrenze greift nur, wenn
  jemand die Waage leer tariert hat.

Ausgerechnet in `tests/`, Punkt 11, bei 14 g Rauschen (= 300 counts je
Sekundenwert, dieselbe Annahme wie in Punkt 10, und das ist der *ungefilterte*
Fall — ein 6-h-Fenstermittel rauscht erheblich weniger):

| Stock steht bei | verworfene Werte |
|---|---|
| 0,0 kg | 0,01 % |
| 0,1 kg | 0 % |
| 0,5 kg | 0 % |
| 2,0 kg | 0 % |

Wer die Null-Umgebung sauber sehen will — etwa für eine Driftmessung mit
leerer, tarierter Waage — setzt in der Stock-Datei `plausibel_kg_min: "-5"`.

---

## 4. Prüfungen

**Aufgelöste Konfiguration, vorher/nachher, alle drei Stöcke:**

```bash
esphome config waage-eg.yaml | sed 's/\x1b\[[0-9;]*m//g' > vorher.txt
# ... Änderung ...
diff vorher.txt nachher.txt
```

Ergebnis: 66 geänderte Zeilen, für `waage-eg`, `waage-stock2` und
`waage-stock3` **zeichengleich identisch**. Kein bestehender Entity-Name, kein
bestehendes Global, keine `restore_value`-Reihenfolge angetastet. Die neuen
Zeilen sind ausschließlich: die zwei substitutions, der Include, die zwei
Globals mit `restore_value: false`, die Prüfung in der Gewichts-Lambda, der
neue Sensor, sein `update()` im Veröffentlichungsskript und die geänderte
Anlaufbedingung.

**Prüfprogramm:**

```bash
cd tests
g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

**4071 Prüfungen, 0 Fehler** (vorher 4045; Punkt 11 ist neu). Darin unter
anderem die vier Ausreißer vom 11.08. als Testfälle und die Reproduktion der
halben Kalibrierung: Span auf 600 counts geschrumpft (die Span-Prüfung im
Kalibrier-Button greift ab 500 nicht mehr) → Anzeige **−2.311,7 kg** → wird
verworfen. Die Größenordnung passt zu dem, was am 11.08. wirklich zu sehen war.

---

## 5. Die Altlast in der Statistik

Aus dem Recorder ausgelesen (`sensor.waage_eg_gewicht`, alle Stundenzeilen der
gesamten vorhandenen Statistik, 30.07.–12.08.2026). **Zwölf Stundenzeilen**
enthalten Werte außerhalb von 0…150 kg:

| Stundenzeile (UTC) | Ortszeit | Mittel | Min | Max | Auslöser |
|---|---|---|---|---|---|
| 2026-08-03 10:00 | 12:00 | −3,10 | −5,50 | 2,20 | Kalibrierverlust 03.08. |
| 2026-08-03 11:00 | 13:00 | 0,51 | −5,50 | 2,20 | dito |
| 2026-08-03 14:00 | 16:00 | 2,05 | −3,00 | 2,30 | Neukalibrierung 16:36/16:41 |
| 2026-08-10 12:00 | 14:00 | 1,99 | −5,50 | 2,20 | Neukalibrierung 15:01/15:03 |
| 2026-08-10 13:00 | 15:00 | 1,10 | −1,60 | 2,20 | dito |
| 2026-08-10 19:00 | 21:00 | −0,58 | −5,70 | 1,80 | Tara-/Kalibrierversuche |
| 2026-08-10 20:00 | 22:00 | 2,89 | −5,70 | 10,60 | dito |
| 2026-08-11 10:00 | 12:00 | 24,64 | −0,90 | 27,60 | Tara |
| 2026-08-11 11:00 | 13:00 | 13,87 | −3,00 | 26,20 | Kalibrierversuche |
| 2026-08-11 12:00 | 14:00 | 21,28 | −9,40 | 40,10 | dito |
| 2026-08-11 16:00 | 18:00 | 21,48 | −4,00 | 39,10 | Neukalibrierung 18:33/18:34 |
| **2026-08-11 18:00** | **20:00** | **70,77** | **−3.749,70** | **2.299,40** | **kaputte Kalibrierung** |

Nur die letzte Zeile überschreitet 150 kg. Die übrigen elf sind
Unterschreitungen von 0, alle im Bereich −9,4 bis −0,9 kg, und alle stammen
aus Kalibrier- oder Tara-Vorgängen — also genau aus den Sekunden, die das neue
Fenster künftig auffängt.

### Warum sie stehen geblieben sind

Home Assistant hat **keine Schnittstelle, um einzelne Statistikzeilen zu
löschen.** `recorder/clear_statistics` löscht die komplette Reihe eines
Sensors (zwei Wochen brauchbarer Daten mit), `recorder.purge_entities` räumt
nur den Zustandsverlauf auf und lässt die Statistik unberührt. Bleibt
`recorder/import_statistics`: damit lassen sich bestehende Stundenzeilen
**überschreiben** — löschen aber nicht.

Überschreiben heißt: die Zeile mit einem neu gerechneten Wert ersetzen. Dabei
sind beim Nachrechnen zwei Dinge aufgefallen, die die Sache heikel machen:

1. **Home Assistant mittelt zeitgewichtet, nicht arithmetisch.** Nachgeprüft
   an der Zeile 2026-08-10 12:00: der arithmetische Mittelwert der
   aufgezeichneten Zustände ergibt 1,47, das Mittel der 5-Minuten-Töpfe 0,55 —
   in der Datenbank steht 1,9944. Zeitgewichtet über die Stunde kommen
   1,9947 heraus. Ein Nachbau muss also die Haltedauer jedes Zustands und den
   aus der Vorstunde übernommenen Anfangszustand berücksichtigen, sonst
   schreibt er still falsche Geschichte.
2. **Für die drei Zeilen vom 03.08. gibt es keine Rohdaten mehr.** Der
   Zustandsverlauf ist nach ~10 Tagen weg. Jeder Ersatzwert dafür wäre
   erfunden.

Eine Zeile mit einer erfundenen Zahl zu überschreiben ist keine Bereinigung,
sondern eine unauffälligere Falschaussage — genau das, was oben schon gegen
das Kappen spricht. Und weil ein Überschreiben der Statistik nicht rückgängig
zu machen ist, ist das eine Entscheidung, die nicht nebenbei fallen sollte.
Die Tabelle oben enthält deshalb bewusst die **alten** Werte: sie ist zugleich
die Sicherung, falls überschrieben wird und es schiefgeht.

### Die drei möglichen Wege

| Weg | Wirkung | Preis |
|---|---|---|
| **Nichts tun** | Die zwölf Stunden bleiben, ab dem Flash kommt nichts Neues dazu | Die Statistik von 03.–11.08. bleibt unbrauchbar, die Skalierung jeder Statistikkarte auch |
| **Neun Zeilen nachrechnen** (10.08./11.08.) und per `recorder/import_statistics` überschreiben, dabei zeitgewichtet und nur über die Werte innerhalb 0…150 kg | Die schlimmste Zeile (+70,8 / ±3.750) verschwindet, die Skalierung stimmt wieder | Die drei Zeilen vom 03.08. bleiben (−5,5 kg, unauffällig). 5-Minuten-Statistik bleibt ~10 Tage schmutzig, räumt sich dann selbst auf |
| **Die ganze Reihe löschen** (`recorder/clear_statistics`) und aus dem Zustandsverlauf neu aufbauen | Alles sauber | Nur die letzten ~10 Tage lassen sich rekonstruieren, alles davor ist endgültig weg |

**Empfehlung: Weg 2.** Er beseitigt den einzigen Ausreißer, der die Auswertung
wirklich unbrauchbar macht, und stützt sich dabei ausschließlich auf noch
vorhandene Rohdaten. Die verbleibenden −5,5 kg vom 03.08. verzerren ein
Stundenmittel um wenige Gramm und sind in keiner Grafik zu sehen.

---

## 6. Offene Punkte

- **Flashen.** Die Änderung wirkt erst danach. Anschließend, wie nach jedem
  Flash: **Kalibrierfaktor prüfen** (Erwartung −18.000 bis −21.000; aktuell
  −20.755,9) und **„Kalibriert bei"** (aktuell 22,2 °C) auf „nicht leer".
  Beide neuen Globals sind `restore_value: no`, es sollte also nichts
  passieren — geprüft ist das erst am Gerät.
- **Entscheidung zur Altlast** (Abschnitt 5). Ohne sie bleibt der Auftrag zur
  Hälfte offen.
- **Nach dem Flash einmal „Gewicht verworfen" ansehen.** Steht dort nach ein
  paar Intervallen etwas anderes als 0, obwohl nicht kalibriert wurde, stimmt
  etwas an der Kalibrierung nicht.
- Unverändert offen aus den Vorsessions: `input_number.leergewicht_beute` und
  `input_number.mindestgewicht_mit_futter` stehen weiter auf 0,0 kg;
  Kalibrier-Sperre fehlt noch bei `Bienen: Futtervorrat kritisch`.
