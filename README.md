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

1. Waage komplett leer räumen → 1 min warten → **"Waage eG Kalibrieren 0kg"**
2. 500-g-Referenzgewicht auflegen → 1 min warten → **"Waage eG Kalibrieren 0,5kg"**
3. Fertig. Ob es geklappt hat, steht im ESPHome-Log (`Nullpunkt gesetzt: ...`
   bzw. `0,5kg-Punkt gesetzt: ... (Span ...)`).

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

Nach einem Neustart kommt der erste Wert bereits nach ~1 Minute, unabhängig vom
eingestellten Intervall - du musst also nicht bis zum Ablauf einer vollen
Periode warten, um zu sehen, ob das Gerät läuft.

## Konfiguration prüfen

```bash
esphome config waage-eg.yaml
```

Geprüft gegen ESPHome 2026.6.5 - siehe Abschnitt "Validierung" in den
Projektnotizen.
