# Sessionbericht 04.08.2026

Umstellung der ESPHome-Konfiguration auf `substitutions` + `packages`, damit
weitere Stöcke ohne Code-Duplizierung dazukommen können. Ein Thema, aber mit
einigen Fallstricken, die für spätere Arbeiten relevant bleiben.

**Anlagenzustand unverändert:** ein Stock produktiv („Waage eG"), Steckbrett am
Netzteil. An Hardware, Verdrahtung und Messlogik wurde **nichts** geändert.

**Nicht Teil dieser Session:** HA-Helfer, Automationen und Dashboard für die
neuen Stöcke. Das kommt als eigener Schritt, jetzt wo die YAML-Basis steht.

---

## 1. Was sich geändert hat

Vorher eine Datei mit allem, jetzt eine Basis plus eine kleine Datei pro Stock:

```
packages/waage-basis.yaml    776 Zeilen   gesamte gemeinsame Logik
waage-eg.yaml                 59 Zeilen   Stock 1 (vorher: 693 Zeilen)
waage-stock2.yaml             61 Zeilen   Stock 2
waage-stock3.yaml             61 Zeilen   Stock 3
```

In der Basis liegen Globals, HX711 samt Filterkette, Kalibrier- und
Tara-Buttons, das Wägezellen-Template, die Diagnose-Sensoren, der
Durchsichtmodus und der Taktgeber. In den Stock-Dateien steht nur noch, was
den jeweiligen Stock unterscheidet.

**Parametrisiert (14 substitutions):**

| Gruppe | Substitutions |
|---|---|
| Identität | `geraete_name`, `anzeige_name`, `ap_ssid` |
| Board | `board` |
| Pins | `pin_hx711_dout`, `pin_hx711_clk`, `pin_temperatur`, `pin_taster`, `pin_led` |
| Kalibrier-Startwerte | `start_calib_raw_zero`, `start_calib_raw_ref`, `start_calib_kg_ref` |
| Einstell-Entities | `start_referenzgewicht`, `start_messintervall`, `start_durchsicht_dauer` |

Die Vorgaben in der Basis sind exakt die bisherigen Werte von `waage-eg`. Eine
Stock-Datei überschreibt davon, was sie braucht.

Unverändert und bewusst so gelassen: `restore_from_flash: true`, kein
`calibrate_linear`, DOUT auf D6/GPIO12 (GPIO16/D0 bleibt für den
Deep-Sleep-Weckpfad frei), deutsche Namen und Kommentare.

---

## 2. Nachweis, dass `waage-eg` unverändert ist

Das war die eigentliche Anforderung — die Umstellung durfte weder Kalibrierung
noch Entity-IDs in HA anfassen. Nachgewiesen mit ESPHome **2026.6.5**, indem die
*aufgelöste* Konfiguration vor und nach der Umstellung verglichen wurde:

```bash
esphome config waage-eg.yaml > vorher.txt     # alter Stand
esphome config waage-eg.yaml > nachher.txt    # nach der Umstellung
diff vorher.txt nachher.txt
```

**Ergebnis:** genau ein hinzugefügter Block — die 16 Zeilen `substitutions:`.
Null Änderungen an allem anderen. `esphome config` löst Packages, Substitutions
und Secrets vollständig auf, der Vergleich erfasst also den kompletten
Gerätezustand und nicht nur die Textform.

Damit sind identisch geblieben:

- `name: waage-eg` und `friendly_name: Waage eG`
- alle **17** in HA sichtbaren Entity-Namen (Gewicht, Temperatur, Rohwert,
  Kalibrierfaktor, Kalibriert bei, Kalibriert mit, Betriebszeit, WLAN-Signal,
  Verbindung, Jetzt messen, Tara, Kalibrieren 0kg, Kalibrieren
  Referenzgewicht, Referenzgewicht, Messintervall, Durchsichtdauer,
  Durchsichtmodus)
- alle Globals mit ihren IDs und `initial_value`

**Diese Technik ist wiederverwendbar.** Jede künftige Änderung an der Basis
lässt sich so gegen jeden Stock prüfen, bevor geflasht wird. Bei einem Gerät,
das produktiv Daten sammelt, ist das deutlich belastbarer als YAML-Lesen.

---

## 3. Erkenntnisse zu ESPHome-Packages

Vier Dinge, die beim Aufteilen nicht offensichtlich waren und die man beim
nächsten Mal nicht neu herausfinden muss.

### Stock-Dateien müssen im Root bleiben

Das ESPHome Device Builder Add-on listet **ausschließlich** die YAML-Dateien
direkt in `/config/esphome/` als flashbare Geräte. Unterverzeichnisse liest es
nur über `packages:` / `!include`.

Daraus folgt die Aufteilung: Stock-Dateien in den Root, Basis nach `packages/`.
Läge die Basis im Root, tauchte sie im Dashboard als Pseudo-Gerät auf, das man
nicht flashen kann. Beim Kopieren ins Add-on-Verzeichnis muss `packages/` mit —
sonst bricht das Kompilieren mit einem Fehler zum fehlenden Include ab.

### `!secret` löst gegen die Stock-Datei auf, nicht gegen das Package

`!secret` in `packages/waage-basis.yaml` sucht die `secrets.yaml` im Verzeichnis
der **geflashten Stock-Datei**, nicht im Package-Verzeichnis. `secrets.yaml`
bleibt also unverändert neben `waage-eg.yaml` liegen (`/config/esphome/`) und
gehört **nicht** nach `packages/`.

Nebenwirkung, die kurz verwirrt: `esphome config packages/waage-basis.yaml`
direkt aufzurufen schlägt fehl mit

```
Error reading file packages/secrets.yaml: [Errno 2] No such file or directory
```

Das ist **erwartet und kein Fehler** — die Basis ist kein Gerät. Geprüft werden
immer die Stock-Dateien.

### Werte aus der Stock-Datei haben Vorrang vor denen aus dem Package

Gilt für Substitutions ebenso wie für normale Konfigurationsblöcke. Das ist die
Grundlage dafür, dass die Basis brauchbare Vorgaben mitbringen kann, und
gleichzeitig der Ausweg für Ausnahmen. Ein Stock kann so einen eigenen API-Key
bekommen, ohne dass die Basis das vorsehen muss:

```yaml
api:
  encryption:
    key: !secret api_key_stock2
```

In den Stock-Dateien ist dieser Block auskommentiert vorbereitet.

### Substitutions können nicht in `!secret` hinein

`!secret ${irgendwas}` funktioniert nicht — Secret-Namen lassen sich nicht
parametrisieren. Deshalb teilen sich alle Stöcke standardmäßig WLAN, API-Key
und OTA-Passwort. Das ist zulässig; wer es pro Gerät trennen will, braucht den
Override-Block von oben.

Für `waage-eg` **bewusst nicht** gemacht: ein neuer API-Key erzwingt ein
erneutes Hinzufügen des Geräts in HA und riskiert genau die Entity-IDs, die
erhalten bleiben sollten.

---

## 4. Namensschema für weitere Stöcke

Angelegt sind `waage-stock2` und `waage-stock3`, einsatzfähig und validiert.

**Empfehlung für die Praxis ist trotzdem der Standort** — `waage-garten`,
`waage-streuobst`, `waage-hausecke`.

Der Grund ist derselbe, der die ganze Migration eingeschränkt hat:
`geraete_name` und `anzeige_name` bilden zusammen mit den `name:`-Feldern die
entity_id in HA (`"Waage eG"` + `"Gewicht"` → `sensor.waage_eg_gewicht`). Wird
später umbenannt, legt HA **neue** Entities an; Verlauf, Helfer, Automationen
und Dashboard hängen danach an den alten, toten IDs.

Durchnummerierte Namen laden genau dazu ein: sobald ein Volk eingeht, verkauft
oder umgesetzt wird, passt die Nummer nicht mehr zur Realität — und dann steht
man vor der Wahl zwischen falschem Namen und verlorener Historie. Ein
Standortname bleibt richtig, auch wenn das Volk darin wechselt.

**Solange ein Stock noch nicht geflasht ist, kostet das Umbenennen drei Zeilen.
Danach kostet es die Historie.** Deshalb vor dem ersten Flash entscheiden.

`waage-eg` bleibt aus demselben Grund unangetastet.

---

## 5. Zur Kalibrierung beim nächsten Flash

Die Globals sind mit identischen IDs übernommen worden, und ESPHome schlüsselt
die gespeicherten `restore_value`-Werte über die ID des Globals auf. Die
Kalibrierung **sollte** den Flash also überstehen.

Verlassen sollte man sich darauf nicht. Am 03.08. ist genau das schiefgegangen
(Faktor fiel auf den Platzhalter 3.500). Deshalb unverändert gültig:

**Nach jedem Flash den Kalibrierfaktor prüfen.** Erwartet werden rund
**−20.840 counts/kg**. Steht dort etwas anderes: **beide** Kalibrierschritte
fahren, nicht nur den Referenzpunkt — nur den Referenzpunkt zu setzen ergibt
einen etwa halbierten Faktor bei plausibel aussehender Anzeige. Erkennungs­zeichen
dafür ist ein leeres „Kalibriert bei".

Für neue Stöcke sind die Startwerte bewusst so gelassen, dass der rechnerische
Faktor (3.500 counts/kg) weit vom Erwartungswert (~18.000–21.000 bei 4×50 kg)
entfernt liegt. So fällt sofort auf, wenn noch nicht oder nicht vollständig
kalibriert wurde.

---

## Offene Punkte

**Aus dieser Session neu:**
- **Namen der neuen Stöcke festlegen** — vor dem ersten Flash, siehe Abschnitt 4.
- **HA-Seite für die neuen Stöcke:** Helfer, Automationen, Dashboard. Bewusst
  auf einen eigenen Schritt vertagt. Die bestehenden Helfer und Automationen
  sind auf `waage-eg`-Entities verdrahtet und müssen pro Stock dupliziert oder
  auf eine Blueprint-/Template-Lösung umgestellt werden.
- Für die neuen Stöcke ist Hardware noch nicht aufgebaut — die YAML-Dateien
  sind vorbereitet, nicht in Betrieb.

**Aus früheren Sessions weiterhin offen:**
- **Neu kalibrieren, beide Schritte** (siehe Abschnitt 5 und
  `sessionbericht-2026-08-03.md`).
- **Schwarm-Alarm gegen den Durchsichtmodus sperren:** Bedingung
  `switch.waage_eg_durchsichtmodus == off for: 00:30:00` fehlt noch. Löst sonst
  nach jeder Durchsicht fälschlich aus.
- `input_number.leergewicht_beute` und `input_number.mindestgewicht_mit_futter`
  stehen beide noch auf 0,0 kg.
- Temperaturkompensation: Koeffizient ist mit +34,5 g/K bestimmt, eingebaut ist
  sie noch nicht.
- Deep Sleep: Pin-Blocker beseitigt, Rest siehe `deep-sleep-vorbereitung.md`.

---

## Werkzeugnotizen

- **ESPHome lässt sich zum Prüfen lokal installieren** (`pip install esphome`,
  hier 2026.6.5 in einem venv). `esphome config <datei>.yaml` validiert
  vollständig und braucht weder Hardware noch das Add-on noch einen
  Compiler-Durchlauf. Für eine Änderung an einer produktiven Konfiguration ist
  das der schnellste Weg zu echter Sicherheit.
- `esphome config` **maskiert Secret-Werte** in der Ausgabe mit ANSI-Codes
  (`\033[8m...\033[28m`). Beim Vergleichen zweier Ausgaben stört das nicht, beim
  Nachsehen eines konkreten Werts schon — dann `--show-secrets` anhängen.
- Beim Ableiten der dritten Stock-Datei aus der zweiten per `sed` hat eine zu
  gierige Ersetzung `sensor.sensor.waage_stock_3_gewicht` erzeugt. Nach solchen
  Massenersetzungen das Ergebnis durchsehen, gerade in Kommentaren.
- **Repo-Verwechslung:** Die Arbeitssitzung war zunächst an
  `CLAUDE-Testumgebung` angehängt (die Imkerei-Webapp), nicht an
  `Bienenstockwaage`. Beide gehören zum selben Konto und die Namen liegen nah
  beieinander. Wenn die im Kontext beschriebenen Dateien fehlen: erst prüfen,
  ob das richtige Repo offen ist, bevor man sie sucht.
