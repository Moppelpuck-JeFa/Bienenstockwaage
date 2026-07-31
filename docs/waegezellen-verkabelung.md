# 4 Wägezellen an einen HX711 - Verkabelung und Entscheidungen

Stand: offener Punkt aus den Projektnotizen (Zellen noch nicht beschafft).
Dieses Dokument sammelt, was vor dem Kauf entschieden sein muss.

## 1. Das Grundprinzip: Parallel heißt Mitteln, nicht Addieren

Vier Vollbrücken-Zellen werden gleichnamig parallel geschaltet:

| HX711 | Zelle 1 | Zelle 2 | Zelle 3 | Zelle 4 | typ. Aderfarbe |
|-------|---------|---------|---------|---------|----------------|
| E+    | E+      | E+      | E+      | E+      | rot            |
| E−    | E−      | E−      | E−      | E−      | schwarz        |
| A+    | A+      | A+      | A+      | A+      | grün           |
| A−    | A−      | A−      | A−      | A−      | weiß           |

Wichtig zum Verständnis: Die vier Brücken liegen dann elektrisch parallel, das
Ausgangssignal ist der **Mittelwert** der vier Zellen, nicht die Summe. Die
Empfindlichkeit bleibt also bei 2 mV/V - bezogen jetzt aber auf die
**Gesamtkapazität** von 4 × 50 kg = 200 kg.

Rechnung fürs Gefühl (bei ~4,3 V Speisung durch den HX711):

- Vollausschlag: 2 mV/V × 4,3 V ≈ **8,6 mV** bei 200 kg
- also ≈ 43 µV pro kg, ≈ **21 µV pro 0,5-kg-Schritt**

Der HX711 hat bei `gain: 128` einen Eingangsbereich von ±20 mV auf 24 Bit. Die
21 µV pro Anzeigeschritt liegen damit deutlich über dem Rauschen - die
geforderte 0,5-kg-Auflösung ist mit 4 × 50 kg problemlos erreichbar. Weniger
Kapazität (z. B. 4 × 20 kg) würde die Auflösung noch verbessern, aber ein voller
Stock mit Zargen kann 100 kg+ wiegen, deshalb ist die Reserve sinnvoll.

## 2. Der Punkt, der vor dem Kauf geklärt sein muss: Eingangswiderstand

Parallelschaltung **viertelt** den Brückenwiderstand, den der HX711 speisen muss:

- 4 × 1000 Ω → **250 Ω** → bei 4,3 V ≈ 17 mA
- 4 × 350 Ω → **87,5 Ω** → bei 4,3 V ≈ 49 mA

Die 4 × 350 Ω sind für die Spannungsversorgung der üblichen HX711-Breakouts
grenzwertig bis zu viel - der Regler auf dem Modul ist nicht dafür gedacht, und
in der Praxis äußert sich das als wegdriftende oder instabile Werte. Deshalb:

> **Beim Kauf auf den Eingangswiderstand achten und 1000-Ω-Zellen bevorzugen.**
> Wenn es doch 350-Ω-Zellen werden, braucht die Brücke eine externe
> Speisespannung statt der HX711-internen.

Das ist bei C3-Zellen relevant, weil die oft mit 350 Ω spezifiziert sind.

## 3. Eckenabgleich - der praktisch wichtigste Fehler

Bei Parallelschaltung wird gemittelt. Sind die vier Zellen nicht exakt gleich
empfindlich, hängt der Messwert davon ab, **wo** die Last auf der Platte liegt.
Genau das ändert sich bei einem Bienenstock ständig (Wabenbau wandert, Futter
sitzt einseitig, Zargen werden umgesetzt).

Zwei Wege:

- **Junction-Box mit Trimmpotis** (z. B. das Hiveeyes-Fertigteil): pro Zelle ein
  Poti im Signalzweig, damit wird der Eckenfehler beim Aufbau weggetrimmt. Das
  ist der Grund, warum sich eine gekaufte Box gegenüber einer reinen
  Lötklemmen-Parallelschaltung lohnt.
- **Eigenbau ohne Potis**: billiger, aber der Eckenfehler bleibt. Bei
  C3-Zellen aus derselben Charge landet man typisch im Bereich 0,5-2 %. Auf
  50 kg sind 1 % bereits 0,5 kg - also genau ein Anzeigeschritt. Das ist für
  eine Trendmessung (Tracht, Futterverbrauch, Schwarmalarm) verkraftbar, für
  einen absoluten Wert nicht.

**Empfehlung:** Fertigbox mit Trimmpotis, wenn der Absolutwert zählt. Eigenbau
reicht, wenn es primär um die Gewichts*änderung* geht - und dafür ist die Waage
ja gedacht.

## 4. Kabelführung (unterschätzt, aber entscheidend)

Das Brückensignal ist im µV-Bereich - jeder Meter Analogkabel ist eine Antenne.

- **HX711-Platine so nah wie möglich an die Junction-Box**, idealerweise mit ins
  gleiche Gehäuse. Die langen Kabel dann für die *digitale* Seite
  (5 V, GND, DOUT, CLK) verwenden, nicht für die Analogseite.
- Geschirmtes Kabel für die Zellenleitungen, **Schirm nur an einer Seite**
  auflegen (am HX711/ESP-Ende), sonst Brummschleife.
- Zellenkabel nicht kürzen, wenn es sich vermeiden lässt: bei 4-Leiter-Zellen
  ist der Kabelwiderstand Teil der Kalibrierung, Kürzen verschiebt den Wert.
  Nicht schlimm bei uns - wir kalibrieren ohnehin nach dem Einbau per Button.
- Draußen: Zellen mit IP67/IP68, Box mit Kabelverschraubungen, und
  Kabeleinführungen nach unten führen (Tropfschlaufe).

## 5. Adern identifizieren, wenn die Farben unklar sind

Bei No-Name-Zellen stimmen die Farbcodes oft nicht. Mit dem Ohmmeter:

- Das Paar mit dem **höheren** Widerstand ist E+/E− (Eingang/Speisung)
- Das Paar mit dem **niedrigeren** Widerstand ist A+/A− (Ausgang/Signal)
- Zwischen einer Eingangs- und einer Ausgangsader misst man ca. den halben Wert

Polarität von A+/A− notfalls durch Ausprobieren: Zeigt die Waage bei Last
negativ, A+ und A− tauschen (oder in Software mit dem Vorzeichen leben - unsere
Zwei-Punkt-Kalibrierung kommt auch mit negativem Span klar, weil sie nur die
Differenz `calib_raw_half − calib_raw_zero` benutzt).

## 6. Die ernsthafte Alternative: 4 × HX711 statt Junction-Box

Statt zu mitteln kann man jede Zelle einzeln messen und in Software addieren:

- 4 HX711-Module, alle am **gemeinsamen CLK**, je ein eigener DOUT
- Pinbedarf am D1 Mini: 5 GPIOs (z. B. D1, D2, D5, D6 für DOUT + D7 als CLK) -
  passt problemlos
- **Vorteile:** kein Eckenfehler (es wird wirklich summiert), kein
  Speisestromproblem (jeder HX711 treibt nur seine eigene Brücke), und man sieht
  pro Ecke, ob eine Zelle spinnt
- **Nachteile:** 4× so viel Verkabelung, und die Kalibrierung muss umgebaut
  werden - die beiden Buttons kalibrieren aktuell *ein* Signal. Bei vier Zellen
  bräuchte man entweder vier Kalibrierpunkt-Paare oder man kalibriert die
  Software-Summe als Ganzes (letzteres geht und wäre der kleine Umbau)

**Einschätzung:** Wenn die Zellen noch nicht gekauft sind, ist das die technisch
sauberere Variante und kostet kaum mehr (HX711-Module sind Cent-Artikel). Der
Aufwand liegt in der Verdrahtung, nicht im Code - die YAML-Änderung wäre
überschaubar (vier interne Sensoren, ein Template-Sensor der summiert, die
bestehende Zwei-Punkt-Kalibrierung bleibt unverändert auf der Summe).

## Offene Entscheidungen

1. Eingangswiderstand der Zellen: 1000 Ω (HX711-freundlich) oder 350 Ω
   (dann externe Speisung einplanen)
2. Junction-Box mit Trimmpotis kaufen oder ohne bauen
3. Ein HX711 mit Parallelschaltung, oder vier HX711 mit Software-Summe
