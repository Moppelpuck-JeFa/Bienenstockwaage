# Sessionbericht 17.08.2026

Portierung der produktiven Konfiguration auf **ESP32 NodeMCU** (DOIT DevKit V1)
als eigenständiges Testgerät. Reine Board-Portierung des Netzteilbetriebs —
kein Deep Sleep, kein Solar-/Batteriebezug.

**Anlagenzustand unverändert:** `waage-eg` läuft weiter auf dem ESP8266,
geflasht wurde nichts. `waage-eg.yaml` und `packages/waage-basis.yaml` sind
**nicht angefasst** worden; die einzige neue Datei ist `waage-esp32-test.yaml`.

**Nicht Teil dieser Session:** Hardware-Aufbau, Flashen, Kalibrieren des
ESP32-Geräts. Die HA-Seite (Helfer, Automationen, Dashboard) hängt weiter
ausschließlich an `waage-eg` und sieht das Testgerät nicht.

---

## 1. Was entstanden ist

Eine Datei, 243 Zeilen, davon 212 Kommentar und Leerzeilen:

```
waage-esp32-test.yaml    substitutions + package-Include + vier Overrides
```

Die gesamte Messlogik kommt **unverändert aus `packages/waage-basis.yaml`** —
derselben Datei, aus der auch `waage-eg` geflasht wird.

**Bewusst keine ESP32-Kopie der Basis.** Die Aufgabenstellung ließ das offen
(„sonst eigene ESP32-Variante der Basis, falls esp8266-spezifische Direktiven
inkompatibel sind"). Nötig war es nicht: die drei Inkompatibilitäten lassen
sich aus der Stock-Datei heraus auflösen, ohne die Basis anzufassen. Eine
zweite Kopie von 1.542 Zeilen Logik wäre der teurere Weg gewesen — sie läuft
beim nächsten Feinschliff auseinander, und zwar still. Genau die Sorte Fehler,
gegen die am 11.08. die Zusammenführung der Veröffentlichungsliste in
`messwerte_veroeffentlichen` gebaut wurde.

Möglich ist das durch zwei ESPHome-Tags, die im Repo bisher nicht vorkamen:

| Tag | Wirkung | hier für |
|---|---|---|
| `!remove` | löscht einen aus einem Package geerbten Schlüssel | `esp8266:` |
| `!extend <id>` | greift einen Listeneintrag des Packages über seine ID heraus und merged hinein | Durchsicht-Taster |

**`!extend` ist dabei nicht optional, sondern zwingend.** Listen aus Packages
werden aneinandergehängt, nicht über die ID zusammengeführt (`merge_config` in
`esphome/config_helpers.py`: `return old + new`). Ein einfaches erneutes
Deklarieren des `binary_sensor` mit derselben ID ergäbe deshalb **zwei**
Einträge mit derselben ID und damit einen Fehler, keinen Override.

---

## 2. Die vier Abweichungen vom ESP8266

### 2.1 Plattformblock

`esp8266: !remove`, dann ein `esp32:`-Block mit `board: ${board}` und
ausgeschriebenem `framework: type: esp-idf`. Die Vorgabe von ESPHome 2026.6.5
für ESP32 ist ohnehin esp-idf (`_set_default_framework` in
`components/esp32/__init__.py`) — ausgeschrieben, damit ein späterer Wechsel
der Vorgabe dieses Gerät nicht unbemerkt auf eine andere Toolchain schiebt.

### 2.2 `restore_from_flash` — es gibt keine Entsprechung, und es braucht keine

Das war der Punkt, der ausdrücklich zu klären und nicht 1:1 zu kopieren war.
Nachgesehen im Quelltext von ESPHome 2026.6.5:

| | ESP8266 | ESP32 |
|---|---|---|
| Signatur | `make_preference(length, type, in_flash)` | `make_preference(length, type)` |
| Platzvergabe | **Reihenfolge** der Aufrufe (`current_flash_offset += total_words`) | eigener NVS-Eintrag je Global |
| Schlüssel | Namens-Hash geht **nur in die CRC** ein | Namens-Hash **ist** der NVS-Schlüssel |
| Ablage | RTC-RAM, nur mit `restore_from_flash: true` im Flash | NVS, also immer Flash |

Der Schlüssel ist in beiden Fällen `1944399030 ^ name_hash`
(`components/globals/globals_component.h`) — auf dem ESP8266 dient er nur der
Prüfsumme, auf dem ESP32 ist er der Name, unter dem
`nvs_set_blob`/`nvs_get_blob` den Wert ablegen.

**Damit fällt der Dauerbrenner dieses Projekts auf dem ESP32 strukturell weg:**
„ein Flash mit einem neuen Global kostet die Kalibrierung" (03.08. und
10.08.2026 real passiert). Ein neues Global belegt einen neuen NVS-Schlüssel
und lässt die bestehenden in Ruhe — unabhängig von seiner Position in der Datei
und unabhängig davon, ob es `restore_value: yes` oder `no` trägt. Die auf dem
ESP8266 hart erarbeitete Regel „neue Globals möglichst mit `restore_value: no`"
ist hier gegenstandslos.

Zwei Einschränkungen, die trotzdem gelten:

- **Die Prüfregel „nach jedem Flash den Kalibrierfaktor ansehen" bleibt.** Sie
  kostet dreißig Sekunden und fängt auch alles, was mit den Preferences nichts
  zu tun hat: halbe Kalibrierung, verrutschte Zelle, gelöschtes NVS.
- **Neu zu beachten:** NVS wird von `erase_flash` bzw. „Clean Build Files"
  mitgelöscht. Auf dem ESP8266 lag der Preferences-Bereich außerhalb dessen,
  was ein OTA anfasst. Vor einem Erase also den Kalibrierfaktor notieren.

### 2.3 WLAN-Stromsparmodus

**Nicht geplant, sondern im Diff aufgefallen.** `power_save_mode` hat eine
plattformabhängige Vorgabe: `NONE` auf dem ESP8266, `LIGHT` auf dem ESP32. Die
Portierung hätte also nebenbei den Stromsparmodus eingeschaltet, ohne dass das
irgendwo im YAML steht.

Deshalb in der Stock-Datei auf `NONE` festgehalten. Sachlich ist das hier auch
die richtige Einstellung: die Waage steht am Rand der Funkabdeckung (−74 bis
−76 dBm an `waage-eg`), und Light Sleep lässt den Empfänger zwischen den
Beacons schlafen. Gespart würde Strom, den ein Gerät am Netzteil nicht sparen
muss.

**Das ist das Ergebnis, für das der Diff-Vergleich existiert.** Im YAML ist von
dieser Änderung nichts zu sehen — sie steht nur in der aufgelösten
Konfiguration.

### 2.4 Durchsicht-Taster auf GPIO34

Der einzige Punkt des Anschlussplans, der mehr kostet als eine geänderte
Pin-Nummer. GPIO34 ist **input-only und hat keine internen Pull-Widerstände**.
Die Basis konfiguriert den Taster mit `pullup: true`; auf GPIO34 bricht damit
schon `esphome config` ab.

Aufgelöst über `!extend durchsicht_taster` mit `pullup: false` — ein Feld, der
Rest (Pin-Nummer, `inverted`, die 50-ms-Entprellung) bleibt aus der Basis.
Hardwareseitig gehört ein **externer 10-kΩ-Pull-up gegen 3V3** dazu, Taster
weiter gegen GND. Damit ist die Logik identisch zum internen Pull-up des
ESP8266 (Ruhe = high, gedrückt = low) und `inverted: true` stimmt weiterhin.

---

## 3. Pinbelegung

| Funktion | ESP8266 (`waage-eg`) | ESP32 (hier) | Externe Beschaltung |
|---|---|---|---|
| HX711 DOUT | D6 (GPIO12) | GPIO16 | — |
| HX711 CLK | D1 (GPIO5) | GPIO17 | — |
| DS18B20 | D5 (GPIO14) | GPIO4 | Pull-up 4,7 kΩ gegen 3V3 |
| Durchsicht-Taster | D2 (GPIO4) | GPIO34 | **Pull-up 10 kΩ gegen 3V3**, Taster gegen GND |
| Durchsicht-LED | D7 (GPIO13) | GPIO18 | Vorwiderstand 1 kΩ |
| Versorgung | 5V / 3V3 | VIN (5 V) / 3V3 | 3V3-Regler der DevKits trägt HX711 samt Brücke (~5,8 mA) locker |

Gemieden und warum: GPIO0/2/5/12/15 (Boot-Strapping — GPIO12 setzt sogar die
Flash-Spannung, ausgerechnet der Pin, auf dem beim ESP8266 DOUT lag),
GPIO6–11 (SPI-Flash), GPIO1/3 (UART0), GPIO35/36/39 (ebenfalls input-only, als
Reserve freigehalten).

**GPIO16 ist auf dem ESP32 unkritisch.** Die ganze Argumentation um den
ESP8266-Weckpin GPIO16 (`docs/deep-sleep-vorbereitung.md`) ist hier
gegenstandslos: der ESP32 weckt über RTC-GPIOs, und GPIO16 gehört nicht dazu.

Ein Vorbehalt für später: GPIO16/17 sind auf ESP32-**WROOM** frei, auf
**WROVER**-Modulen liegt dort das PSRAM. Ein Modulwechsel zwänge DOUT/CLK zum
Umzug.

---

## 4. Nachweis

Geprüft mit ESPHome **2026.6.5**, nach der Methode aus dem
[Sessionbericht 04.08.](sessionbericht-2026-08-04.md) — verglichen wird die
*aufgelöste* Konfiguration, nicht das YAML:

```bash
esphome config waage-eg.yaml         | sed 's/\x1b\[[0-9;]*m//g' > eg.txt
esphome config waage-esp32-test.yaml | sed 's/\x1b\[[0-9;]*m//g' > esp32.txt
diff eg.txt esp32.txt
```

**Ergebnis: 133 abweichende Zeilen von 1.112 bzw. 1.163, restlos in fünf
Gruppen einsortierbar** — Identität, Plattformblock, Plattformvorgaben,
Pin-Nummern, Taster-Pull-up. Keine einzige davon in der Messlogik.

Gezielt gegengeprüft:

| Prüfung | Ergebnis |
|---|---|
| Entity-Namen (`name:`-Felder) | **identisch**, einzige Abweichung ist der Gerätename `waage-eg` → `waage-esp32-test` |
| `globals:`-Block, komplett | **identisch** — 21 Globals, gleiche IDs, `restore_value` und `initial_value` |
| `script:` + `interval:`, komplett | **identisch** — 260 Zeilen |
| `tests/waage-temperatur-test.cpp` | 4092 Prüfungen, 0 Fehler |
| `esphome config` aller vier Stock-Dateien | gültig |

Die 21 identischen Globals sind der eigentliche Beleg für die Vorgabe
„Kalibrierfaktor/Globals unverändert übernehmen": Rechenweg und Startwerte sind
Zeile für Zeile dieselben, der Erwartungsbereich von **−18.000 bis −21.000
counts/kg** gilt hier also unverändert weiter. Er hängt an Wägezellen, Brücke
und HX711 — der Mikrocontroller geht in die Größe nicht ein.

Die verbleibenden Unterschiede außerhalb von Pins und Identität sind sämtlich
Plattformvorgaben, die ESPHome selbst setzt, und alle unkritisch:

```
ota port          8266 -> 3232
api               listen_backlog 1->4, max_connections 4->5, max_send_queue 4->8
logger            esp8266_store_log_strings_in_flash entfällt, task_log_buffer_size 768 neu
wifi              output_power entfällt, enable_btm/enable_rrm neu
gpio-Schema       analog: entfällt; drive_strength, ignore_strapping_warning neu
```

---

## 5. Was am Testgerät noch zu tun ist

Bewusst **nicht** vorweggenommen:

- **Kalibrieren, beide Schritte.** Die `start_calib_*`-Werte sind wie bei jedem
  neuen Gerät Platzhalter und ergeben rechnerisch 3.500 counts/kg — das
  vereinbarte Alarmzeichen für „noch nie kalibriert". Echte Werte dort
  einzutragen wäre ein Fehler: sie sähen nach einer gültigen Kalibrierung aus,
  ohne je gegen ein Gewicht geprüft worden zu sein.
- **Messintervall** bleibt bei 6 h, damit die Portierung sich vom Verhalten her
  nicht von `waage-eg` unterscheidet. Zum Testen in HA herunterstellen — es ist
  eine Number-Entity und kostet keinen Flash.
- **Temperaturkoeffizient auf 32,5 g/K** statt der sonst für neue Geräte
  vorgeschriebenen 0. Begründung: derselbe Aufbau, dieselben Zellen; der
  Koeffizient ist eine Eigenschaft der Mechanik, nicht des Controllers. Wird
  die Portierung an einem **zweiten, eigenen** Aufbau getestet, gehört dort
  `"0"` hin.

---

## Offene Punkte

**Aus dieser Session neu:**

- **Nicht kompiliert.** `esphome config` validiert vollständig, ersetzt aber
  keinen Compilerlauf; die PlatformIO-Registry ist durch die Egress-Policy
  gesperrt (bekannte Einschränkung, CLAUDE.md). Der erste `esphome compile`
  für die esp-idf-Toolchain steht also noch aus. Bekannte Stolperstellen sind
  vorab ausgeräumt: `web_server` läuft auf ESP32 über den IDF-eigenen Server
  statt über ESPAsyncWebServer (`components/web_server_base/__init__.py`,
  früher Rücksprung bei `CORE.is_esp32`), und die drei Header aus `packages/`
  binden nur `<cmath>`/`<cstdint>` ein, also nichts Arduino-Spezifisches.
  `hx711`, `one_wire`/`dallas_temp` und `api: reboot_timeout` haben keinerlei
  Plattformeinschränkung im Schema.
- **Hardware nicht aufgebaut**, insbesondere der externe 10-kΩ-Pull-up an
  GPIO34. Ohne ihn schwebt der Tastereingang und der Durchsichtmodus schaltet
  sich zufällig ein.
- **Erste NVS-Erfahrung sammeln.** Die Aussage „ein neues Global kostet auf dem
  ESP32 die Kalibrierung nicht" ist aus dem Quelltext abgeleitet, nicht am
  Gerät belegt — derselbe Stand, in dem die ESP8266-Regel am 11.08. war, bevor
  sie am 12.08. erstmals bestätigt wurde. Beim ersten Flash mit einem neuen
  Global also gezielt hinsehen.
- **Entscheiden, wozu das Testgerät führt.** Wenn der ESP32 `waage-eg` einmal
  ablösen soll, ist der teure Teil nicht die Firmware, sondern die
  Entity-Migration: `geraete_name`/`anzeige_name` bilden die entity_id, und
  Verlauf, Helfer, Automationen und Dashboard hängen daran. Das ist ein eigener
  Schritt und braucht einen Plan, keinen Flash.

**Aus früheren Sessions weiterhin offen:** unverändert, siehe
[`waage-eg-claude-code-kontext.md`](../waage-eg-claude-code-kontext.md),
Abschnitt 7.

---

## Werkzeugnotizen

- **`pip install esphome` schlägt hier ohne venv fehl.** Die
  Debian-gepatchte `setuptools` aus `/usr/lib/python3/dist-packages` bricht
  beim Bauen von `crcmod` und `paho-mqtt` mit
  `AttributeError: install_layout` ab. Abhilfe: `python3 -m venv`, darin
  `pip install --upgrade pip setuptools wheel`, dann `pip install esphome`.
  Ergebnis war wie gehabt 2026.6.5.
- **`!remove` und `!extend` sind die Werkzeuge, um ein Package zu variieren,
  ohne es zu kopieren.** `!remove` löscht einen geerbten Schlüssel (auch einen
  ganzen Komponentenblock wie `esp8266:`), `!extend <id>` greift einen
  Listeneintrag über seine ID heraus. Beides ist im Repo bisher nicht
  vorgekommen und beim nächsten Sonderfall die erste Frage.
- **Listen aus Packages werden konkateniert, nicht über die ID gemerged.**
  Nachgesehen in `esphome/config_helpers.py` (`merge_config`: bei zwei Listen
  `return old + new`). Wer ohne `!extend` überschreiben will, erzeugt einen
  Duplikat-ID-Fehler — der Hinweis in der Fehlermeldung führt nicht zu
  `!extend`, deshalb hier notiert.
- **Der Diff der aufgelösten Konfiguration hat sich zum zweiten Mal bezahlt
  gemacht.** Am 04.08. belegte er, dass sich *nichts* geändert hat; diesmal
  fand er eine Änderung, die niemand geschrieben hatte
  (`power_save_mode: LIGHT`). Bei einem Plattformwechsel ist er kein Nachweis
  am Ende, sondern ein Suchwerkzeug: **plattformabhängige Vorgaben sind im YAML
  unsichtbar.**
