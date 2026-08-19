# Sessionbericht 19.08.2026 — Wachhalten wirkte nicht

Gerät: `stockwaage` (ESP32 DOIT DevKit V1), Deep Sleep, 60 min Schlaf.
Vorgeschichte: [Sessionbericht 17.08.](sessionbericht-2026-08-17.md),
[deep-sleep-vorbereitung.md](deep-sleep-vorbereitung.md) Abschnitt 9.

## 1. Beobachtung

`input_boolean.stockwaage_wachhalten` stand seit **08:15:07** auf *an*.
Das Gerät bootete um **08:50:32** und schlief trotzdem wieder ein.

Aus der HA-Historie, ohne Gerätezugriff rekonstruiert:

| Zeitpunkt | Entity | Wert |
|---|---|---|
| 08:50:35 | `sensor.stockwaage_betriebszeit` | `unknown` (Entity verfügbar → API verbunden) |
| 08:52:05 | `sensor.stockwaage_betriebszeit` | 93,47 s → Boot um 08:50:32,4 |
| 08:53:52 | `binary_sensor.stockwaage_verbindung` | **off** |

08:53:52 minus 08:50:32,4 = **200,2 s**, und `wachzeit` stand am Gerät auf
`200s`. Das Einschlafen kam also punktgenau von `run_duration` —
`deep_sleep.prevent` war nicht aktiv.

## 2. Ausschluss: falsche Firmware

Der naheliegende Verdacht war der alte: Add-on-Kopie und Repo
auseinandergelaufen, `on_state` gar nicht geflasht. Widerlegt über die
WS-API des Add-ons (`/devices`, Port 6052, Ingress gibt 403):

```
expected_config_hash: 5066dafe
deployed_config_hash: 5066dafe
has_pending_changes:  false
```

Und `devices/get_config` zeigte `on_state:`, `wachhalten_ha` sowie
`entity_id: input_boolean.stockwaage_wachhalten` in der geflashten Datei.
Die Logik war also an Bord und lief trotzdem nicht.

Nebenbefund: Am Gerät stand `wachzeit: "200s"`, im Repo `120s` — von Hand
geändert und nie zurückgeflossen. Jetzt im Repo nachgezogen.

## 3. Ursache

Sie steht nicht im YAML, sondern im Codegenerator. ESPHome 2026.7.4,
`components/binary_sensor/__init__.py`:

```python
trigger = config.get(CONF_TRIGGER_ON_INITIAL_STATE, False) or config.get(
    CONF_PUBLISH_INITIAL_STATE, False
)
cg.add(var.set_trigger_on_initial_state(trigger))
```

Der Aufruf erfolgt **immer**. Die Vorgabe im C++ (`binary_sensor.h`:
`bool trigger_on_initial_state_{true}`) wird damit jedes Mal
überschrieben — fehlt der Schlüssel im YAML, mit `false`.

Ausgewertet wird das in `core/entity_base.h`:

```cpp
if (new_state.has_value() && (had_state || this->get_trigger_on_initial_state()))
  this->state_callbacks_.call(new_state.value());
```

Der erste Zustand aus Home Assistant kommt über
`HomeassistantBinarySensor::setup()` als `publish_initial_state()`, und
das ruft zuerst `invalidate_state()`. `had_state` ist beim Rückruf also
**false**. Mit `trigger_on_initial_state_ == false` fällt die Bedingung
komplett durch: **der allererste Zustand nach dem Booten löst kein
`on_state` aus.**

Daraus folgt die Regel, die diesen Fehler erklärt und wiederholbar macht:

> **`on_state` an einem `homeassistant`-Sensor feuert nur ab dem
> *zweiten* Wert.** An einem dauerhaft laufenden Gerät fällt das nie auf,
> weil jedes spätere Umlegen `had_state == true` hat. An einem
> Deep-Sleep-Gerät ist der erste Wert der **einzige**, auf den es ankommt.

Das erklärt auch die scheinbar sprunghafte Beobachtung vom 18.08.: einmal
blieb es wach (Helfer wurde *während* des Weckfensters umgelegt → zweiter
Wert), beim nächsten Boot schlief es ein (nur erster Wert).

## 4. Behebung

In `stockwaage.yaml`, am `wachhalten_ha`-Sensor:

```yaml
trigger_on_initial_state: true
```

Dazu `on_boot` gehärtet — es wartete auf `api.connected`, was zu früh
ist, weil der Zustand erst ein Stück nach dem Verbindungsaufbau eintrifft:

```yaml
- wait_until:
    condition:
      lambda: 'return id(wachhalten_ha).has_state();'
    timeout: 20s
```

Beide Wege bewusst nebeneinander: `on_state` fängt das Umlegen während
des Weckfensters, `on_boot` bleibt auch dann richtig, wenn eine künftige
ESPHome-Version an der Rückruf-Logik wieder etwas dreht.

Geprüft mit `esphome config stockwaage.yaml` (2026.7.4), Exit 0.
`waage-eg.yaml` und `packages/waage-basis.yaml` unangetastet — der Fehler
und der Fix betreffen nur die Deep-Sleep-Datei.

## 5. Dashboard

Der Softwareschalter hat jetzt eine Kachel: Ansicht *Übersicht*,
Abschnitt *Tiefschlaf* (Toggle, Hardwareschalter-Status, *Gerät wach*),
dazu ein Badge in der Kopfzeile und dieselbe Kachel unter *Technik →
Diagnose*.

## 6. Am Gerät bestätigt

Geflasht um **18:54:32**, `expected_config_hash` = `deployed_config_hash`
= `a856b3c1`, keine pending changes. Im geflashten Inhalt gegengelesen:
`trigger_on_initial_state: true` und `has_state()` sind drin.

Kontrolle nach dem Flash, beide Punkte bestanden: Kalibrierfaktor
**−20.780,66** (unverändert, im Erwartungsbereich), „Kalibriert bei"
**22,5 °C** (nicht leer). Zweiter Beleg dafür, dass NVS auf dem ESP32
einen Flash übersteht.

**Der Nachweis musste den Hardwareschalter loswerden.** Solange der auf
*an* stand, hielt `KEEP_AWAKE` das Gerät wach und ein wachliegendes Gerät
bewies über den Softwareschalter nichts. Entscheidend ist die Reihenfolge
in `begin_sleep()`:

```cpp
if (this->prevent_ && !manual) { this->next_enter_deep_sleep_ = true; return; }
if (!this->prepare_to_sleep_()) { return; }   // hier fragt KEEP_AWAKE den Pin
```

`run_duration` war längst abgelaufen, `loop()` versuchte also bei jedem
Durchlauf zu schlafen und scheiterte am Pin. Fällt der Pin weg,
entscheidet allein `prevent_`. Damit wurde aus einer Wartezeit von einer
Stunde ein Test von fünf Sekunden.

| | |
|---|---|
| Boot | 18:55:24 |
| `wachzeit` (`run_duration`) | 200 s → fällig 18:58:44 |
| Hardwareschalter **aus** | 19:19:25 |
| Softwareschalter (HA) | **an**, seit 13:04 |
| Gerät antwortet auf 192.168.1.171 | **19:23:14** |

28 Minuten Wachzeit bei 200 s `run_duration`, davon knapp 4 Minuten ohne
Hardwareschalter. Vorher schlief das Gerät auf die Sekunde genau nach
200,2 s ein. Der Unterschied ist die eine Zeile.

## 7. Schlafdauer an das Messintervall gekoppelt

`sleep_duration` war eine feste substitution, die Number „Messintervall"
nur eine Beschriftung. Jetzt setzt das Skript `schlafdauer_koppeln` sie zur
Laufzeit — beim Booten als erste Aktion in `on_boot` und bei jedem
Verstellen über ein per `!extend` angehängtes `on_value`.

Gerechnet wird mit der **Periode**: `Messintervall = Schlafdauer + Wachzeit`.
Bei 72 h fällt die Wachzeit mit 0,08 % nicht ins Gewicht, bei 30 min wären
es 11 %.

**Am Gerät belegt** über den Boot-Konfigurationsdump:

```
[C][deep_sleep:033]:   Sleep Duration: 1600000 ms
[C][deep_sleep:036]:   Run Duration: 200000 ms
```

1.600.000 ms = 30 min × 60 − 200 s. Der YAML-Rückfallwert wären 3.600.000 ms
gewesen. Das ist der schnelle Nachweis — auf das tatsächliche Aufwachen nach
72 h müsste man drei Tage warten.

Die Obergrenze der Number bleibt bei 10.080 min (7 Tage); 72 h sind 4.320 und
lagen ohnehin darin.

## 8. `on_boot`: erst einschwingen, dann auf HA warten

Die Reihenfolge war falsch herum. Gemessen: WLAN- und API-Anlauf brauchen
**37,3 s**, der `wait_until` davor gab nach 20 s auf. Die Wachhalten-Prüfung
in `on_boot` lief also *jedes Mal* ins Leere — dass Wachhalten trotzdem
wirkt, verdankt sich allein `on_state`. Nebenwirkung war ein Senden bei
110 s statt 90 s.

Jetzt laufen Einschwingen und Verbindungsaufbau parallel.

## 9. Jeder Neustart startete einen Durchsicht-Nachlauf

Der teuerste Befund des Tages, und er hat mich drei Fehldiagnosen gekostet.

**Symptom:** pro Weckfenster zwei Sendevorgänge — einer bei rund 60 s, einer
bei 90 s. 60 s liegen vor dem Ende der gemessenen Einschwingzeit.

**Drei falsche Erklärungen**, alle plausibel, alle widerlegt:

1. *Anlauf-Zweig des Taktgebers.* `erste_messung_erfolgt` ist
   `restore_value: no`, nach jedem Aufwachen also false. Behoben mit
   `erste_messung_erfolgt = true` in `on_boot` — half nicht.
2. *Zu spät gesetzt.* Der erste Tick eines `interval:` kommt nach einem
   Zufallsversatz von 0…interval/2 (`core/scheduler.cpp`), und
   `PollingComponent::call_setup()` startet den Poller vor `setup()`. Also
   nach `on_boot 700` verschoben, zwischen Globals (`HARDWARE` = 800) und
   `IntervalTrigger` (`DATA` = 600) — half auch nicht. Der Mechanismus ist
   echt, war aber nicht die Ursache.
3. *Durchsicht-Nachlauf.* Als Kandidat verworfen, weil
   `switch.stockwaage_durchsichtmodus` nachweislich durchgehend auf *aus*
   stand.

Punkt 3 war der richtige, und der Ausschluss war der Fehler. Nachgewiesen
mit einer Diagnosezeile, die jede Aufrufstelle sich selbst nennen ließ:

```
[I][waage:1438]: AUSLOESER: Durchsicht-Nachlauf beendet
[I][waage:1271]: Messfenster: 0 Rohwerte -> -601776.8 counts (Streuung nan)
[I][waage:1325]: veroeffentlichen: t=60.8s min_seit=0 soll=30 erste=1
```

**Ursache:** der Durchsichtmodus-Schalter hat `restore_mode: ALWAYS_OFF`.
ESPHome ruft dafür beim Booten `turn_off()` auf, und das führt
`turn_off_action` aus — also `durchsicht_beenden`, das `nachlauf_minuten = 2`
setzt. Der Schalter selbst bleibt dabei auf *aus*, deshalb ist von außen
nichts zu sehen.

> **`restore_mode: ALWAYS_OFF` ist kein „lass es aus", sondern ein
> Schaltbefehl beim Start.** Wer im `turn_off_action` mehr tut als einen Pin
> zu setzen, führt das bei jedem Boot aus.

**Folgen im Deep-Sleep-Betrieb**, wo jedes Aufwachen ein Neustart ist:

- ein zusätzlicher Sendevorgang bei ~60 s, und zwar ein **Momentanwert** —
  der Nachlauf schickt bewusst kein Fenstermittel
- während des Nachlaufs sammelt das Messfenster nicht. Der eigentliche
  Messwert bei 90 s stand deshalb auf **39 statt 119 Rohwerten** — das halbe
  Mittelungsfenster fehlte

Am dauerhaft laufenden Gerät kostete das nach einem Neustart zwei Minuten,
und Neustarts sind selten. Deshalb ist es seit dem 03.08. unbemerkt geblieben.

**Behoben:** `durchsicht_beenden` räumt nur noch auf, wenn wirklich eine
Durchsicht lief. Gefragt wird der Schalterzustand, nicht
`durchsicht_restminuten` — beim Ablauf der Zeit hat der Taktgeber den Zähler
bereits auf 0 gesetzt, bevor er das Skript ruft.

**Gegenprobe am Gerät**, Boot 20:47:05:

| | vorher | nachher |
|---|---|---|
| Sendevorgänge pro Weckfenster | 2 (60,8 / 90,3 s) | **1** (90,32 s) |
| Rohwerte im Messfenster | 39 | **119** |
| Rohwert Streuung | `nan` | **12,83** counts (≈ 0,6 g) |

## 10. Offene Punkte
- **BS250 ist abgeklemmt.** Der Lastschalter wirkt derzeit nicht, die
  ~5,8 mA Grundlast bleiben. Ersatztyp diskutiert (TP2104, V_GS(th) max
  −2,0 V — der BS250 ist mit max −3,5 V bei 3,3-V-Ansteuerung gar nicht
  zugelassen); Alternative ohne Halbleiter: 10 kΩ Pull-up auf SCK, dann
  geht der HX711 im Schlaf über PD_SCK selbst in den Power-Down.
  Entscheidung steht aus.
- **`schlafdauer` weiter bei 60 min.** Bei 200 s Wachzeit sind das gut
  5,5 % Einschaltdauer — damit trägt der HX711-Power-Down kaum. Das ist
  eine imkerliche Entscheidung über die Messdichte, deshalb nicht
  eigenmächtig geändert.
- **WLAN bei −75 dBm.** Weiter im Auge behalten — beim Test standen
  −67 dBm an, also besser als gestern.
- **Gegenprobe steht noch aus:** Softwareschalter auf *aus*, beide
  Schalter aus, dann muss das Gerät binnen einer Sekunde einschlafen.
  Erst damit ist auch der `else`-Zweig (`deep_sleep.allow`) belegt.
- **Messintervall steht auf 30 min.** Für den eigentlichen Zweck sind
  **4320** (72 h) einzustellen. Das Skript rechnet dann 259.000 s Schlaf;
  im Boot-Dump nachlesbar, ohne drei Tage zu warten.
- **Log mitlesen ist mühsam.** Der ESPHome-Log-Stream liefert über die
  Add-on-API nur Ausschnitte von 10 s (`wait_for_close: false`) bzw. rund
  30 s (`wait_for_close: true`, endet per `size_limit`). Einen Vorgang bei
  60 s Betriebszeit zu treffen kostete vier Anläufe. Wer das öfter braucht,
  sollte den Befund in eine Entity schreiben statt ins Log.
