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

## 6. Offene Punkte

- **Noch nicht am Gerät bestätigt.** Der Fix braucht einen Flash. Prüfung
  danach: Helfer auf *an*, eine Schlafphase abwarten, `Verbindung` muss
  über 200 s hinaus *an* bleiben.
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
- **WLAN bei −75 dBm.** Weiter im Auge behalten.
