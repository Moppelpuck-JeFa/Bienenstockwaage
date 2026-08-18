# Deep Sleep vorbereiten

> **Dieses Dokument beschreibt den ESP8266 (`waage-eg`).** Für den zweiten
> Stock `stockwaage` auf ESP32 gilt zusätzlich **Abschnitt 9** — dort fallen
> mehrere der Blocker unten ersatzlos weg, und andere kommen hinzu. Wer am
> ESP32 arbeitet, liest Abschnitt 9 zuerst.

Deep Sleep ist aktuell **blockiert**, und zwar aus einem Grund, der sich beim
nächsten Öffnen der Hardware in fünf Minuten beseitigen lässt. Dieses Dokument
hält fest, was dafür zu tun ist — und was danach immer noch im Weg steht.

**Stand der Dinge:** Die Anlage liegt noch auf dem Steckbrett am Netzteil. Der
Zielaufbau ist ein Standort im Garten, versorgt aus einem **NiMH-Akku mit
2600 mAh über eine Ladelogik am Solarpanel**. Damit ist Deep Sleep kein
Nice-to-have, sondern die Voraussetzung dafür, dass der Aufbau über den Herbst
kommt — die Rechnung dazu steht in Abschnitt 6.

Weil noch nichts fest verbaut ist, sind die Änderungen aus diesem Dokument
gerade so billig, wie sie je sein werden: auf dem Steckbrett ist der Pin-Wechsel
ein umgestecktes Kabel.

## Kurzfassung

| | |
|---|---|
| **Jetzt zu tun** | HX711 `DOUT` von `D0` auf `D6` umstecken, `D0`↔`RST` überbrücken (über 470 Ω), Pull-up 10 kΩ von `D1` (SCK) nach 3V3 |
| **YAML dazu** | genau eine Zeile: `dout_pin: D0` → `dout_pin: D6` |
| **Kalibrierung** | bleibt gültig — der Rohwert hängt nicht davon ab, welcher GPIO ihn einliest |
| **Danach möglich** | Deep Sleep per YAML nachrüstbar, ohne die Hardware nochmal anzufassen |
| **Der eigentliche Stromfresser** | nicht der ESP, sondern HX711 + Wägezellenbrücke (~5,8 mA dauerhaft) |

## 1. Warum es heute nicht geht: GPIO16

Der ESP8266 kann aus dem Deep Sleep **ausschließlich über den RTC-Timer**
aufwachen, und der weckt den Chip, indem er `GPIO16` auf Masse zieht — deshalb
muss `GPIO16` mit `RST` verbunden sein. Bestätigt im ESPHome-Quelltext,
`components/deep_sleep/deep_sleep_esp8266.cpp`:

> The ESP8266 can only wake from deep sleep through the RTC timer
> (via GPIO16 -> RST).

An `GPIO16` hängt bei uns aber `DOUT` des HX711 (`D0`). Solange das so ist,
kann das Gerät zwar einschlafen, aber nie wieder aufwachen.

Auf dem D1 Mini ist die Brücke `D0`→`RST` **nicht** ab Werk gesetzt; sie muss
von Hand gelegt werden.

## 2. Der eigentliche Blocker: HX711 und Brücke schlafen nicht mit

Das ist der Punkt, der die ganze Übung entscheidet — und der beim Planen leicht
übersehen wird, weil er nichts mit dem ESP zu tun hat.

**Der ESPHome-HX711-Treiber kennt keinen Power-Down.** In
`components/hx711/hx711.cpp` gibt es genau `setup()`, `update()` und
`read_sensor_()`; nirgends wird `PD_SCK` länger als eine Taktflanke hochgehalten.
Der HX711 bleibt also durchgehend aktiv, auch wenn der ESP schläft. Und mit ihm
die Brückenspeisung.

Grobe Rechnung mit unseren Werten (Brücke ~1 kΩ laut Ohmmeter-Gegenprobe,
HX711 speist mit ~4,3 V):

| Verbraucher | Strom |
|---|---|
| Wägezellenbrücke (4,3 V / 1 kΩ) | ~4,3 mA |
| HX711 aktiv | ~1,5 mA |
| ESP8266 im Deep Sleep | ~0,02 mA |
| **Summe mit schlafendem ESP** | **~5,8 mA** |

Zum Vergleich der Wachbetrieb: bei einem Weckfenster von 20 s pro Stunde und
~80 mA im Betrieb sind das im Mittel ~0,45 mA. **Der schlafende ESP verbraucht
also rund ein Zehntel dessen, was HX711 und Brücke nebenher ziehen.** Deep Sleep
allein bringt damit fast nichts — erst der Power-Down des HX711 rechtfertigt den
Aufwand. Die vollständige Bilanz gegen den geplanten 2600-mAh-Akku steht in
Abschnitt 6.

### Der HX711 kann es, man muss es ihm nur sagen

Laut Datenblatt geht der HX711 in den Power-Down (< 1 µA), sobald `PD_SCK`
länger als **60 µs** high bleibt; dabei schaltet auch der interne Regler ab, der
die Brücke speist. Beide Posten aus der Tabelle oben verschwinden also
gleichzeitig.

Im Deep Sleep treibt der ESP seine GPIOs nicht mehr — `SCK` würde floaten und
der Power-Down wäre nicht garantiert. **Ein Pull-up von 10 kΩ zwischen `D1`
(SCK) und 3V3 löst das**: im Schlaf hält er die Leitung high und damit den HX711
im Power-Down, im Betrieb stört er den getriebenen Takt nicht.

Das ist die billigste Lösung. Die deterministische Alternative wäre ein
High-Side-P-MOSFET, der die Versorgung des HX711-Moduls ganz abwirft,
geschaltet über einen freien GPIO (`D7`/GPIO13 ist frei). Mehr Bauteile, dafür
kein Verlass auf ein Datenblatt-Detail.

**Ungeprüft und vor dem Batteriebetrieb zu messen:** ob das Breakout in der
Praxis wirklich auf µA heruntergeht. Manche Module haben zusätzliche
LEDs oder Spannungsteiler, die unabhängig vom HX711 weiterziehen. Ein
Multimeter in der Versorgungsleitung beantwortet das in einer Minute — und diese
Messung entscheidet, ob die 5-Monate-Zeile überhaupt erreichbar ist.

## 3. Hardware-Änderungen

| # | Änderung | Warum |
|---|---|---|
| 1 | HX711 `DOUT` von `D0` (GPIO16) auf **`D6` (GPIO12)** | macht GPIO16 für den Weckpfad frei |
| 2 | Brücke **`D0` ↔ `RST`**, über einen **470-Ω-Widerstand** statt Draht | ohne sie kein Aufwachen; der Widerstand lässt den USB-Serial-Wandler beim Flashen weiter an `RST` |
| 3 | Pull-up **10 kΩ von `D1` (SCK) nach 3V3** | hält den HX711 im Schlaf im Power-Down (siehe oben) |

`GPIO12` ist frei und unkritisch: kein Boot-Strapping-Pin, interruptfähig, nicht
belegt. Ebenso frei wären `D2` (GPIO4) und `D7` (GPIO13) — `D7` bleibt besser
reserviert, falls doch der MOSFET-Weg kommt.

`CLK` bleibt auf `D1` (GPIO5), der DS18B20 auf `D5` (GPIO14). An der Belegung
ändert sich sonst nichts.

**Zur 470-Ω-Brücke:** Ein direkter Draht zwischen `D0` und `RST` funktioniert
zum Wecken, kann aber das Flashen über USB blockieren, weil der Serial-Wandler
`RST` dann gegen einen treibenden GPIO ziehen muss. 470 Ω sind niederohmig genug
zum Wecken und hochohmig genug, damit der Reset über USB weiter durchkommt. Wer
lieber auf Nummer sicher geht, setzt statt dessen einen steckbaren Jumper und
zieht ihn zum Flashen.

## 4. YAML: die eine Zeile jetzt, der Rest später

### Jetzt, zusammen mit dem Umlöten

```diff
   - platform: hx711
     id: hx711_raw_counts
     internal: true
-    dout_pin: D0
+    dout_pin: D6
     clk_pin: D1
     gain: 128
```

Mehr nicht — geprüft mit `esphome config` gegen ESPHome 2026.6.5, vorher wie
nachher *Configuration is valid*. Wichtig dabei:

- **Die Kalibrierung bleibt gültig.** Der HX711 liefert dieselben counts,
  unabhängig davon, an welchem GPIO sie eingelesen werden. `calib_raw_zero`,
  `calib_raw_ref` und `calib_kg_ref` bleiben unangetastet, `restore_from_flash`
  bringt sie über den Neustart. Es muss **nicht** neu kalibriert werden.
- Die Kommentare im YAML zu GPIO16 (Interrupts, Deep-Sleep-Weckpin) beschreiben
  danach den alten Zustand und gehören mit angepasst.
- Der Umbau **unterbricht die laufende Temperaturdrift-Messung** und stört über
  das Anfassen der Mechanik den Kriechvorgang. Deshalb erst umbauen, wenn die
  Driftreihe ausgewertet ist.

### Später, wenn Deep Sleep wirklich gebraucht wird

Der folgende Entwurf ist mit `esphome config` gegen **ESPHome 2026.6.5** geprüft:
*Configuration is valid.* Ein vollständiger `esphome compile` war wie schon in
den Projektnotizen beschrieben nicht möglich, die PlatformIO-Registry ist in der
Build-Umgebung gesperrt. Die Syntax stimmt also, das Laufzeitverhalten auf dem
Gerät bleibt zu prüfen.

Ergänzung im bestehenden `esphome:`-Block:

```yaml
esphome:
  name: waage-eg
  friendly_name: "Waage eG"
  on_boot:
    priority: -100
    then:
      - wait_until:
          condition: api.connected
          timeout: 20s
      - if:
          condition:
            binary_sensor.is_on: wachhalten
          then:
            - deep_sleep.prevent: schlaf
```

**In den bestehenden `binary_sensor:`-Block einhängen** — ein zweiter Block mit
demselben Schlüssel ist ein YAML-Fehler (`Duplicate key "binary_sensor"`) und
genau die Falle, in die man beim Zusammenkopieren tritt:

```yaml
binary_sensor:
  - platform: status
    name: "Verbindung"
    entity_category: diagnostic

  # Spiegelt einen HA-Schalter: solange er an ist, wird nicht geschlafen -
  # sonst waeren Tara, Kalibrierung und OTA nicht mehr erreichbar.
  - platform: homeassistant
    id: wachhalten
    entity_id: input_boolean.waage_wachhalten
```

Neu am Ende der Datei:

```yaml
deep_sleep:
  id: schlaf
  run_duration: 30s        # Sicherheitsnetz: schlafen auch ohne HA-Verbindung
  sleep_duration: 60min
```

Dazu gehört ein `input_boolean.waage_wachhalten` in Home Assistant.

Verfügbare Aktionen laut `components/deep_sleep/__init__.py`:
`deep_sleep.enter`, `deep_sleep.prevent`, `deep_sleep.allow`.

## 5. Was am Betriebskonzept kippt

Deep Sleep ist kein Schalter, den man umlegt — es zieht mehrere der bewussten
Entscheidungen aus den Projektnotizen mit:

**Das Messintervall wandert.** Der `interval: 60s`-Taktgeber und das Global
`minuten_seit_messung` werden überflüssig; die Schlafdauer *ist* das Intervall.
Die Number-Entity „Messintervall" müsste statt des Zählers die `sleep_duration`
setzen.

**Maximal ~71 Minuten am Stück.** Der RTC-Timer des ESP8266 bekommt die Zeit in
Mikrosekunden in einem 32-Bit-Wert, das deckelt bei 4.294.967.295 µs ≈ 71,6 min.
Die aktuelle Voreinstellung von 360 min ist damit nicht darstellbar.

**Und ESPHome fängt das für den ESP8266 nicht ab.** Der Validator in
`components/deep_sleep/__init__.py` begrenzt nur BK72XX (36 h) und Zephyr
(49 Tage); für den ESP8266 gibt es keine Prüfung. Nachgestellt mit ESPHome
2026.6.5: `sleep_duration: 360min` läuft anstandslos durch `esphome config`.
Der Fehler fällt also erst am Gerät auf, und dort als „die Waage wacht viel zu
früh auf" statt als Konfigurationsfehler.

Praktisch ist das kein Hindernis: die Saison-Automation stellt für Juli/August
ohnehin schon 60 min ein. Für längere Intervalle müsste man mehrere
Schlafphasen verketten und den Restzähler über einen `restore_value`-Global
mitführen.

**Die 60-Sekunden-Filterkette passt nicht mehr.** `median(5)` +
`sliding_window_moving_average(12)` brauchen ~60 s Einschwingzeit — bei einem
30-s-Weckfenster kommt hinten nie ein Wert heraus. Das Fenster muss kürzer
werden. Das ist vertretbar: laut Projektnotizen sind 0,1 kg ≈ 1.790 counts
gegenüber ~21 counts ADC-Rauschen, die lange Mittelung dient der Mechanik, nicht
dem Wandler. Dazu kommt, dass der HX711 nach dem Power-Down erst wieder
einschwingen muss.

**Die Buttons sind nicht mehr jederzeit erreichbar.** Tara, beide
Kalibrier-Buttons und „Jetzt messen" funktionieren nur im Weckfenster. Dafür ist
der `wachhalten`-Schalter oben da — und er ist zugleich die einzige Möglichkeit,
noch ein OTA-Update einzuspielen. Ohne ihn ist das Gerät nach dem ersten Flash
mit aktivem Deep Sleep praktisch nur noch über USB erreichbar.

**Der Durchsicht-Taster funktioniert so nicht mehr.** Der ESP8266 kann aus dem
Deep Sleep **nur** über den RTC-Timer aufwachen, nicht über einen GPIO. Ein
Momenttaster, der während der Schlafphase gedrückt wird, ist danach spurlos
weg — und genau dann steht man am Stock. Auch die LED erlischt, weil die GPIOs
im Schlaf nicht mehr getrieben werden.

Zwei Wege:

- **Rastenden Schalter statt Momenttaster** (Kipp- oder Schiebeschalter). Der
  hält seinen Zustand über die Schlafphase; das Gerät liest ihn bei jedem
  Aufwachen und überspringt die Veröffentlichung, solange er an ist. Die
  LED-Anzeige lässt sich dann **direkt über den Schalter** verdrahten, ohne
  GPIO — dann leuchtet sie auch im Schlaf, was der eigentliche Zweck ist. Der
  Preis: das Blinken in den letzten Minuten entfällt, und die Zeitbegrenzung
  auch (der Schalter bleibt an, bis man ihn umlegt).
- **Taster zusätzlich auf `RST`** legen, damit ein Druck das Gerät weckt. Dann
  weiß das Gerät nach dem Neustart aber nicht, *warum* es wach ist, und
  `durchsicht_restminuten` ist als nicht gesichertes Global ohnehin weg.
  Funktioniert nur mit `restore_value: yes` und einer Auswertung des
  Reset-Grunds — deutlich mehr Aufwand für wenig Gewinn.

**Empfehlung:** Beim Umstieg auf Deep Sleep den Momenttaster gegen einen
rastenden Schalter tauschen und die LED an den Schalter hängen. Bis dahin — im
Netzbetrieb — ist die jetzige Lösung die komfortablere.

**Kleinkram:** `web_server` sollte raus (RAM und Strom). `wifi` sollte
`fast_connect: true` und eine feste `manual_ip` bekommen — der WLAN-Scan ist
sonst der größte Einzelposten im Weckfenster. Die `uptime`-Entity verliert ihren
Sinn, weil sie bei jedem Aufwachen neu beginnt.

## 6. Zielaufbau: Solar mit 2600-mAh-NiMH

Bei Solar ist nicht die Akkulaufzeit die Frage, sondern die **Tagesbilanz im
schlechtesten Monat**. In Dezember liefert ein Panel in Deutschland grob ein
Zehntel des Juni-Ertrags — danach muss ausgelegt werden, sonst steht die Waage
ab Oktober regelmäßig still.

| Variante | mittlerer Strom | pro Tag | Pack allein (2600 mAh) |
|---|---|---|---|
| A — heute: dauerhaft wach | ~86 mA | ~2.060 mAh | **~1,3 Tage** |
| B — A + `power_save_mode` | ~26 mA | ~620 mAh | ~4 Tage |
| C — Deep Sleep, HX711 bleibt an | ~6,3 mA | ~150 mAh | ~17 Tage |
| D — Deep Sleep + HX711-Power-Down | ~0,5 mA | ~12 mAh | **~200 Tage** |

Variante A braucht rund 10 Wh pro Tag. Das im Dezember zuverlässig zu ernten
heißt Panel und Akku in einer Größenordnung, die zum Rest des Aufbaus nicht mehr
passt. Variante D braucht ~0,06 Wh pro Tag — das deckt ein kleines Panel auch im
Dezember mit Faktor 20 Reserve.

**Variante B ist der billigste Zwischenschritt:** `power_save_mode` im
`wifi:`-Block ist eine Zeile, ohne Hardware und ohne Deep Sleep, und nimmt den
größten Einzelposten (den dauerhaft sendenden ESP) heraus. Sie reicht für Solar
aber nicht, weil HX711 und Brücke mit 5,8 mA als Sockel stehen bleiben — das
sind allein schon 139 mAh pro Tag. **Erst der HX711-Power-Down aus Abschnitt 2
macht Solar einfach.**

### Drei NiMH- und Board-Details, die den Plan sonst unterlaufen

**Selbstentladung schlägt bei Variante D den Verbrauch.** Gewöhnliche
NiMH-Zellen verlieren bis zu 20–30 % pro Monat, bei 2600 mAh also grob
**17–22 mAh pro Tag** — mehr, als das Gerät dann selbst zieht. Zellen mit
geringer Selbstentladung (LSD-/eneloop-Typ, ~0,5 %/Monat ≈ 0,4 mAh/Tag) sind
damit keine Komfortfrage, sondern entscheiden, ob die Optimierung überhaupt
ankommt.

**Nicht unter 0 °C laden.** NiMH nimmt Ladung bei Frost nicht mehr auf; der
Strom geht in Gasung und zerstört die Zelle. Eine Ladelogik für den Garten
braucht eine Temperatursperre. Ebenso hat NiMH keinen Erhaltungsladezustand wie
Blei — Dauertrickle bei C/10 kocht die Zellen; nötig ist eine echte Abschaltung
oder ein Trickle unterhalb von etwa C/40.

**Das D1-Mini-Board selbst ist im Schlaf oft der größte Verbraucher.** Der
ESP8266 zieht im Deep Sleep ~20 µA — der USB-Serial-Wandler (CH340), die
Power-LED und der LDO auf dem Board ziehen in der Praxis zusammen häufig
**150–300 µA**, also ein Vielfaches davon. Solange auf dem Steckbrett noch
nichts fest ist, ist das der richtige Moment, über ein nacktes ESP-12F-Modul mit
sparsamem LDO nachzudenken oder wenigstens die Power-LED auszulöten. Auch das
gehört in die Ruhestrommessung aus Abschnitt 2 — sie misst ohnehin das ganze
Board, nicht nur den HX711.

### Was der Funkpegel schon jetzt sagt

Auf dem Steckbrett liegt der Pegel zwischen −69 und −78 dBm, mit einem Ausreißer
auf −85 dBm. Das ist bereits im Grenzbereich (Richtwert der Projektnotizen:
besser als −70 dBm ist gut, unter −80 dBm wird es wackelig) — und der Standort
im Garten wird eher schlechter, nicht besser. Für Deep Sleep zählt das doppelt:
jede Sekunde, die der Verbindungsaufbau länger dauert, geht direkt in die
Tagesbilanz. Eine externe Antenne oder ein Repeater sollte mit eingeplant
werden, ebenso `fast_connect: true` und eine feste `manual_ip`.

## 7. `reboot_timeout: 0s` nimmt dem Gerät die Selbstheilung

Unabhängig von Deep Sleep, aber am Steckbrett aufgefallen: Am 02.08. war das
Gerät von 17:46 bis 22:04 über vier Stunden nicht erreichbar, **ohne neu zu
starten** — die Betriebszeit lief mit exakt +18.000 s durch. Es hing also in
einem WLAN-Zustand, aus dem es nicht von selbst herausfand.

Genau das ist die Folge von `api: reboot_timeout: 0s`. Die Projektnotizen
begründen die Einstellung mit dem langen Messintervall, aber `reboot_timeout`
zählt die Zeit **ohne verbundenen API-Client**, nicht die Zeit zwischen zwei
Messungen. Solange Home Assistant verbunden ist, löst der Timer ohnehin nie aus
— die 0 schaltet nur den Wachhund ab, der einen hängenden WLAN-Stack wieder
geradezieht.

Für den Netzbetrieb ist ein endlicher Wert (z. B. `15min`) die bessere Wahl. Im
Deep-Sleep-Betrieb muss er dagegen bei `0s` bleiben, sonst startet das Gerät im
Weckfenster neu, bevor es überhaupt schlafen kann.

## 8. Offene Punkte

1. **Ruhestrom messen**, bevor überhaupt ein Akku geplant wird — siehe
   Abschnitt 2. Alles Weitere hängt an dieser einen Zahl.
2. **Einschwingzeit des HX711 nach Power-Down** ausmessen: bestimmt, wie kurz
   `run_duration` werden darf, und damit die Batterielaufzeit.
3. **Kriechen und Temperaturgang nach jedem Aufwachen.** Wird die Brücke
   stromlos, kühlt sie zwischen den Messungen aus. Ob das die Wiederholbarkeit
   verschlechtert, ist offen — und hängt direkt mit der laufenden
   Drift-Auswertung zusammen.
4. **Ist Deep Sleep überhaupt der richtige Weg?** Bei 4,3 mA allein für die
   Brücke ist ein kleines Solarpanel mit LiFePO4-Zelle womöglich die einfachere
   Antwort als das Jagen nach Mikroampere.

---

## 9. Umsetzung auf dem ESP32 (`stockwaage`, 17.08.2026)

Eingebaut in `stockwaage.yaml`, **nicht** in `packages/waage-basis.yaml` — die
Basis wird mit `waage-eg` geteilt, ein `deep_sleep:` dort würde beim nächsten
Flash das produktive ESP8266-Gerät schlafen legen. Nachgewiesen: `deep_sleep`
taucht in der aufgelösten Konfiguration von `waage-eg`, `waage-stock2` und
`waage-stock3` **null**mal auf, in `stockwaage` genau einmal.

### 9.1 Was der ESP32 gegenüber Abschnitt 1–5 ändert

| Aus diesem Dokument | Auf dem ESP32 |
|---|---|
| Brücke GPIO16 ↔ RST zwingend (Abschnitt 1) | **entfällt ersatzlos** — der RTC-Timer weckt chipintern |
| Aufwachen **nur** über den Timer (Abschnitt 5) | **GPIO-Wecken möglich** (ext0/ext1) — davon lebt der Hardwareschalter |
| Schlafdauer max. ~71 min (32-Bit-µs) | **kein Limit in der Praxis** (64 Bit) |
| Pull-up 10 kΩ von SCK nach 3V3 (Abschnitt 2) | **weglassen** — siehe 9.3 |
| Momenttaster im Schlaf verloren | gilt weiter, aber lösbar (siehe 9.6) |

Weckfähige Pins des klassischen ESP32, aus
`components/deep_sleep/__init__.py` (2026.7.4): **0, 2, 4, 12, 13, 14, 15, 25,
26, 27, 32–39**. GPIO16/17 (HX711) und GPIO18 (LED) sind **nicht** dabei.

### 9.2 Der BS250 als Lastschalter

P-Kanal-MOSFET als **High-Side**-Schalter: Source an 3V3, Drain an VCC des
HX711-Moduls, Gate an **GPIO25**. Gate LOW = leitend, deshalb `inverted: true`
am Schalter.

**Zwingend: 100 kΩ vom Gate nach 3V3** (Gate→Source). Im Deep Sleep sind die
GPIOs hochohmig; ohne diesen Widerstand schwebt das Gate und der MOSFET steht
undefiniert, im schlimmsten Fall halb leitend. Der Pull-up ist der
Ausschalt-Pfad im Schlaf — es gibt keine Aktion, die das erledigt, und es
braucht auch keine. 220 Ω in Reihe ins Gate begrenzen den Umladestrom.

> **Vorbehalt, vor dem Batteriebetrieb zu messen:** Der BS250 ist **kein
> Logic-Level-Typ.** Sein Rds(on) ist bei **Vgs = −10 V** spezifiziert, die
> Schwellspannung liegt je nach Exemplar zwischen etwa −1 V und −3,5 V. Am
> 3,3-V-Rail stehen aber nur −3,3 V zur Verfügung — ein Exemplar am oberen Ende
> der Streuung leitet damit kaum. Bei den ~6 mA Laststrom fällt das lange nicht
> auf, kann aber die Versorgungsspannung des HX711 drücken und damit die
> Messung verstimmen.
>
> **Gegenprobe mit dem Multimeter:** Spannung zwischen Source und Drain im
> eingeschalteten Zustand messen. Mehr als ~50 mV heißt, der MOSFET leitet
> schlecht. Dann entweder ein Logic-Level-Typ (IRLML6402, DMG3415, AO3401) oder
> das Gate über eine kleine NPN-Stufe aus 5 V ansteuern.

### 9.3 Warum der SCK-Pull-up jetzt schädlich wäre

Abschnitt 2 empfiehlt einen Pull-up von SCK nach 3V3, damit der HX711 im Schlaf
selbst in den Power-Down geht. Mit dem MOSFET ist das nicht nur überflüssig,
sondern **falsch**: Der Widerstand hinge an *dauerhaften* 3V3 und würde über die
Schutzdiode am SCK-Eingang Strom in den abgeschalteten HX711 speisen — genau die
Versorgung, die der MOSFET gerade abwirft. Wer beide Wege kombiniert, muss den
Pull-up an die **geschaltete** Seite legen.

### 9.4 Die beiden Schalter

**Hardware — rastender Schalter an GPIO33 gegen GND.** GPIO33 ist weckfähig
*und* hat einen internen Pull-up (anders als GPIO34–39, wo einer extern nötig
wäre). Umgesetzt über `wakeup_pin_mode: KEEP_AWAKE`, also nicht selbst gebaut.
Der Mechanismus steht in `deep_sleep_esp32.cpp`:

```cpp
if (wakeup_pin_mode_ == WAKEUP_PIN_MODE_KEEP_AWAKE &&
    wakeup_pin_ != nullptr && wakeup_pin_->digital_read()) {
  ESP_LOGW(TAG, "Waiting for wakeup pin state change");
  return false;   // -> es wird nicht geschlafen
}
```

Geprüft wird bei **jedem** Schlafversuch, nicht nur beim Booten — Umlegen wirkt
in beide Richtungen. Und weil derselbe Pin Weckpin ist, holt ein Umlegen
*während* des Schlafs das Gerät sofort hoch.

Ein **rastender** Schalter ist hier zwingend, kein Taster: Ein Momentkontakt
wäre beim nächsten Schlafversuch längst wieder offen.

**Software — `input_boolean.stockwaage_wachhalten`,** gespiegelt über einen
`binary_sensor: platform: homeassistant`, ausgewertet in `on_boot` mit
`deep_sleep.prevent`.

> Bewusst **kein** Template-Switch auf dem Gerät, obwohl der seinen Zustand über
> NVS behalten würde: Ein Umlegen **während des Schlafs** ginge verloren, weil
> das Gerät nicht erreichbar ist — und genau das ist der Hauptfall („ich will
> gleich ein OTA fahren"). Der `homeassistant`-Sensor holt sich beim Verbinden
> den *aktuellen* Stand des Helfers; Umlegen im Schlaf wirkt damit beim nächsten
> Aufwachen.

**Der Softwareschalter ist zugleich der einzige Weg zu OTA.** Ohne ihn ist das
Gerät nach dem ersten Flash mit aktivem Deep Sleep praktisch nur noch über USB
erreichbar (Abschnitt 5).

### 9.5 Was am Messkonzept angepasst wurde

**Der HX711 wird im Weckfenster schneller abgetastet: 100 ms statt 1 s.** Die
Filterkette (Median 5 → gleitendes Mittel 12) braucht bei 1 s rund **60 s**, bis
hinten der erste Wert herauskommt — länger als das ganze Weckfenster. Bei 100 ms
ist sie nach gut **6 s** voll, bei identischer Glättung. 10 Werte/s ist zugleich
die Wandlungsrate des HX711 bei Gain 128.

**Veröffentlicht wird explizit in `on_boot`, nicht über den 60-s-Taktgeber der
Basis** — der feuert erst nach dem Ende des Weckfensters. Reihenfolge wie dort:
erst `messfenster_abschliessen` (setzt die `mittel_*`-Globals), dann
`messwerte_veroeffentlichen`.

**Das Messfenster schrumpft, und damit die Genauigkeit.** Der Gewinn aus dem
Sessionbericht 11.08. (bei 6 h Intervall ~4.300 Werte statt 12, simulierte
Reststreuung 2,2 g → 0,1 g) ist im Deep-Sleep-Betrieb nicht zu haben: In den
~12 s nach dem Einschwingen kommen rund 25 Werte zusammen. Das ist gegenüber
der Auflösung von 0,1 kg unkritisch, sollte aber niemanden überraschen, der die
beiden Geräte vergleicht.

**`deep_sleep.enter` wird bewusst nicht aufgerufen.** Das Einschlafen erledigt
`run_duration`. Grund: `begin_sleep(bool manual)` überspringt die
`prevent_`-Prüfung, wenn `manual` gesetzt ist — ein explizites `enter` könnte
also am Wachhalten vorbeischlafen. Über `run_duration` greifen `prevent` und
`KEEP_AWAKE` beide zuverlässig.

**Nebenwirkung, die gefällt:** Solange wachgehalten wird, übernimmt nach der
ersten Veröffentlichung wieder der normale Taktgeber der Basis — das Gerät
verhält sich dann exakt wie im Netzbetrieb, samt Durchsichtmodus und
Kalibrier-Sperre.

**Weiter offen:** `web_server` ist entfernt (RAM und Weckzeit), `fast_connect`
gesetzt. Eine feste `manual_ip` würde zusätzlich DHCP sparen, ist aber bewusst
nicht geraten — eine falsche Netzmaske heißt im Deep-Sleep-Betrieb USB-Kabel.
Die `uptime`-Entity verliert ihren Sinn, sie beginnt bei jedem Aufwachen neu.

### 9.6 Offen für den ESP32

1. **Ruhestrom messen** — unverändert Punkt 8.1. Erst diese Zahl sagt, ob aus
   Variante C (~17 Tage) wirklich Variante D (~200 Tage) wird.
2. **Spannungsabfall über den BS250 messen** — siehe 9.2. Entscheidet, ob der
   Typ taugt.
3. **Einschwingzeit des HX711 nach Power-Down ausmessen.** `einschwingzeit` steht
   auf 12 s, geschätzt mit Reserve. Jede eingesparte Sekunde geht direkt in die
   Laufzeit.
4. **Durchsicht-Taster.** GPIO34 ist weckfähig, ein Druck *könnte* das Gerät
   also wecken — anders als beim ESP8266. Nicht eingebaut: ext0 ist bereits vom
   Wachhalten-Schalter belegt, für zwei Weckquellen bräuchte es `ext1` mit
   beiden Pins, und GPIO34 braucht dafür seinen externen Pull-up. Bis dahin gilt
   die Empfehlung aus Abschnitt 5: rastender Schalter statt Taster.

