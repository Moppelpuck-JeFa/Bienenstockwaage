# waage

ESPHome-Konfiguration für die Bienenstockwaage **"Waage eG"** -
ESP8266 (D1 Mini) mit HX711-Wägezellenverstärker und 4 Wägezellen.

## Dateien

| Datei | Inhalt |
|-------|--------|
| [`waage-eg.yaml`](waage-eg.yaml) | Die ESPHome-Konfiguration - der eigentliche Code |
| [`secrets.yaml.example`](secrets.yaml.example) | Vorlage für die Zugangsdaten (kopieren nach `secrets.yaml`) |
| [`waage-eg-notes.md`](waage-eg-notes.md) | Projektnotizen: alle Entscheidungen, Anforderungen, offene Punkte |
| [`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md) | Verkabelung der 4 Zellen, Junction-Box, Kaufkriterien |

`secrets.yaml` selbst ist per `.gitignore` ausgeschlossen und gehört nicht ins Repo.

## Was das Gerät kann

- Gewicht in **kg**, gerundet auf **0,1 kg**
- **Messintervall frei einstellbar aus Home Assistant** (1 Minute bis 7 Tage),
  Voreinstellung 360 min = 6 h
- **Zwei-Punkt-Kalibrierung per Button aus HA** - kein festes `calibrate_linear`
  im YAML, die Kalibrierwerte liegen in `globals` mit `restore_value: yes` und
  überleben einen Neustart
- **Referenzgewicht frei wählbar** (0,1 bis 50 kg) - je schwerer, desto genauer
- **Tara-Button**, unabhängig von der Kalibrierung
- Fallback-Hotspot, OTA-Updates, Webserver auf Port 80

## Inbetriebnahme

1. `secrets.yaml.example` nach `secrets.yaml` kopieren und ausfüllen
   (im ESPHome Device Builder liegt sie unter `/config/esphome/secrets.yaml`)
2. `waage-eg.yaml` ins ESPHome-Verzeichnis legen, kompilieren und flashen
3. Gerät in Home Assistant hinzufügen (der API-Key aus `secrets.yaml`)

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
Absolutwerts liegt realistisch bei ±0,1 bis ±0,5 kg, begrenzt durch
Temperaturdrift, Kriechen und vor allem den Eckenfehler der Wägezellen (nicht
durch die Elektronik - der HX711 hat hier reichlich Reserve).

Für das, worum es bei einer Stockwaage geht - Gewichts*änderung* über Stunden
und Tage - ist das genau richtig, weil sich diese Fehler bei gleichbleibendem
Aufbau herauskürzen. Die letzte Nachkommastelle sollte man nur nicht als
absolute Wahrheit lesen. Hintergrund in den Projektnotizen und in
[`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md).

Eine leere Waage kann dabei statt 0,0 auch mal ±0,1 anzeigen, wenn sie
thermisch weggedriftet ist - dann hilft ein Druck auf "Tara".

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

Der HX711 selbst misst unabhängig davon weiter im Sekundentakt - das Intervall
steuert nur, wie oft ein Wert **nach Home Assistant** geschickt wird. Ein
kurzes Intervall kostet also keine zusätzliche Messgenauigkeit und ein langes
verschlechtert sie nicht.

### "Die Werte aktualisieren sich so langsam"

Das ist die Voreinstellung: **360 Minuten**. Für den Dauerbetrieb einer
Stockwaage ist das richtig, beim Einrichten stört es.

- **Einzelner Wert jetzt:** Button **"Waage eG Jetzt messen"** drücken. Er
  veröffentlicht Gewicht und alle Diagnose-Entities sofort und startet den
  Intervall-Zähler neu. Das Intervall selbst bleibt unverändert.
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

## Fehlersuche: die Waage misst Unsinn

Drei Diagnose-Entities beantworten die Frage, wo es klemmt. Sie stehen in HA
unter "Diagnose" und werden bei jeder Messung aktualisiert.

**1. "Waage eG Kalibrierfaktor"** — der wichtigste Wert. An diesem Aufbau
gemessen: **−17.900 counts/kg**.

| Anzeige | Bedeutung |
|---|---|
| **exakt 3.500** | Eindeutig: die Kalibrierung ist auf die Platzhalter zurückgefallen. Neu kalibrieren. |
| sehr groß (>100.000) | Beim Kalibrieren war der Span zu klein — Gewicht lag nicht auf, oder es wurde nicht ~1 min gewartet. Neu kalibrieren. |
| ~±17.900 | Die Umrechnung ist in Ordnung, weiter bei Punkt 2. |

Der Wert **3.500** ist die schärfste Diagnose, weil er sich exakt aus den
Platzhaltern ergibt (1750 counts / 0,5 kg) und mit keiner realen Kalibrierung
zufällig zusammenfällt.

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
verbleibende Kapazität in kg = (Rohwert + 8.388.608) / 17.900
```

Bei einem Rohwert von z. B. +600.000 sind das rund 500 kg Vorrat — völlig
unkritisch. Läge der Rohwert dagegen schon tief im Negativen, könnte ein voller
Stock den Wandler in die Sättigung fahren; dann A+/A− tauschen und neu
kalibrieren.

**2. "Waage eG Rohwert"** — der gefilterte HX711-Zählwert. Bei **unbelasteter,
ruhender** Waage sollte der über Minuten nur um einige hundert counts wandern.
Springt er um Tausende, liegt es an der Hardware und nicht an dieser
Konfiguration: Verkabelung, Wackelkontakt in der Junction-Box, Halbbrücken-
statt Vollbrückenzellen, oder eine zu schwache Speisung (siehe
[`docs/waegezellen-verkabelung.md`](docs/waegezellen-verkabelung.md), Abschnitt
zum Eingangswiderstand).

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
```

Geprüft gegen ESPHome 2026.6.5 - siehe Abschnitt "Validierung" in den
Projektnotizen.
