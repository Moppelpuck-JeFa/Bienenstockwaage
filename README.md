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

- Gewicht in **kg**, gerundet auf **0,5 kg**, alle **6 Stunden** an Home Assistant
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

## Konfiguration prüfen

```bash
esphome config waage-eg.yaml
```

Geprüft gegen ESPHome 2026.6.5 - siehe Abschnitt "Validierung" in den
Projektnotizen.
