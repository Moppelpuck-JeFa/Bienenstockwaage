# waage

ESPHome-Konfiguration für Bienenstockwaagen -
ESP8266 (D1 Mini) mit HX711-Wägezellenverstärker und 4 Wägezellen.
Produktiv läuft **"Waage eG"**; die Konfiguration ist so aufgeteilt, dass
weitere Stöcke ohne Code-Duplizierung dazukommen (siehe
[Mehrere Stöcke](#mehrere-stöcke)).

## Dateien

| Datei | Inhalt |
|-------|--------|
| [`CLAUDE.md`](CLAUDE.md) | Kurzanleitung für Claude Code: Prüfbefehle, Architektur, harte Regeln. Wird bei jeder Session automatisch geladen |
| [`packages/waage-basis.yaml`](packages/waage-basis.yaml) | Die gesamte gemeinsame Logik - der eigentliche Code. Wird nicht direkt geflasht |
| [`packages/waage-temperatur.h`](packages/waage-temperatur.h) | Die Formel der Temperaturkompensation, an einer Stelle statt an dreien |
| [`packages/waage-mittelwert.h`](packages/waage-mittelwert.h) | Mittelwert und Streuung des Messfensters, aus demselben Grund ausgelagert |
| [`packages/waage-grenzen.h`](packages/waage-grenzen.h) | Plausibilitätsfenster: welches Gewicht überhaupt nach Home Assistant darf |
| [`waage-eg.yaml`](waage-eg.yaml) | Stock 1 "Waage eG": nur substitutions + package-Include |
| [`waage-stock2.yaml`](waage-stock2.yaml) | Stock 2, gleiche Bauart |
| [`waage-stock3.yaml`](waage-stock3.yaml) | Stock 3, gleiche Bauart |
| [`secrets.yaml.example`](secrets.yaml.example) | Vorlage für die Zugangsdaten (kopieren nach `secrets.yaml`) |
| [`waage-eg-notes.md`](waage-eg-notes.md) | Projektnotizen: alle Entscheidungen, Anforderungen, offene Punkte |
| [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md) | Verkabelung der 4 Zellen, Junction-Box, Kaufkriterien |
| [`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md) | Was Batterie-/Solarbetrieb braucht: Verdrahtung, Strombilanz, YAML |
| [`docs/sessionbericht-2026-08-03.md`](docs/sessionbericht-2026-08-03.md) | Temperaturdrift ausgewertet, Durchsichtmodus, Kalibrierungsverlust |
| [`docs/sessionbericht-2026-08-04.md`](docs/sessionbericht-2026-08-04.md) | Umstellung auf substitutions/packages, Namensschema, ESPHome-Fallstricke |
| [`docs/sessionbericht-2026-08-10.md`](docs/sessionbericht-2026-08-10.md) | Temperaturkompensation: Auswertung über 7 Tage und Einbau |
| [`docs/sessionbericht-2026-08-11.md`](docs/sessionbericht-2026-08-11.md) | Mittelung der Rohwerte über das Messintervall, zwei neue Diagnose-Entities |
| [`docs/sessionbericht-2026-08-12.md`](docs/sessionbericht-2026-08-12.md) | Plausibilitätsfenster gegen Ausreißer in der Langzeitstatistik |
| [`tests/waage-temperatur-test.cpp`](tests/waage-temperatur-test.cpp) | Prüfprogramm für die Kompensationsformel (`g++`, ohne Hardware) |

`secrets.yaml` selbst ist per `.gitignore` ausgeschlossen und gehört nicht ins Repo.

## Anschlüsse

| Signal | Pin | GPIO |
|---|---|---|
| HX711 DOUT | `D6` | 12 |
| HX711 CLK | `D1` | 5 |
| DS18B20 Data | `D5` | 14 |
| Durchsicht-Taster | `D2` | 4 |
| Durchsicht-LED | `D7` | 13 |

Der **Taster** liegt gegen GND und nutzt den internen Pull-up — kein externer
Widerstand nötig. Die **LED** hängt über einen Vorwiderstand von **1 kΩ**
gegen GND (~1,3 mA; hell genug und sparsam genug für den späteren
Solarbetrieb).

Der DS18B20 braucht einen **externen Pull-up 4,7 kΩ zwischen D5 und 3V3** —
parallel zur Datenleitung, nicht in Reihe. Unbedingt gegen **3,3 V**, nicht
gegen 5 V: die GPIOs des ESP8266 sind nicht 5-V-tolerant. Viele fertige
DS18B20-Module haben den Widerstand schon an Bord, dann keinen zweiten dazu.

Zwei Pins sind bewusst freigehalten. **D0 (GPIO16)** ist der einzige Weg, auf
dem der ESP8266 aus dem Deep Sleep aufwacht — der RTC-Timer zieht GPIO16 auf
Masse, weshalb der Pin an `RST` liegen muss. Elektrisch hätte der HX711 dort
funktioniert (der Treiber pollt, und DOUT wird aktiv getrieben, GPIO16 kann
nämlich keine Interrupts), aber damit wäre Batteriebetrieb dauerhaft
ausgeschlossen. Deshalb liegt DOUT auf `D6`. **D4 (GPIO2)** wurde für den
DS18B20 gemieden, weil es ein Boot-Strapping-Pin ist.

Details zur Verdrahtung und zum Rest der Deep-Sleep-Vorbereitung:
[`docs/deep-sleep-vorbereitung.md`](docs/deep-sleep-vorbereitung.md).

## Was das Gerät kann

- Gewicht in **kg**, gerundet auf **0,1 kg**
- **Messintervall frei einstellbar aus Home Assistant** (1 Minute bis 7 Tage),
  Voreinstellung 360 min = 6 h
- **Mittelung über das ganze Messintervall** — veröffentlicht wird nicht eine
  Momentaufnahme, sondern der Mittelwert aller Rohwerte seit der letzten
  Messung (bei 6 h rund 4.300 Werte). Ein längeres Intervall misst dadurch
  auch genauer. Siehe unten
- **Zwei-Punkt-Kalibrierung per Button aus HA** - kein festes `calibrate_linear`
  im YAML, die Kalibrierwerte liegen in `globals` mit `restore_value: yes` und
  überleben einen Neustart
- **Referenzgewicht frei wählbar** (0,1 bis 50 kg) - je schwerer, desto genauer
- **Tara-Button**, unabhängig von der Kalibrierung
- **Temperaturkompensation** (DS18B20) — die Wägezellen driften mit der
  Temperatur; an "Waage eG" gemessene **+32,5 g/K** werden aus dem Gewicht
  herausgerechnet. Der Koeffizient ist aus HA einstellbar, `0` schaltet die
  Kompensation ab. Siehe unten.
- **Plausibilitätsfenster 0 bis 150 kg** — was außerhalb liegt, geht gar nicht
  erst nach Home Assistant. Solche Werte kommen nie aus dem Volk, sondern aus
  einer halben Kalibrierung, und sie stünden sonst dauerhaft in der
  Langzeitstatistik. Der Zähler „Gewicht verworfen" macht sichtbar, dass es
  passiert ist. Siehe unten
- **Verbindungsstatus und WLAN-Signal** als Diagnose — damit ein Ausfall
  auffällt, statt dass die Sensoren still ihren letzten Wert behalten
- Fallback-Hotspot, OTA-Updates, Webserver auf Port 80

## Inbetriebnahme

1. `secrets.yaml.example` nach `secrets.yaml` kopieren und ausfüllen
   (im ESPHome Device Builder liegt sie unter `/config/esphome/secrets.yaml`)
2. `waage-eg.yaml` **und den Ordner `packages/`** ins ESPHome-Verzeichnis
   legen, kompilieren und flashen. Ohne `packages/` bricht das Kompilieren
   mit einem Fehler zum fehlenden Include ab - die Stock-Datei allein
   enthält keine Logik mehr.
3. Gerät in Home Assistant hinzufügen (der API-Key aus `secrets.yaml`)

## Mehrere Stöcke

Die Konfiguration ist in **eine Basis + eine Datei pro Stock** aufgeteilt:

```
packages/waage-basis.yaml    komplette Logik, für alle Stöcke identisch
waage-eg.yaml                Stock 1 - nur substitutions + package-Include
waage-stock2.yaml            Stock 2
waage-stock3.yaml            Stock 3
```

Die Stock-Dateien liegen bewusst **im Root**, nicht in einem
Unterverzeichnis: Das ESPHome Device Builder Add-on listet nur die YAML-
Dateien direkt in `/config/esphome/` als flashbare Geräte auf.
Unterverzeichnisse liest es ausschließlich über `packages:` / `!include` —
genau deshalb taucht `packages/waage-basis.yaml` dort nicht als
Pseudo-Gerät auf.

### Einen weiteren Stock anlegen

1. Eine der vorhandenen Stock-Dateien kopieren
2. `geraete_name`, `anzeige_name` und `ap_ssid` anpassen
3. Pins nur anfassen, wenn wirklich anders verdrahtet wurde
4. Flashen, in HA hinzufügen, **beide Kalibrierschritte** fahren

Alles andere — Filterkette, Kalibrier-Buttons, Durchsichtmodus, Taktgeber —
kommt aus der Basis und wird nie kopiert. Eine Verbesserung dort wirkt
nach dem nächsten Flash auf allen Stöcken.

### Namensschema

`waage-stock2` / `waage-stock3` sind als Vorgabe eingetragen und
funktionieren sofort. **Empfehlung für die Praxis: stattdessen den
Standort verwenden** — `waage-garten`, `waage-streuobst`, `waage-hausecke`.

Grund: `geraete_name` und `anzeige_name` bilden die entity_id in HA
(`"Waage eG"` + `"Gewicht"` → `sensor.waage_eg_gewicht`). Wird später
umbenannt, legt HA **neue** Entities an, und Verlauf, Helfer, Automationen
und Dashboard hängen an den alten, toten IDs. Durchnummerierte Namen laden
genau dazu ein: sobald ein Volk eingeht, verkauft oder umgesetzt wird,
passt die Nummerierung nicht mehr zur Realität. Ein Standortname bleibt
richtig, auch wenn das Volk darin wechselt.

**Solange ein Stock noch nicht geflasht ist, ist das Umbenennen gratis** —
drei Zeilen in der Stock-Datei. Danach kostet es die Historie.

`waage-eg` bleibt aus genau diesem Grund unverändert.

### Zugangsdaten

Alle Stöcke teilen sich standardmäßig WLAN, API-Key und OTA-Passwort aus
`secrets.yaml`. Das ist zulässig und hält `secrets.yaml` klein. Wer pro
Gerät eigene Schlüssel will, legt sie unter eigenem Namen in
`secrets.yaml` an und überschreibt den Block in der jeweiligen Stock-Datei
(dort auskommentiert vorbereitet) — Werte aus der Stock-Datei haben Vorrang
vor denen aus dem Package.

Für `waage-eg` bewusst **nicht** gemacht: ein neuer API-Key erzwingt ein
erneutes Hinzufügen des Geräts in HA und riskiert genau die Entity-IDs,
die erhalten bleiben sollen.

## Kalibrieren

Die Waage mittelt das Rohsignal über etwa **60 Sekunden**. Deshalb bei allen
drei Buttons gilt: erst auflegen bzw. abräumen, **~1 Minute warten**, dann drücken.

1. **"Waage eG Referenzgewicht"** auf die Masse deines Prüfgewichts stellen
   (in kg, z. B. `10` oder `4.987`). Das ist nur die Eingabe - sie ändert noch
   nichts.
2. Waage komplett leer räumen → 1 min warten → **"Waage eG Kalibrieren 0kg"**
3. Referenzgewicht auflegen → 1 min warten →
   **"Waage eG Kalibrieren Referenzgewicht"**
4. Fertig. Ob es geklappt hat, steht im ESPHome-Log (`Nullpunkt gesetzt: ...`
   bzw. `Referenzpunkt gesetzt: 10.000 kg = roh ... (Span ... counts, ... counts/kg)`).
   Die Diagnose-Entity **"Waage eG Kalibriert mit"** zeigt danach dauerhaft an,
   mit welchem Gewicht zuletzt tatsächlich kalibriert wurde.

### Nimm ein möglichst schweres Referenzgewicht

Der gemessene Span wird auf den ganzen Bereich bis 200 kg hochgerechnet. Bei
0,5 kg Referenz ist das Faktor 400 - und jeder Fehler beim Setzen des Punkts
skaliert genauso mit hoch. Konkret, bei einem Ablesefehler von 20 counts und
hochgerechnet auf 100 kg:

| Referenzgewicht | Fehler bei 100 kg |
|---|---|
| 0,5 kg | ~222 g |
| 10 kg | ~11 g |

Erlaubt ist alles von **0,1 bis 50 kg** in 1-g-Schritten. Ein nachgewogener
Sack Zucker oder eine Hantelscheibe tut es - Hauptsache, du kennst die Masse
und trägst sie genau ein. Und: **das Gewicht möglichst mittig auflegen**, sonst
geht der Eckenfehler der Wägezellen direkt in die Kalibrierung ein.

### Referenzgewicht ändern ändert nichts an der Messung

Die Number-Entity ist reine Eingabe für die **nächste** Kalibrierung. Die
Anzeige rechnet gegen das Gewicht, mit dem tatsächlich kalibriert wurde
(sichtbar unter "Kalibriert mit"). Du kannst die Zahl also gefahrlos anpassen -
wirksam wird sie erst mit dem Druck auf den Kalibrier-Button.

**"Waage eG Tara"** nullt nur das gerade aufliegende Gewicht (z. B. eine leere
Zarge) und lässt die Kalibrierung unangetastet. Umgekehrt setzt
"Kalibrieren 0kg" ein bestehendes Tara zurück, weil es mit einem neuen
Nullpunkt seine Bedeutung verliert.

### Was 0,1 kg realistisch heißt

Die **Auflösung** ist 0,1 kg - so fein wird angezeigt. Die **Genauigkeit** des
Absolutwerts liegt realistisch bei ±0,1 bis ±0,5 kg, begrenzt durch Kriechen
und vor allem den Eckenfehler der Wägezellen (nicht durch die Elektronik - der
HX711 hat hier reichlich Reserve).

Der vierte Posten, die **Temperaturdrift**, war mit rund 0,65 kg über einen
Sommertag der mit Abstand größte - er wird seit dem 10.08.2026
herausgerechnet, siehe [Temperaturkompensation](#temperaturkompensation). Übrig
bleibt davon eine Reststreuung von rund 15 g.

Für das, worum es bei einer Stockwaage geht - Gewichts*änderung* über Stunden
und Tage - ist das genau richtig, weil sich diese Fehler bei gleichbleibendem
Aufbau herauskürzen. Die letzte Nachkommastelle sollte man nur nicht als
absolute Wahrheit lesen. Hintergrund in den Projektnotizen und in
[`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md).

Eine leere Waage kann dabei statt 0,0 auch mal ±0,1 anzeigen - dann hilft ein
Druck auf "Tara". Das Tara wird dabei gegen den **kompensierten** Wert gesetzt
und hält deshalb über den Tagesgang; früher wanderte die Drift nach dem
Tarieren wieder ins Gewicht zurück.

## Messintervall einstellen

In Home Assistant gibt es die Entity **"Waage eG Messintervall"** - ein
Eingabefeld in **Minuten**, in das du die Zahl direkt eintippst.

| Wunsch | Eintrag |
|--------|---------|
| 15 Minuten | `15` |
| 1 Stunde | `60` |
| 6 Stunden (Voreinstellung) | `360` |
| 12 Stunden | `720` |
| 1 Tag | `1440` |

Erlaubt ist alles von **1** bis **10080** (7 Tage). Die Änderung greift sofort:
der Zähler startet neu und es wird direkt einmal gemessen, damit du in HA
siehst, dass die Einstellung angekommen ist. Der Wert überlebt einen Neustart.

### Das Intervall ist auch das Mittelungsfenster

Seit dem 11.08.2026 ist der veröffentlichte Wert **der Mittelwert aller
Rohwerte seit der letzten Messung** — nicht mehr eine Momentaufnahme. Der HX711
liefert alle 5 Sekunden einen gefilterten Wert; bei 6 Stunden Intervall gehen
also rund 4.300 Werte in eine Zahl ein statt der 12, die im 60-Sekunden-Fenster
der Filterkette stecken.

**Ein längeres Intervall misst damit auch genauer.** Simulation der echten
Filterkette mit 300 counts Rauschen je Sekundenwert
([`tests/waage-temperatur-test.cpp`](tests/waage-temperatur-test.cpp), Punkt
10), angegeben ist die Reststreuung bei konstanter Last:

| Intervall | vorher (Momentanwert) | jetzt (Fenstermittel) |
|---|---|---|
| 1 min | 2,1 g | 1,9 g |
| 15 min | 2,2 g | 0,6 g |
| 60 min | 2,1 g | 0,3 g |
| 360 min | 2,2 g | 0,1 g |

Bei 1 Minute bringt es fast nichts — dort *ist* das Intervall schon das
Filterfenster. Der Gewinn wächst mit der Wurzel aus der Zahl der Werte und ist
ab etwa einer Viertelstunde deutlich.

Das ist gegenüber den übrigen Fehlerquellen (Kalibrierung, Kriechen,
Temperaturrest ~15 g) klein — die Mittelung macht die Waage nicht auf 0,1 g
genau, sie nimmt nur den Rauschanteil praktisch vollständig heraus. Sichtbar
wird das vor allem in der **Tagesbilanz**, wo sich zwei verrauschte Messwerte
sonst zu doppeltem Rauschen addieren.

Zwei Nebenwirkungen, die man kennen sollte:

- **Der Messwert ist ein Mittel über den ganzen Zeitraum, kein "jetzt".** Bei
  6 h Intervall liegt sein Schwerpunkt drei Stunden in der Vergangenheit. Für
  Trends und Tagesbilanzen ist das richtig; wer einen Momentanwert braucht,
  drückt "Jetzt messen" (der liefert weiterhin den Momentanwert).
- Auch die **Temperatur** wird über dasselbe Fenster gemittelt, sonst würde die
  Kompensation ein 6-Stunden-Mittel mit der Temperatur des Sendezeitpunkts
  korrigieren. Der Fehler dabei wäre nicht klein: in derselben Simulation
  **97,7 g** bei 6 h Intervall. Dafür gibt es die Diagnose-Entity
  "Temperatur Mittel"; die Entity "Temperatur" bleibt der Momentanwert im
  60-Sekunden-Takt.

Während einer **Durchsicht** und im Nachlauf danach wird nichts gesammelt — die
abgestellte Zarge landet also nicht anteilig im nächsten Mittelwert. Auch
**Tarieren und Kalibrieren** verwerfen das laufende Fenster, weil die bis dahin
gesammelten Werte gegen eine andere Rechnung entstanden sind.

### "Die Werte aktualisieren sich so langsam"

Das ist die Voreinstellung: **360 Minuten**. Für den Dauerbetrieb einer
Stockwaage ist das richtig, beim Einrichten stört es.

- **Einzelner Wert jetzt:** Button **"Waage eG Jetzt messen"** drücken. Er
  veröffentlicht Gewicht und alle Diagnose-Entities sofort und startet den
  Intervall-Zähler neu. Das Intervall selbst bleibt unverändert. Er liefert
  bewusst den **Momentanwert**, kein Fenstermittel — wer gerade etwas
  aufgelegt hat, will nicht das Mittel der letzten Stunden sehen. "Rohwert
  Streuung" bleibt dabei entsprechend stehen.
- **Dauerhaft schneller zum Testen:** "Waage eG Messintervall" auf `1` stellen,
  danach wieder hochsetzen.
- **Am schnellsten:** das ESPHome-Log. Der HX711-Treiber loggt **jede Sekunde**
  den Rohwert (`[D][hx711:031]: '': Got value 25830`). Dafür braucht es keine
  Konfigurationsänderung, und man sieht sofort, wie ruhig das Signal ist.

Zu beachten: Zwischen Auflegen und einem verlässlichen Wert liegen ohnehin
~60 Sekunden Filterlaufzeit. Schneller als etwa eine Minute wird die Anzeige
also nie sinnvoll reagieren, egal welches Intervall eingestellt ist.

Nach einem Neustart kommt der erste Wert bereits nach ~1 Minute, unabhängig vom
eingestellten Intervall - du musst also nicht bis zum Ablauf einer vollen
Periode warten, um zu sehen, ob das Gerät läuft.

## Durchsichtmodus

Während einer Durchsicht misst die Waage alles Mögliche — die abgestellte
Zarge, die Hand am Rahmen, den Stockmeißel — nur nicht das Stockgewicht. Diese
Werte dürfen nicht nach Home Assistant, sonst stehen sie für immer in der
Langzeitstatistik und reißen Tagesbilanz und Trend auseinander.

**Ein Druck auf den Taster am Gehäuse** schaltet die Übertragung ab, die LED
leuchtet. Nach der eingestellten Zeit (Voreinstellung 60 min) endet der Modus
von selbst.

| Bedienung | Wirkung |
|---|---|
| Taster drücken, LED aus → an | Durchsicht startet, keine Übertragung mehr |
| Taster nochmal drücken, LED an → aus | Durchsicht sofort beenden |
| Taster während der Durchsicht | jeder Druck startet die Zeit neu (verlängern) |
| LED blinkt | weniger als 5 Minuten übrig |

Dasselbe geht über die Entity **„Waage eG Durchsichtmodus"** in HA — praktisch,
wenn man am Stock das Drücken vergessen hat. Die Dauer stellt
**„Waage eG Durchsichtdauer"** ein (5 bis 240 Minuten).

**Was dabei genau passiert:** Der HX711 misst und filtert unverändert weiter,
nur die Veröffentlichung nach HA unterbleibt — Gewicht wie Diagnosewerte. In HA
bleibt der letzte Wert von vor der Durchsicht stehen. Nach dem Ende laufen
**zwei Minuten Nachlauf**, damit das 60-Sekunden-Mittelungsfenster die
Manipulation ausspült; danach kommt sofort ein frischer Wert, unabhängig vom
eingestellten Messintervall.

Ein Neustart beendet den Modus. Das ist Absicht: eine Waage, die nach einem
Stromausfall still im Durchsichtmodus hängt und wochenlang nichts mehr sendet,
wäre schlimmer als einmal zu viel drücken.

### Der Schwarm-Alarm ist gegen Fehlauslöser gesperrt

Nach der Durchsicht springt das Gewicht in einem Schritt auf den neuen Wert.
Der Helfer für die Schwarm-Erkennung (Ableitung über 20 min in kg/h) sieht
diesen Sprung als rasanten Gewichtsverlust — hast du eine Honigzarge
abgenommen, sähe das für ihn aus wie ein abgegangener Schwarm.

Dasselbe gilt für **jeden** Sprung, der nicht vom Stock kommt. Am 10.08.2026
hat der Alarm nach einer Neukalibrierung fehlausgelöst (−3,3 kg/h). Die
Automation `Bienen: Schwarm-Alarm` hat deshalb seitdem **drei Sperren**
zusätzlich zum Zeitfenster 9–18 Uhr:

| Sperre | Fängt ab |
|---|---|
| `switch.waage_eg_durchsichtmodus` = off `for: 00:30:00` | Durchsicht, Honigernte |
| `sensor.waage_eg_betriebszeit` > 1800 | Neustart der Waage — vor allem den Fall, dass ein Flash die Kalibrierung verliert |
| Template: 30 min seit letzter Kalibrierung/Tara | Kalibrieren, Tarieren |

**Alle drei sind 30 min lang, nicht 20** — die Sperre muss länger halten als
das Ableitungsfenster, sonst steckt der Sprung beim Freigeben noch darin.

Die dritte Sperre prüft `last_changed` von „Kalibrierfaktor", „Kalibriert bei"
und den drei Buttons. Die Diagnose-Sensoren sind dabei die wichtigeren: Wird
ein Button über die **ESPHome-Weboberfläche** statt über HA gedrückt, sieht HA
den Druck nicht — die Sensoren ändern sich trotzdem. Einzige verbleibende
Lücke ist ein Tara über die Weboberfläche.

Warum dafür ein Template und nichts Natives: Eine `state`-Bedingung mit `for:`
braucht einen festen Zielzustand. Der Zustand eines Buttons *ist* aber der
Zeitstempel des letzten Drucks. Für „hat sich lange nicht geändert" gibt es in
HA keine native Bedingung.

Sinngemäß dasselbe fehlt noch für `Bienen: Futtervorrat kritisch` — dort ist es
weniger dringend, weil der Schwellwert erst nach 2 h Überschreitung auslöst.

## Temperaturkompensation

Die Wägezellen driften mit der Temperatur: Bei Wärme zeigt die Waage mehr an,
ohne dass sich am Stock etwas geändert hätte. An "Waage eG" sind das
**+32,5 g/K**, gemessen über 6,8 Tage und 9,4 K Temperaturhub
([`docs/sessionbericht-2026-08-10.md`](docs/sessionbericht-2026-08-10.md)).
Über einen normalen Sommertag mit 20 K Tagesgang sind das **0,65 kg** — mehr
als ein guter Trachttag einträgt. Deshalb wird gerechnet:

```
Gewicht = Bruttogewicht − (Temperatur − "Kalibriert bei") × Koeffizient − Tara
```

**Der Bezugspunkt ist die Temperatur beim Nullpunkt-Kalibrieren.** Sie steht in
der Diagnose-Entity "Kalibriert bei". Genau dort ist die Korrektur null; die
Anzeige entspricht dann exakt dem, was ohne Kompensation herauskäme. Ist
"Kalibriert bei" leer, wurde "Kalibrieren 0 kg" nie gedrückt — dann fehlt der
Bezugspunkt und es wird **bewusst nicht** kompensiert.

### Die zwei Entities dazu

| Entity | Bedeutung |
|---|---|
| `number.<stock>_temperaturkoeffizient` | Der Koeffizient in **g/K**. Einstellbar, `0` = Kompensation aus |
| `sensor.<stock>_temperaturkorrektur` | Was gerade abgezogen wird, in kg. Diagnose |
| `sensor.<stock>_temperatur_mittel` | Die Temperatur, mit der gerechnet wurde: Mittel über dasselbe Messfenster wie der Rohwert. Diagnose |

**Warum der Koeffizient aus HA einstellbar ist und nicht im YAML steht:** Er
ist eine Messgröße, keine Konstante. Er hängt an diesen vier Zellen, an dieser
Verkabelung und an diesem Aufbau. Nach dem Umzug in den Garten — Sonne auf
einer Seite, andere thermische Ankopplung — ist er neu zu bestimmen. Als
Number kostet das eine Zahl in HA; fest im YAML kostete es einen Flash, und
**jeder Flash kann die Kalibrierung mitnehmen**.

**"Temperaturkorrektur" ist die Kontrollentity.** Ohne sie sieht man in HA nur
noch das fertige Gewicht und kann nicht mehr unterscheiden, ob eine Änderung
aus dem Stock kommt oder aus der Rechnung. Steht dort dauerhaft 0,000, ist die
Kompensation aus.

**Der Rohwert bleibt unkompensiert.** "Rohwert" ist und bleibt der nackte
HX711-Zählwert (seit dem 11.08.2026 über das Messintervall gemittelt, aber
nicht korrigiert). Nur so lässt sich der Koeffizient später gegen neue Daten
nachprüfen — ein bereits korrigierter Wert wäre für eine Nachmessung wertlos.

### Den Koeffizienten für einen neuen Stock bestimmen

Neue Stöcke starten mit `0` (Kompensation aus). Der Wert von "Waage eG" lässt
sich **nicht** übernehmen: ein geratener Koeffizient macht die Messung
schlechter, nicht besser — und man merkt es nicht, weil das Ergebnis weiter
plausibel aussieht.

1. Kalibrieren (beide Schritte), dann eine **konstante Last** auflegen und
   liegen lassen.
2. Messintervall auf `15` oder `60` stellen und **mindestens fünf Tage**
   laufen lassen. Die Automation `Bienen: Messintervall nach Saison` dabei
   ausschalten, sonst überschreibt sie das Intervall nachts um 3 Uhr.
3. "Rohwert" gegen **"Temperatur Mittel"** auftragen und eine Gerade
   durchlegen — **mit einem Zeitglied als zweitem Term**. Ohne das schiebt das
   Kriechen der Zellen den Koeffizienten nach oben (im 2-Tage-Datensatz vom
   03.08. um 5 g/K, im 7-Tage-Datensatz um knapp 5 g/K).

   > **Nicht "Temperatur" nehmen.** Seit der Rohwert ein Intervallmittel ist,
   > passt der Momentanwert von "Temperatur" zeitlich nicht mehr dazu; die
   > Regression käme zu flach heraus. "Temperatur Mittel" deckt exakt
   > denselben Zeitraum ab wie der Rohwert. Für Datensätze von **vor** dem
   > 11.08.2026 gilt umgekehrt weiter "Temperatur".
4. Steigung in g/K in die Number eintragen. Kontrolle: "Temperaturkorrektur"
   muss sich über den Tag sichtbar bewegen, das Gewicht bei konstanter Last
   dagegen nicht mehr.

Ein Tag reicht dafür nicht. Über einen einzelnen Tag sind Temperatur- und
Zeitanteil nicht zu trennen; die Tageswerte der Messreihe an "Waage eG"
streuten zwischen 23 und 35 g/K, über die volle Woche kamen 32,5 ± 0,7 heraus.

### Was die Kompensation nicht kann

Sie korrigiert die **gemeinsame** Temperatur aller vier Zellen. Steht die Waage
in der Sonne und wird eine Ecke deutlich wärmer als die anderen, entsteht ein
Fehler, den ein einzelner Sensor prinzipiell nicht sehen kann. Dass das hier
klein ist, war die Bedingung für den Einbau und wurde geprüft: Erwärmung und
Abkühlung liefern denselben Zusammenhang (Hysterese ±1,4 g). **Nach dem Umzug
in den Garten ist das erneut zu prüfen** — dort ist genau diese Voraussetzung
am ehesten verletzt.

Ebenfalls nicht korrigiert wird das **Kriechen** der Zellen nach einem
Lastwechsel. In der Messreihe steckt ein davon herrührender Anteil von
−10,9 g/Tag, der mit der Temperatur nichts zu tun hat.

**Die Variante ohne alles:** Gewicht immer zur selben Uhrzeit vergleichen, am
besten vor Sonnenaufgang. Das kürzt den Tagesgang ohnehin weitgehend heraus.
Der Helfer "Waage Tagesbilanz" (`statistics`/`change`/24 h) macht genau das
und funktioniert mit und ohne Kompensation.

## Plausibilitätsfenster: was gar nicht erst nach HA geht

Die Waage veröffentlicht ein Gewicht nur, wenn es **zwischen 0 und 150 kg**
liegt. Alles andere wird verworfen — die Entity „Gewicht" behält dann ihren
letzten guten Wert, und der Zähler „Gewicht verworfen" zählt eins hoch.

**Warum überhaupt.** `sensor.waage_eg_gewicht` hat `state_class: measurement`,
jeder veröffentlichte Wert landet damit **dauerhaft in der Langzeitstatistik**.
Der Zustandsverlauf wird nach rund 10 Tagen aufgeräumt, die Statistik nie. Am
11.08.2026 gingen während einer kaputten Kalibrierung nacheinander
**+2.299,4 kg, +1.035,3 kg, −3.749,7 kg und −48,6 kg** nach HA. Das
Stundenmittel dieser Stunde steht seitdem bei **+70,8 kg** statt bei den
tatsächlichen ~26,7 kg, und jede `statistics-graph`-Karte skaliert auf
±3.750 kg — die echte Messreihe wird darin zu einer flachen Linie.
Herausrechnen lässt sich das hinterher nur noch von Hand.

**Warum verwerfen und nicht kappen.** Ein auf 150,0 kg gekappter Wert wäre eine
Behauptung über das Volk, die genauso falsch ist wie die 2.299 — nur
unauffälliger. Ein fehlender Wert ist ehrlich.

**Was weiterhin gesendet wird:** Rohwert, Rohwert-Streuung, Temperatur,
Temperatur Mittel, Kalibrierfaktor, „Kalibriert bei"/„mit" und
Temperaturkorrektur. Genau die braucht man, um herauszufinden, warum das
Gewicht fehlt — deshalb hängen sie nicht mit am Fenster.

### Die Grenzen ändern

Beide stehen als `substitutions` in
[`packages/waage-basis.yaml`](packages/waage-basis.yaml) und lassen sich pro
Stock in dessen Datei überschreiben:

```yaml
substitutions:
  plausibel_kg_min: "0"
  plausibel_kg_max: "150"
```

`plausibel_kg_max` kleiner oder gleich `plausibel_kg_min` **schaltet die Prüfung
ab** (dann geht alles außer NAN durch).

**Die Untergrenze 0 hat einen Preis, und der ist Absicht:** Steht die Waage
frisch tariert bei null, ist −0,1 kg ein *echter* Messwert — und der bleibt
jetzt aus. Werte zwischen −0,05 und 0 sind nicht betroffen, die runden schon in
der Anzeige auf 0,0. Wie oft es bei 14 g Rauschen wirklich zuschlägt, rechnet
das Prüfprogramm aus (Punkt 11): bei einem Stock auf 0,0 kg rund **0,01 %** der
Werte, ab 0,1 kg Last **gar keine**. Wer die Null-Umgebung sauber sehen will —
etwa für eine Driftmessung mit leerer, tarierter Waage — setzt
`plausibel_kg_min: "-5"`.

### „Gewicht verworfen" lesen

| Anzeige | Bedeutung |
|---|---|
| 0 | Normalfall. Alles, was gemessen wurde, ist auch in HA. |
| einzelne Werte, direkt nach dem Kalibrieren | Erwartbar: zwischen „Kalibrieren 0kg" und „Kalibrieren Referenzgewicht" stimmt der Span nicht. |
| steigt im Takt des Messintervalls | Die Kalibrierung ist kaputt. Kalibrierfaktor prüfen (Erwartung −18.000 bis −21.000) und **beide** Schritte neu fahren. |

Der Zähler beginnt nach jedem Neustart wieder bei 0 und hat bewusst **keine**
`state_class` — er ist kein Messwert und gehört nicht in die Langzeitstatistik.
Warum ein Wert verworfen wurde, steht mit Rohwert im Log:

```
[W][waage]: Gewicht -2311.7 kg verworfen - ausserhalb von 0.0..150.0 kg
            (Kalibrierung pruefen! roh -599434.0, 1. Mal seit dem Neustart)
```

> **Die Statistik von vor dem Umbau bereinigt das nicht.** Das Fenster wirkt ab
> dem Flash; die Ausreißer, die schon in der Langzeitstatistik stehen, bleiben
> dort stehen. Siehe
> [`docs/sessionbericht-2026-08-12.md`](docs/sessionbericht-2026-08-12.md),
> Abschnitt „Die Altlast in der Statistik".

## Fehlersuche: die Waage misst Unsinn

Drei Diagnose-Entities beantworten die Frage, wo es klemmt. Sie stehen in HA
unter "Diagnose" und werden bei jeder Messung aktualisiert.

**1. "Waage eG Kalibrierfaktor"** — der wichtigste Wert. An diesem Aufbau
gemessen: **−20.874 counts/kg** (Stand 10.08.2026).

> Frühere Kalibrierungen dieses Aufbaus ergaben −17.900 und −20.840. Die
> Streuung kommt von der Kalibrierung selbst, nicht von der Hardware; der
> Erwartungsbereich für die Fehlersuche ist deshalb **−18.000 bis −21.000**.

| Anzeige | Bedeutung |
|---|---|
| **exakt 3.500** | Eindeutig: die Kalibrierung ist auf die Platzhalter zurückgefallen. Neu kalibrieren. |
| sehr groß (>100.000) | Beim Kalibrieren war der Span zu klein — Gewicht lag nicht auf, oder es wurde nicht ~1 min gewartet. Neu kalibrieren. |
| ~±18.000 bis ±21.000 | Die Umrechnung ist in Ordnung, weiter bei Punkt 2. |
| rund die **Hälfte** des Erwartungswerts | Nur „Kalibrieren Referenzgewicht" gedrückt, der Nullpunkt fehlt. Erkennbar auch an leerem „Kalibriert bei". Beide Schritte fahren. |

Der Wert **3.500** ist die schärfste Diagnose, weil er sich exakt aus den
Platzhaltern ergibt (1750 counts / 0,5 kg) und mit keiner realen Kalibrierung
zufällig zusammenfällt.

**Wann das passiert:** nicht nur bei Stromausfall ohne `restore_from_flash`.
**Auch ein Flash, der neue `globals` hinzufügt, setzt die Kalibrierung zurück** —
real beobachtet am 03.08.2026. Nach jedem Flash also den Kalibrierfaktor prüfen.

### Nach dem Verlust immer BEIDE Schritte kalibrieren

Wird nur „Kalibrieren Referenzgewicht" gedrückt, bleibt der Nullpunkt auf dem
Platzhalter 0 und der Kalibrierfaktor landet bei rund der Hälfte des richtigen
Werts. Die Anzeige sieht dabei **korrekt aus, solange das Prüfgewicht aufliegt**,
und ist bei jeder anderen Last falsch.

Der verräterische Hinweis steht in **„Kalibriert bei": unbekannt.** Die
Temperatur wird ausschließlich beim Nullpunkt gespeichert — steht dort nichts,
wurde „Kalibrieren 0 kg" nie ausgeführt.

**Zweite Folge davon:** Ohne „Kalibriert bei" fehlt der Bezugspunkt der
Temperaturkompensation, und die schaltet sich ab. Zu erkennen an
„Temperaturkorrektur": steht die dauerhaft auf 0,000, obwohl ein Koeffizient
eingetragen ist, fehlt der Nullpunkt.

### Warum der Faktor negativ ist — und warum das in Ordnung ist

Ein **negatives** Vorzeichen heißt nur: mehr Last erzeugt einen *kleineren*
Rohwert. Die Signalpolarität ist vertauscht, in der Regel weil A+ und A− (bzw.
bei der Halbbrücken-Ringschaltung die Reihenfolge der mittleren Adern)
andersherum angeschlossen sind.

**Die Umrechnung stört das nicht.** Sie benutzt ausschließlich die Differenz
`calib_raw_ref − calib_raw_zero`; ein negativer Rohwert-Hub geteilt durch einen
negativen Span ergibt wieder ein positives Gewicht. Anzeige, Tara und die
Span-Plausibilitätsprüfung (die mit `fabs` arbeitet) sind alle darauf getestet.

**Ein Punkt bleibt trotzdem zu prüfen: der ADC-Vorrat.** Weil die Last den
Rohwert nach *unten* zieht, ist die Frage, wie weit es bis zum unteren
Anschlag (−8.388.608) noch ist. Schau in "Waage eG Rohwert" bei leerer Waage:

```
verbleibende Kapazität in kg = (Rohwert + 8.388.608) / 20.874
```

Bei einem Rohwert von z. B. +600.000 sind das rund 430 kg Vorrat — völlig
unkritisch; die real gemessenen ~26.700 counts bei leerer Waage ergeben rund
400 kg, das mechanische Limit von 200 kg greift also lange vorher. Läge der Rohwert dagegen schon tief im Negativen, könnte ein voller
Stock den Wandler in die Sättigung fahren; dann A+/A− tauschen und neu
kalibrieren.

**2. "Waage eG Rohwert Streuung"** — die Standardabweichung der Rohwerte
innerhalb des Messintervalls, in counts. Das ist der Nachfolger der alten
Faustregel "springt der Rohwert um Tausende, liegt es an der Hardware": Seit
"Rohwert" ein Intervallmittel ist, sieht er auch dann ruhig aus, wenn das
Signal wild springt — diese Entity zeigt es trotzdem.

Bei **unbelasteter, ruhender** Waage und kurzem Intervall sollten hier einige
hundert counts stehen (bei −20.874 counts/kg sind 200 counts rund 10 g).
Deutlich mehr, ohne dass Wind, Durchsicht oder ein Wetterwechsel dahinterstehen,
heißt Hardware und nicht Konfiguration: Verkabelung, Wackelkontakt in der
Junction-Box, Halbbrücken- statt Vollbrückenzellen, oder eine zu schwache
Speisung (siehe
[`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md), Abschnitt
zum Eingangswiderstand).

> **Bei langem Intervall ist ein hoher Wert normal.** Über 6 Stunden steckt der
> Temperaturgang mit im Fenster und dominiert die Streuung; in der Simulation
> ergeben 300 counts Rauschen plus 1 K/h Erwärmung zusammen rund 1.200 counts.
> Zum Beurteilen der Hardware also entweder kurz aufs Intervall `1` gehen oder
> die Entity über Tage mit sich selbst vergleichen.

Am direktesten bleibt trotzdem das Log — siehe unten.

**3. "Waage eG Kalibriert mit"** — das Referenzgewicht, mit dem tatsächlich
kalibriert wurde. Steht hier 0,5 obwohl du mit 10 kg kalibriert hast, wurde der
Kalibrier-Button nicht wirksam ausgeführt (Log prüfen).

### Das Log ist ergiebiger als jede Entity

Der HX711-Treiber loggt **jeden** Rohwert im Sekundentakt:

```
[D][hx711:031]: '': Got value 412345
```

Damit siehst du live, wie ruhig das Signal wirklich ist. Zwei Warnungen deuten
direkt auf Verkabelung hin:

```
[W][hx711:039]: HX711 is not ready for new measurements yet!
[W][hx711:062]: HX711 DOUT pin not high after reading (data 0x...)!
```

Die Kalibrier-Buttons loggen ebenfalls, was sie gespeichert haben — oder warum
sie abgebrochen sind (`Span nur ... counts`, `Referenzgewicht ungueltig`).

## Konfiguration prüfen

```bash
esphome config waage-eg.yaml
esphome config waage-stock2.yaml
esphome config waage-stock3.yaml
```

Geprüft gegen ESPHome 2026.6.5 - siehe Abschnitt "Validierung" in den
Projektnotizen.

Die Rechnung der Temperaturkompensation lässt sich zusätzlich ohne Hardware
und ohne ESPHome prüfen - das Programm bindet die echte Header-Datei ein:

```bash
cd tests
g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
```

Bei der Umstellung auf packages wurde die aufgelöste Konfiguration von
`waage-eg` vorher und nachher verglichen: identisch bis auf den zusätzlichen
`substitutions:`-Block. Wer eine Änderung an der Basis nachprüfen will, kann
das genauso machen:

```bash
esphome config waage-eg.yaml > vorher.txt
# ... Änderung ...
esphome config waage-eg.yaml > nachher.txt
diff vorher.txt nachher.txt
```
