# Sessionbericht 11.08.2026 — Rohwerte über das Messintervall mitteln

**Auftrag:** „mittle alle rohwerte über das eingestellte messintervall um
genauer zu werden"

**Ergebnis:** Umgesetzt. Der veröffentlichte Wert ist jetzt der Mittelwert
aller Rohwerte seit der letzten Messung statt einer Momentaufnahme. Zwei neue
Diagnose-Entities. **Nicht geflasht** — geprüft nur mit `esphome config` und
dem g++-Test.

**Branch:** `claude/rohwerte-messintervall-mittel-ddd3ss`

---

## 1. Was vorher passierte

Der HX711 lief schon immer im Sekundentakt durch eine Filterkette
(Median 5 → gleitender Mittelwert über 12 Werte ≈ 60 s). Veröffentlicht wurde
davon aber nur der **Momentanwert im Sendeaugenblick**: bei 360 Minuten
Messintervall also ein 60-Sekunden-Fenster, und die übrigen 359 Minuten
Messdaten wanderten in den Papierkorb.

Von 4.320 gefilterten Werten je Intervall gingen 12 in die Zahl ein, die in HA
ankam. Das ist der Ansatzpunkt der Änderung.

## 2. Was jetzt passiert

Jeder gefilterte Rohwert (alle 5 s) und jeder Temperaturwert (alle 60 s) läuft
in laufende Summen-Globals. Beim Veröffentlichen werden daraus drei Werte
gerechnet — `mittel_rohwert`, `mittel_streuung`, `mittel_temperatur` — und das
Fenster beginnt neu.

**Alle Entities rechnen ab jetzt gegen diese drei Globals**, keine mehr direkt
gegen `hx711_raw_counts`. Damit beziehen sich Gewicht, Rohwert und
Temperaturkorrektur garantiert auf denselben Messzeitraum; vorher war das nur
deshalb der Fall, weil es überhaupt nur einen Momentanwert gab.

### Die Rechnung liegt in einem eigenen Header

`packages/waage-mittelwert.h`, aus demselben Grund wie
`packages/waage-temperatur.h`: reine Zahlen, kein `id()`, mit g++ durchrechenbar.
Zwei Funktionen, `waage_fenster_mittelwert()` und `waage_fenster_streuung()`.

Summiert wird nicht `x`, sondern `x − bezug`, wobei `bezug` der erste Wert des
Fensters ist. Grund: Der HX711 ist 24 bit breit und liefert bis zu ±8.388.608
counts. Bei 7 Tagen Messintervall kämen ~121.000 Werte zusammen; die direkte
Quadratsumme läge dann bei ~10¹⁸ und damit an der Grenze eines `double` — und
ausgerechnet die Streuung, die als Differenz zweier großer Zahlen entsteht,
würde darin untergehen. Mit dem Bezugswert sind die Summanden so groß wie das
Rauschen selbst. Punkt 9 des Tests prüft genau diesen Grenzfall: 120.960 Werte
bei 8 Mio counts, Mittelwert auf 0,5 counts genau, Streuung 299,8 statt der
eingespeisten 300,0.

## 3. Was das bringt

Simulation der **echten** Filterkette (Median 5 → gleitendes Fenster 12, mit
den korrekt überlappenden Ausgaben), 300 counts Rauschen je Sekundenwert,
konstante Last, 1 K/h Erwärmung, 100 Läufe je Zeile. Angegeben ist die
Reststreuung des angezeigten Gewichts, ungerundet:

| Intervall | vorher (Momentanwert) | jetzt (Fenstermittel) | Fenstermittel mit Momentan-Temperatur |
|---|---|---|---|
| 1 min | 2,1 g | 1,9 g | 1,9 g |
| 15 min | 2,2 g | 0,6 g | 4,2 g |
| 60 min | 2,1 g | 0,3 g | 16,5 g |
| 360 min | 2,2 g | 0,1 g | 97,7 g |

Nachvollziehbar über `tests/waage-temperatur-test.cpp`, Punkt 10.

**Einordnung, damit die Zahl nicht überinterpretiert wird:** Das ist der
*Rauschanteil*, und der war schon vorher der kleinste Posten. Kalibrierung,
Eckenfehler, Kriechen und der Temperaturrest von ~15 g bleiben unverändert. Die
Waage wird durch diese Änderung nicht auf 0,1 g genau — der Rauschanteil
verschwindet nur praktisch vollständig. Am ehesten sichtbar wird das in der
**Tagesbilanz**, wo sich zwei verrauschte Messwerte sonst zu doppeltem Rauschen
addieren.

Bei 1 Minute bringt die Mittelung fast nichts. Das ist erwartbar: dort *ist*
das Messintervall bereits das Filterfenster.

### Die dritte Spalte ist der eigentliche Fund

Die naheliegende Umsetzung — Rohwert mitteln, Temperaturkorrektur wie bisher
mit dem aktuellen Sensorwert — wäre **schlechter als der Ausgangszustand
gewesen**, und zwar um Größenordnungen: 97,7 g bei 6 h Intervall gegen 2,2 g
vorher.

Der Grund ist einfach, fällt aber nicht auf: Ein über 6 Stunden gemitteltes
Gewicht gehört zur *mittleren* Temperatur dieser 6 Stunden, nicht zu der am
Ende. Bei 1 K/h Erwärmung sind das 3 K Differenz und mit +32,5 g/K eben rund
98 g. Das Ergebnis hätte weiter völlig plausibel ausgesehen — genau die Sorte
Fehler, gegen die es diesen Test gibt.

Deshalb wird die Temperatur über **dasselbe** Fenster gemittelt. Sauber ist das,
weil die Kompensation linear ist: der Mittelwert der Einzelkorrekturen ist exakt
die Korrektur des Mittelwerts.

## 4. Zwei neue Diagnose-Entities

| Entity | Warum |
|---|---|
| `sensor.waage_eg_rohwert_streuung` | Ein gemittelter Rohwert sieht auch dann ruhig aus, wenn das Signal springt. Vorher war „springt der Rohwert um Tausende?" die **wichtigste** Fehlersuchregel; ohne Ersatz hätte die Mittelung sie ersatzlos gestrichen. |
| `sensor.waage_eg_temperatur_mittel` | Für die Nachmessung des Temperaturkoeffizienten. Seit „Rohwert" ein Intervallmittel ist, passt der Momentanwert von „Temperatur" zeitlich nicht mehr dazu — die Regression käme zu flach heraus. |

Beide sind `entity_category: diagnostic`. Bestehende Entities sind unverändert.

**Zur Streuung eine Warnung, die ins Lesen gehört:** Sie misst *alle* Einflüsse
im Fenster, nicht nur das Rauschen. Über 6 Stunden dominiert der Temperaturgang
— in der Simulation ergeben 300 counts Rauschen plus 1 K/h Erwärmung zusammen
rund 1.200 counts. Ein hoher Wert bei langem Intervall ist also normal. Zum
Beurteilen der Hardware kurz auf Intervall `1` gehen oder die Entity über Tage
mit sich selbst vergleichen.

## 5. Was das Fenster verwirft

Nicht jede Veröffentlichung darf ein Mittel sein. Fünf Fälle nehmen bewusst den
Momentanwert, alle über das Skript `messfenster_frisch`:

| Auslöser | Warum |
|---|---|
| „Jetzt messen" | Wer den Button drückt, hat gerade etwas aufgelegt und will nicht das Mittel der letzten Stunden sehen |
| Tara | Tara gilt ab jetzt; das alte Fenster verfälschte den nächsten Mittelwert um den halben Tara-Betrag. Nebeneffekt: die Anzeige steht danach exakt auf 0,0 kg, weil Tara und Anzeige denselben Rohwert benutzen |
| Kalibrieren 0 kg | Die gesammelten Werte stammen aus der alten Kalibrierung |
| Kalibrieren Referenzgewicht | Zusätzlich: im Fenster steckt die Zeit **vor** dem Auflegen — der Span käme zu klein heraus und der Kalibrierfaktor wäre still verkehrt |
| Intervallwechsel | Ein halb volles Fenster wäre eine Mischung aus altem und neuem Intervall |

Während **Durchsicht und Nachlauf** wird gar nicht erst gesammelt (Sperre am
`on_value` von HX711 und DS18B20 — im `interval:`-Block zu sperren hätte nicht
gereicht, die Werte laufen im 5-s-Takt auf). Der erste Wert nach dem Nachlauf ist
ebenfalls ein Momentanwert, sonst steckte eine abgenommene Honigzarge anteilig
darin.

**Nicht** verworfen wird bei einer Änderung des Temperaturkoeffizienten: dort
sollen die beiden Entities den zuletzt veröffentlichten Messwert mit der neuen
Zahl nachrechnen, und genau dafür müssen die `mittel_*`-Globals stehen bleiben.

## 6. Die Veröffentlichungsliste steht jetzt einmal

Die acht `update()`-Aufrufe standen an drei Stellen (Taktgeber, Nachlauf,
„Jetzt messen"). Beim Ergänzen eines Sensors blieb regelmäßig eine Stelle
zurück — der Sensor war dann nach einem Neustart dauerhaft leer, ohne dass
etwas fehlerhaft aussah. Das stand als Warnung sogar in `CLAUDE.md`.

Da diese Änderung zwei Sensoren hinzufügt, war das der Anlass, die Liste ins
Skript `messwerte_veroeffentlichen` zu ziehen. Wer künftig einen Sensor
ergänzt, trägt ihn nur noch dort ein.

## 7. Flash-Risiko: diesmal vermutlich keins

Die Änderung bringt **neun neue Globals** mit — genau das, was am 03.08. und am
10.08. die Kalibrierung gekostet hat. Deshalb nachgesehen, warum das eigentlich
passiert (`esphome/components/esp8266/preferences.cpp`, 2026.6.5):

> Der Speicherplatz eines `restore_value`-Globals ergibt sich aus der
> **Reihenfolge** der `make_preference()`-Aufrufe während des Setups, nicht aus
> seinem Namen. Der Namens-Hash (`md5(id)[:8]`) geht nur in die CRC ein.

Ein neues Global mit `restore_value: yes` verschiebt deshalb alles, was danach
allokiert wird; die CRC schlägt fehl und die betroffenen Werte fallen auf
`initial_value` zurück. Genau das Beobachtete.

**Ein Global mit `restore_value: no` ruft `make_preference()` gar nicht auf und
verschiebt nichts.** Alle neun neuen Globals sind so angelegt — nicht aus
Vorsicht, sondern weil ein angefangenes Messfenster über einen Neustart hinweg
fortzusetzen ohnehin falsch wäre (es vermengte Werte von vor und nach dem
Neustart, und dazwischen kann ein Flash die Kalibrierung gewechselt haben).

Die zwei neuen Sensoren sind Template-Sensoren und fordern ebenfalls keinen
Speicher an; die Reihenfolge der vorhandenen Anforderungen (5 restore-Globals,
4 Number-Entities) bleibt unberührt.

**Erwartung: Dieser Flash kostet die Kalibrierung nicht.** Das ist aus dem
Quelltext abgeleitet, nicht am Gerät getestet — **also trotzdem nach dem Flash
den Kalibrierfaktor und „Kalibriert bei" prüfen.**

## 8. Wie geprüft

**`esphome config`** für alle drei Stöcke, vorher/nachher verglichen (ANSI
entfernt). Am aufgelösten Ergebnis geändert haben sich nur:

- neun neue `globals`, alle `restore_value: false`
- zwei neue Sensoren (`rohwert_streuung`, `temperatur_mittel`)
- drei neue Skripte, die Lambda-Körper
- `accuracy_decimals` von „Rohwert": 0 → 1

**Kein bestehender `name:`, keine bestehende `id`, kein bestehendes Global
verändert.** Damit bleiben alle entity_ids in HA erhalten und Verlauf, Helfer,
Automationen und Dashboard hängen weiter an lebenden IDs.

**g++-Test:** 4.045 Prüfungen, 0 Fehler (vorher 4.028). Neu sind Punkt 9 (die
Mittelwertrechnung selbst, inklusive 7-Tage-Grenzfall) und Punkt 10 (die
Genauigkeitsaussage oben). Der Test bindet beide echten Header ein.

Zwei Nachbauten kamen dazu und müssen mitgezogen werden, wenn sich die YAML
ändert: das Messfenster (`on_value` des HX711 plus
`messfenster_abschliessen`) und die Filterkette.

> Beim Bauen von Punkt 10 zunächst mit einer Temperaturrampe von festem
> *Gesamthub* je Intervall gerechnet — das koppelte den Temperaturfehler an die
> Intervalllänge und machte die Spalten unvergleichbar. Ebenso fehlte anfangs
> der 60-s-Vorlauf der Filterkette, weshalb das 1-min-Intervall künstlich
> schlecht aussah (auf dem Gerät läuft die Kette durchgehend, ihr gleitendes
> Fenster ist beim Intervallstart längst gefüllt). Beides korrigiert; die
> Tabelle oben ist die Fassung nach beiden Korrekturen.

Ein vollständiger `esphome compile` war wie immer nicht möglich
(PlatformIO-Registry durch die Egress-Policy gesperrt).

## 9. Offene Punkte

- **Flashen und die zwei Pflichtprüfungen fahren** (Kalibrierfaktor im Bereich
  −18.000 bis −21.000, „Kalibriert bei" nicht leer). Erwartung siehe Abschnitt 7.
- **Dashboard:** „Rohwert Streuung" und „Temperatur Mittel" sind noch nirgends
  eingebunden. Die HA-Seite liegt nicht im Repo — Änderungen über die
  ha-mcp-Tools, dokumentiert in `waage-eg-notes.md`.
- **Bruch in der Messreihe beachten.** Ab dem Flash bedeutet „Rohwert" etwas
  anderes als davor (Intervallmittel statt Momentanwert). Für eine Auswertung,
  die über den Flashzeitpunkt hinweggeht, ist das der Übergang — und für
  Datensätze von davor gilt weiter „Temperatur", nicht „Temperatur Mittel".
- **Der Messwert hat jetzt einen Zeitschwerpunkt.** Bei 6 h Intervall liegt er
  drei Stunden vor dem Zeitstempel. Für Trends und Tagesbilanzen ist das
  richtig; für den Schwarm-Alarm (Ableitung über 20 min) spielt es keine Rolle,
  weil der ohnehin nur bei kurzem Intervall greift. Falls die Automation
  `Bienen: Messintervall nach Saison` je auf lange Intervalle im Sommer
  umgestellt wird, wäre das nochmal zu bedenken.
- **Nicht angefasst:** die Filterkette selbst. Median 5 → gleitendes Fenster 12
  ist mit der Mittelung streng genommen doppelt gemoppelt; das gleitende Fenster
  liefert überlappende und damit korrelierte Werte, was den Gewinn gegenüber
  √N drückt. Ein Median ohne nachgeschaltete Glättung wäre für die Mittelung
  sauberer — aber die Buttons und die Anzeige beim Kalibrieren hängen an genau
  dieser Glättung. Bewusst gelassen, wäre eine eigene Änderung mit eigenem Test.
