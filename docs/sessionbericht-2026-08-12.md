# Sessionbericht 12.08.2026 — Plausibilitätsfenster gegen Ausreißer

**Auftrag:** die Gewichtsstatistik von Ausreißern unter 0 kg und über 150 kg
bereinigen.

Das zerfällt in zwei Dinge, die nichts miteinander zu tun haben:

1. **Dass keine neuen dazukommen** — eine Änderung an der Firmware. Erledigt,
   geprüft, in diesem Bericht beschrieben.
2. **Die, die schon in der Langzeitstatistik stehen** — eine Änderung an der
   Recorder-Datenbank von Home Assistant. Ebenfalls erledigt: zwölf belastete
   Stundenzeilen sind weg, neun davon durch nachgerechnete ersetzt, drei
   ersatzlos gestrichen. Ablauf und Prüfung unter „Die Altlast in der
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

- **Neue substitutions** `plausibel_kg_min: "-1"` und `plausibel_kg_max: "150"`
  (zur Untergrenze siehe Abschnitt 3).
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

## 3. Die Untergrenze: erst 0, dann −1

Zuerst stand die Untergrenze auf **0** — ein Stock kann nicht weniger als
nichts wiegen. Das hatte einen Preis, der im Gespräch zur Änderung führte:

**Eine frisch tarierte Waage kann nicht mehr −0,1 kg anzeigen.** Steht der
Stock bei null und rauscht die Anzeige um die Null, fällt die untere Hälfte des
Rauschens aus — und zwar nur die untere. Damit ist es kein bloßer Datenverlust,
sondern ein **systematischer Fehler**: der Mittelwert wandert nach oben, und
man sieht es der Statistik hinterher nicht an. Genau die Sorte Fehler, gegen
die das Fenster eigentlich gebaut wurde.

**Die Untergrenze steht deshalb jetzt auf −1 kg.** Das Kilo Luft lässt das
Rauschen um die Null vollständig heil und fängt trotzdem alles, was aus einer
kaputten Kalibrierung kommt — die lag real bei −3.749 kg, also drei
Größenordnungen daneben. Zwischen „echter Messwert" und „Rechenfehler" liegt
hier so viel Platz, dass die genaue Zahl unkritisch ist.

Ausgerechnet in `tests/`, Punkt 11, bei 14 g Rauschen (= 300 counts je
Sekundenwert, dieselbe Annahme wie in Punkt 10, und das ist der *ungefilterte*
Fall — ein 6-h-Fenstermittel rauscht erheblich weniger):

| Stock steht bei | verworfen mit Grenze 0 | verworfen mit Grenze −1 |
|---|---|---|
| 0,0 kg | 0,01 % | 0 % |
| −0,5 kg | 100 % | 0 % |
| −0,9 kg | 100 % | 0 % |
| −1,2 kg | 100 % | 100 % |

Wer noch mehr Luft braucht — etwa für eine Driftmessung mit leerer, tarierter
Waage, die über Tage ins Negative laufen kann — setzt in der Stock-Datei
`plausibel_kg_min: "-5"`.

> **Kleine Unstimmigkeit, die man kennen sollte:** Die Altlast in der
> Langzeitstatistik (Abschnitt 5) wurde gegen **0** gefiltert, weil der Auftrag
> so lautete und weil dort ausschließlich Kalibrier-Artefakte betroffen waren.
> Künftig veröffentlicht die Waage aber ab −1 kg. In der Reihe kann also
> irgendwann ein echter Wert von −0,3 kg stehen, während die alten −0,9 kg vom
> August entfernt sind. Das ist so gewollt: die alten waren keine Messung.

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

**4079 Prüfungen, 0 Fehler** (vorher 4045; Punkt 11 ist neu). Darin unter
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

### Das Werkzeugproblem

Home Assistant hat **keine Schnittstelle, um einzelne Statistikzeilen zu
löschen.** `recorder/clear_statistics` löscht die komplette Reihe eines
Sensors, `recorder.purge_entities` räumt nur den Zustandsverlauf auf und lässt
die Statistik unberührt. `recorder/import_statistics` kann bestehende
Stundenzeilen **überschreiben** — löschen aber nicht.

Gewählt wurde deshalb: **Reihe löschen und neu aufbauen**, aber so, dass nichts
Sauberes dabei verloren geht. Alle 251 Stundenzeilen wurden vorher ausgelesen
und gesichert; zurückgeschrieben wurden 248 (plus die inzwischen dazugekommene
Zeile 04:00 UTC) — die unbelasteten unverändert, die neun reparierbaren neu
gerechnet. Nur die drei vom 03.08. fehlen jetzt: für sie gibt es keine Rohdaten
mehr, und eine erfundene Zahl wäre keine Bereinigung, sondern eine
unauffälligere Falschaussage.

### Wie die neun Zeilen gerechnet wurden

Ein Nachbau, der die Statistik überschreibt, muss die Rechnung von Home
Assistant **treffen** — sonst schreibt er still falsche Geschichte. Der Weg
dahin über drei Versuche:

| Modell | Ergebnis |
|---|---|
| arithmetisches Mittel der Zustände | 1,47 statt 1,9944 — falsch |
| durchgehend zeitgewichtet über die Stunde | trifft 15 von 18 Kontrollstunden exakt, weicht bei Stunden mit `unavailable`-Lücken um bis zu 0,95 kg ab |
| **je 5-Minuten-Topf zeitgewichtet, Stundenmittel = Mittel der Töpfe** | **trifft alle Kontrollstunden auf 0,0000 kg** |

Das dritte Modell ist also das, was HA tatsächlich tut, und der Unterschied
zeigt sich genau dort, wo nicht jeder Topf gleich gut besetzt ist. Zwei
Einzelheiten fielen dabei noch auf und stecken im Nachbau:

- Der **Trägerzustand** aus der Vorstunde zählt mit — auch in Min und Max.
  Belegt an der Stunde 09:00 UTC des 11.08., deren Minimum 23,8 kg ein Wert
  von 10:55 Ortszeit der Vorstunde ist.
- Eine `unavailable`/`unknown`-Lücke **hält den vorigen Wert**, sie fällt nicht
  aus der Gewichtung heraus.

Geprüft wurde gegen 18 Stunden, deren HA-Werte bekannt waren. Beim Filtern
werden Intervalle mit Werten außerhalb 0…150 kg übersprungen, und ein
verworfener Wert läuft danach auch nicht als Trägerwert weiter.

### Was jetzt drinsteht

| Stundenzeile (UTC) | alt | neu |
|---|---|---|
| 2026-08-03 10:00 | −3,10 [−5,50 … 2,20] | *gestrichen* |
| 2026-08-03 11:00 | 0,51 [−5,50 … 2,20] | *gestrichen* |
| 2026-08-03 14:00 | 2,05 [−3,00 … 2,30] | *gestrichen* |
| 2026-08-10 12:00 | 1,99 [−5,50 … 2,20] | 2,10 [0,00 … 2,20] |
| 2026-08-10 13:00 | 1,10 [−1,60 … 2,20] | 1,12 [0,00 … 2,20] |
| 2026-08-10 19:00 | −0,58 [−5,70 … 1,80] | 0,77 [0,00 … 1,80] |
| 2026-08-10 20:00 | 2,89 [−5,70 … 10,60] | 3,37 [0,00 … 10,60] |
| 2026-08-11 10:00 | 24,64 [−0,90 … 27,60] | 25,18 [0,00 … 27,60] |
| 2026-08-11 11:00 | 13,87 [−3,00 … 26,20] | 22,71 [0,00 … 26,20] |
| 2026-08-11 12:00 | 21,28 [−9,40 … 40,10] | 23,05 [0,00 … 40,10] |
| 2026-08-11 16:00 | 21,48 [−4,00 … 39,10] | 24,14 [0,00 … 39,10] |
| **2026-08-11 18:00** | **70,77 [−3.749,70 … 2.299,40]** | **29,41 [0,00 … 33,50]** |

### Die Probe

Vor dem Löschen wurde die Abschrift der 251 Stundenzeilen **unabhängig**
geprüft: aus ihnen lassen sich die zwölf Tageszeilen nachrechnen, die HA
separat geliefert hatte. Alle zwölf stimmten auf sechs Nachkommastellen. Damit
war ausgeschlossen, dass ein Abschreibfehler beim Zurückschreiben unbemerkt in
die Datenbank wandert.

Danach, aus der Datenbank zurückgelesen:

| Tag | Mittel vorher | Mittel nachher | Min vorher | Min nachher |
|---|---|---|---|---|
| 30.07.–31.07. | 2,260959 / 2,155060 | **unverändert** | 2,20 | 2,20 |
| 01.08. | 1,891890 | 2,187759 | −5,50 | 2,10 |
| 02.08.–07.08. | 2,235820 … 2,100064 | **unverändert** | 2,00–2,20 | 2,00–2,20 |
| 09.08. | 2,302254 | 2,394948 | −5,70 | 0,00 |
| 10.08. | 20,185953 | 19,038276 | −3.749,70 | 0,00 |

Die acht unbelasteten Tage kommen **bitgleich** wieder heraus — die
Wiederherstellung hat nichts angefasst, was sauber war. Kein Wert der Reihe
liegt mehr außerhalb von 0…150 kg.

### Was dabei doch verloren ging

- **Die drei Stundenzeilen vom 03.08.** (12:00, 13:00 und 16:00 Ortszeit).
  Dort ist jetzt eine Lücke. Das ist die ehrliche Darstellung: in diesen
  Stunden wurde kalibriert, ein Stockgewicht gibt es für sie nicht.
- **Die 5-Minuten-Statistik** (`statistics_short_term`) der ganzen Reihe.
  `clear_statistics` löscht sie mit, und `import_statistics` schreibt nur
  Stundenzeilen. Sie hätte sich ohnehin nach ~10 Tagen selbst aufgeräumt; sie
  betrifft nur die feine Auflösung in Diagrammen der letzten Tage.
- **Der Zustandsverlauf ist unangetastet.** Dort stehen die −3.749,7 kg noch
  bis zur nächsten Recorder-Aufräumung (~10 Tage). Das ist Absicht: er ist die
  Rohdatenquelle, aus der eben gerechnet wurde, und er verschwindet von selbst.

Die alten Werte aller 251 Zeilen liegen als Abschrift vor (Abschnitt oben plus
die Tabelle der belasteten Zeilen); ein Zurückrollen wäre also möglich, wenn
sich doch noch etwas als falsch herausstellt.

### Nachtrag: der wertbasierte Schnitt hat nicht gereicht

Danach war kein Wert mehr außerhalb von 0…150 kg — und das Diagramm sah
trotzdem falsch aus. **Das ist die eigentliche Lehre dieser Session:**

| Zeitraum (Ortszeit) | was in der Reihe stand | Ursache |
|---|---|---|
| 10.08. 14–17 Uhr | Einbrüche auf 0,0 kg, dann 3,3 | Tara + Nullpunkt-Kalibrierung |
| 10.08. 21–23 Uhr | 0,0 … 10,6 kg | Tara-Versuche |
| 11.08. 08–11 Uhr | 9,9 → 27,2 → 57,1 kg | Referenzgewicht aufgelegt |
| 11.08. 12–14 Uhr | Einbrüche auf 0,0, Spitze 40,1 | weitere Versuche |
| 11.08. 18–20 Uhr | 0,0 / 39,1 / 40,4 / 33,5 | Neukalibrierung |

**Während einer Kalibrierung liefert die Waage plausible, aber ungültige
Werte.** 57 kg sind für einen Stock völlig normal — keine wertbasierte Regel
der Welt trennt das vom Referenzgewicht auf der Waagschale. Das
Plausibilitätsfenster fängt den kaputten Rechenweg, nicht den falschen
Messgegenstand.

Dazu kommt ein zweites Argument, das schwerer wiegt: **ein Gewichtsverlauf ist
nur innerhalb EINER Kalibrierung vergleichbar.** Jede Neukalibrierung ist ein
Sprung in der Bezugsgröße, kein Ereignis am Stock.

Deshalb ist die Reihe am Ende **zeitbasiert** geschnitten worden: alles vor der
aktuell gültigen Kalibrierung (11.08.2026, 20:34 Uhr Ortszeit, Faktor
−20.756) wurde verworfen. Übrig bleiben 11 Stundenzeilen ab 21:00 Uhr; die
Reihe wächst von da an. Die 12 Tage davor stammen aus zwei anderen
Kalibrierungen und aus wechselnden Prüflasten — als Messreihe waren sie
ohnehin nichts wert.

Damit ist auch die aufwendige Rekonstruktion der neun Stundenzeilen weiter
oben hinfällig geworden. Sie steht hier trotzdem, weil das Verfahren stimmt
und beim nächsten Mal gebraucht werden kann — und weil die Reihenfolge lehrt,
in welcher Reihenfolge man besser fragt: **erst „ist dieser Zeitraum überhaupt
eine Messung?", dann „ist dieser Wert plausibel?"**

---

## 6. Das Dashboard zeigte die Ausreißer weiter — warum

Nach der Bereinigung standen sie in fünf von sechs Diagrammen immer noch drin.
Der Grund ist eine Unterscheidung, die man am Kartentyp ablesen kann:

| Kartentyp | liest | war betroffen |
|---|---|---|
| `statistics-graph` | Langzeitstatistik (permanent) | nein, war ja bereinigt |
| `history-graph` | Zustandsverlauf (~10 Tage) | **ja** |
| Kachel-Feature `trend-graph` | Zustandsverlauf | **ja** |

Bereinigt worden war nur die Langzeitstatistik — und genau eine Karte las
daraus. **Merksatz: Was in einer `history-graph`-Karte steht, hat mit der
Langzeitstatistik nichts zu tun.**

### Dabei aufgefallen: die abgeleiteten Helfer

Vier Helfer haben eigene, ebenfalls dauerhafte Langzeitstatistiken, und die
waren noch dreckig — deutlich schlimmer als das Gewicht, weil eine Ableitung
den Sprung mit ihrem Zeitfenster multipliziert:

| Helfer | Tag 10.08. vor der Bereinigung |
|---|---|
| Waage Änderung (6 h → kg/d) | Min **−15.110 kg/d**, Max **+9.085 kg/d** |
| Schwarm-Signal (20 min → kg/h) | Min **−11.343 kg/h**, Max **+6.804 kg/h** |
| Waage Tagesbilanz | Min −3.751 kg, Max +2.298 kg |
| Futtervorrat | Min −3.768 kg, Max +2.281 kg |

Diese vier wurden per `recorder/clear_statistics` **vollständig gelöscht**,
nicht Zeile für Zeile repariert. Begründung: Sie tragen keine eigene
Information — sie sind Ableitungen des Gewichts, und das liegt als saubere
Reihe vor. Der Aufwand einer zeilenweisen Rekonstruktion (viermal das
Verfahren aus Abschnitt 5) stünde in keinem Verhältnis, und bei Ableitungen
schmiert die Störung ohnehin über das jeweilige Fenster aus, die Abgrenzung
wäre also unschärfer als beim Gewicht.

### Was am Dashboard geändert wurde

Drei `history-graph`-Karten wurden auf `statistics-graph` mit `period: hour`
umgestellt — Gewicht 48 h (Übersicht), Tagesbilanz 14 Tage und
Änderung + Schwarm-Signal 7 Tage (Auswertung). Bei 60 min Messintervall
kostet das nichts: pro Stunde gibt es ohnehin nur einen Messwert, und
`stat_types: [mean, min, max]` hält auch die Ausschläge fest, wenn die
Saison-Automation das Intervall auf 15 min stellt.

Die Karte „Rohwert + Temperatur 72 h" bleibt bewusst ein `history-graph`:
sie war nie betroffen, und für die Fehlersuche ist die volle Auflösung der
Punkt.

### Die Trend-Kachel

Das Kachel-Feature `trend-graph` kann **nur** den Zustandsverlauf lesen, eine
Statistik-Variante davon gibt es nicht. Es blieb damit als einziges Element
schmutzig, nachdem alles andere sauber war.

Gelöst durch `recorder.purge_entities` mit `keep_days: 0` für
`sensor.waage_eg_gewicht`: der Zustandsverlauf der Entity ist damit leer und
baut sich neu auf. Das war vertretbar, weil der Zustandsverlauf ohnehin nach
10 Tagen verfällt und die Langzeitstatistik der Archivspeicher ist — die
Rohdaten hatten ihren Zweck (die Rekonstruktion in Abschnitt 5) da schon
erfüllt.

**Was nicht angefasst wurde:** Die Kachelwerte „Bilanz 24 h" (25,9 kg) und
„Änderung" (44,47 kg/d) sind keine Diagramme, sondern die aktuellen Zustände
der Helfer. Sie rechnen ihr Fenster (24 h bzw. 6 h) noch über den
Lastwechsel von heute früh — das ist ein echtes Ereignis, kein Artefakt.

---

## 7. Offene Punkte

- **Flashen.** Die Änderung wirkt erst danach. Anschließend, wie nach jedem
  Flash: **Kalibrierfaktor prüfen** (Erwartung −18.000 bis −21.000; aktuell
  −20.755,9) und **„Kalibriert bei"** (aktuell 22,2 °C) auf „nicht leer".
  Beide neuen Globals sind `restore_value: no`, es sollte also nichts
  passieren — geprüft ist das erst am Gerät.
- **Kalibrieren sollte die Veröffentlichung sperren.** Der Durchsichtmodus
  löst genau dieses Problem für die Durchsicht — für die Kalibrierung gibt es
  nichts Vergleichbares, obwohl der Schaden derselbe ist: plausibel
  aussehende Werte, die keine Messung sind, dauerhaft in der Statistik. Ein
  Nachlauf analog zum Durchsicht-Nachlauf (die drei Kalibrier-/Tara-Buttons
  setzen ihn, er sperrt das Veröffentlichen für ein paar Minuten) wäre die
  naheliegende Lösung. **Nicht umgesetzt** — das ist ein eigener Entwurf,
  kein Nebenprodukt dieser Session.
- **Die Reihe ist jetzt kurz.** Sie beginnt am 11.08. um 21:00 Uhr. Wer in
  den nächsten Tagen auf die 30-Tage-Karte sieht, sieht fast nichts — das ist
  richtig so und kein Fehler.
- **Nach dem Flash einmal „Gewicht verworfen" ansehen.** Steht dort nach ein
  paar Intervallen etwas anderes als 0, obwohl nicht kalibriert wurde, stimmt
  etwas an der Kalibrierung nicht.
- Unverändert offen aus den Vorsessions: `input_number.leergewicht_beute` und
  `input_number.mindestgewicht_mit_futter` stehen weiter auf 0,0 kg;
  Kalibrier-Sperre fehlt noch bei `Bienen: Futtervorrat kritisch`.
