# Sessionbericht 30.08.2026 — dritter Boardtausch, Kalibrierung wiederhergestellt

Fortsetzung von [`sessionbericht-2026-08-28.md`](sessionbericht-2026-08-28.md),
der die Sitzungen vom 28. und 29.08.2026 abdeckt.

## 1. Das Board wurde ein drittes Mal getauscht

Bestätigt am 30.08.2026. Der Bootlog nennt eine MAC, die es in Home Assistant
noch nie gab:

| | MAC | Zustand |
|---|---|---|
| Board 1 (ESP8266-Nachfolger, bis 28.08.) | `F8:B3:B7:49:59:7C` | .171, abgebaut |
| Board 2 (28.–29.08.) | `20:50:0D:CA:B2:BC` | seit 29.08. 20:53 `not_home` |
| **Board 3 (seit 29.08. abends)** | **`20:50:0D:D2:53:64`** | .115, aktiv |

Die FRITZ!Box hat für Board 2 sogar den Hostnamen verloren und führt es als
`PC-20-50-0D-CA-B2-BC`. **Damit gibt es jetzt zwei Karteileichen im Router**,
beide unter dem Hostnamen `stockwaage` bzw. dessen Rest — genau die
Mehrdeutigkeit, die am 29.08. die `.171`-Zugriffe des Device Builders
verursacht hat.

`minimum_chip_revision: "3.1"` hat gehalten: der Bootlog meldet
`chip revision: v3.1`. Beim nächsten Tausch ist das die erste Stelle zum
Nachsehen — auf einem älteren ESP32 startet das Bild nicht.

## 2. Warum die Kalibrierung zweimal misslang

Ein neues Board heißt leeres NVS, also Platzhalter. Der Verlauf am Vormittag:

| Zeit | Kalibrierfaktor | |
|---|---|---|
| 09:45 | 3.500 | Platzhalter — nie kalibriert |
| 10:10 | **+35.623** | positiv |
| 10:13 | **−1.087.848** | |

Beide Zwischenwerte sind unmöglich, und zwar aus demselben Grund.

**Die Kalibrierprozedur passt nicht in ein Weckfenster.** Sie braucht mit den
Wartezeiten gut drei Minuten; `run_duration` sind 200 Sekunden. Zwischen
„Kalibrieren 0 kg" und „Kalibrieren Referenzgewicht" lag jedes Mal ein
Neustart — der Nullpunkt stammte damit aus einem anderen Zustand als der
Referenzpunkt.

Von außen sah das aus wie Abstürze: alle drei bis vier Minuten war das Gerät
weg. Die Betriebszeit belegt das Gegenteil — sie lief jedes Mal bis ~90 s
(der Sendezeitpunkt), dann kam nach 200 s der reguläre Tiefschlaf.

> **Regel daraus: vor jeder Kalibrierung das Wachhalten einschalten.** Steht
> jetzt auch im Kommentar bei `bienenwaage.yaml`.

Der Wert **−1.087.848** hat zusätzlich eine eigene Signatur: das
Referenzgewicht stand auf dem Platzhalter **0,5 kg**, während der Stock
auflag. −560.000 counts ÷ 0,5 kg ≈ −1,12 Mio. Wer den Referenz-Button drückt,
ohne die Masse vorher einzutragen, bekommt genau das.

## 3. Die Kalibrierung, die gehalten hat

Mit *Wachhalten* an, Referenzgewicht auf 22,221 kg gesetzt, beide Schritte in
Ruhe gefahren:

| Prüfung | Wert | |
|---|---|---|
| **Gewicht bei aufliegendem Prüfgewicht** | **22,20 kg** bei 22,221 kg | **21 g Abweichung** |
| Kalibrierfaktor | −24.356,30 | negativ, plausible Größenordnung |
| Kalibriert bei | 21,125 °C | nicht leer |
| Kalibriert mit | 22,221 kg | |
| Temperaturkorrektur | 0,002 kg | ≈ 0, weil Ist- ≈ Kalibriertemperatur |
| Rohwert | −557.002 counts | |

Gegengerechnet: Nullpunkt ≈ −16.300 counts, Span 540.703 counts über
22,221 kg = 24.333 counts/kg. In sich konsistent.

### Der Erwartungsbereich im Repo war falsch

−24.356 liegt außerhalb der dokumentierten **−18.000 bis −21.000**. Der
Bereich stammt vom ESP8266-Aufbau, und die Begründung dafür — der Faktor
hänge nur an Brücke und HX711, nicht am Mikrocontroller — trägt nicht:

| | Faktor | gegengeprüft? |
|---|---|---|
| ESP8266 | −20.756 | ja |
| ESP32-Board 2 | −14.081 | **nein** |
| ESP32-Board 3 | −24.356 | **ja** |

Woran der Unterschied hängt, ist offen. Der HX711 misst ratiometrisch, die
Versorgungsspannung sollte sich also herauskürzen.

**Was daraus folgt, ist wichtiger als die Erklärung:** Der Faktor taugt nur
noch als Grobfilter — negativ, Größenordnung 10.000 bis 30.000. Entschieden
wird mit dem **bekannten Gewicht**. In `CLAUDE.md`, `README.md` und
`bienenwaage.yaml` entsprechend korrigiert.

Nebenbei ist damit auch der seit dem 28.08. offene Punkt erledigt, den
Faktor −14.081 gegenzuprüfen: er war zu klein, und er ist Geschichte.

## 4. Was der Bootlog sonst zeigte

**`fast_connect: false` war die richtige Entscheidung**, und der Scan zeigt
warum:

```
- 'Faul-W-Lan' (E0:28:6D:D4:FE:59) Ch: 1 -66dB   <- gewaehlt
- 'Faul-W-Lan' (E8:DF:70:57:E4:05) Ch: 1 -71dB
- 'Faul-W-Lan' (E0:28:6D:68:9D:D7) Ch: 1 -78dB
```

**Drei Accesspoints mit derselben SSID.** Mit `fast_connect: true` hätte sich
das Gerät blind auf den zuletzt benutzten festgenagelt — möglicherweise den
mit −78 dB. Genau der Mechanismus aus Punkt 15 des Vorberichts, hier zum
ersten Mal belegt statt hergeleitet.

Bemerkenswert dabei: die Verbindung meldet danach **−76 dB** für dieselbe
BSSID, die der Scan mit −66 dB gefunden hat. Die Scanwerte sind also
optimistisch.

**Neuer Befund — Authentifizierung schlägt beim ersten Versuch fehl:**

```
Disconnected ssid='Faul-W-Lan' bssid=E0:28:6D:D4:FE:59 reason='Authentication Failed'
Retry attempt 2/2 ... Connected
```

Das kostet rund 1,6 s pro Weckvorgang und ist **kein Reichweitenproblem**,
sondern eines am Accesspoint. Ein eigenständiger Kandidat für die verpassten
Weckfenster. Zu prüfen in der FRITZ!Box: WPA-Modus, Band-Steering,
MAC-Filter.

**Weiteres:** `use_address` wirkt (`OTA Address: 192.168.1.115:3232`), und das
Add-on läuft jetzt auf **ESPHome 2026.8.1** statt 2026.6.5 — die
Versionsangabe in `CLAUDE.md` ist entsprechend zu lesen.

## 5. Was nicht geflasht ist

Der Bootlog sagt `compiled on 2026-08-29 21:04:08`. Weckgrund und Bootnummer
(Punkt 24 des Vorberichts) kamen erst um 22:15 ins Repo und sind auf dem
Gerät **nicht** drauf — im `dump_config` fehlen beide. Die Frage „weckt der
Timer?" ist damit weiterhin offen.

Außerdem fehlt `binary_sensor.bienenwaage_wachhalten_schalter` in Home
Assistant, obwohl die Firmware ihn laut Bootlog hat
(`GPIO Binary Sensor 'Wachhalten (Schalter)', Pin: GPIO33`). Vermutlich die
Namensfalle aus Punkt 24: der Registry-Eintrag war gelöscht, ein neu
angelegter bekommt die ID aus `geraete_name`, also `stockwaage_*`. Noch nicht
nachgesehen.

## 6. Der Timer weckt — belegt

Am Vormittag zum Testen `Messintervall` auf **20 min** gestellt. Die Änderung
greift sofort: `bienenwaage.yaml` hängt an der Number ein
`on_value: script.execute: schlafdauer_koppeln`. Schlafdauer damit
1.200 − 200 = **1.000 s**.

### Der erste Versuch war verunreinigt

| Zeit | |
|---|---|
| 11:15:25 | eingeschlafen |
| **11:23:01** | Boot — **456 s** später |

456 s passt zu keiner gesetzten Schlafdauer, weder zu 1.000 s noch zum
vorherigen Wert von 3.400 s. Zu dieser Zeit wurde am Gerät hantiert
(Prüfgewicht abgeräumt); der Kippschalter an GPIO33 löst ext0 aus. Der
Weckvorgang beweist damit nichts über den Timer.

### Der zweite Versuch, ohne Eingriff

| | |
|---|---|
| Eingeschlafen | 11:26:16 |
| Wachphase davor | 11:23:01 → 11:26:16 = **195 s** (`run_duration` 200 s ✓) |
| **Boot** | **11:42:23** |
| Erwartet | 11:42:56 |

**Gemessene Schlafdauer: 967 s bei 1.000 s gesetzt** — 3,3 % zu kurz.

Das ist keine Fehlfunktion, sondern der interne RC-Oszillator, aus dem der
ESP32 im Tiefschlaf seine Zeit nimmt; ein paar Prozent Abweichung sind dort
normal. Für ein Messintervall ohne Sekundenanspruch ist das ohne Belang —
wer es genau braucht, müsste den RTC gegen einen Quarz kalibrieren.

**Damit ist die Frage aus Punkt 24 des Vorberichts beantwortet: der Timer
weckt.** Zusammen mit dem ext0-Nachweis vom 29.08. (Punkt 24, Abschnitt „Der
Schalter weckt") funktionieren **beide** Weckpfade.

Was daraus für die verpassten Weckfenster der Vortage folgt: Der Tiefschlaf
war nie die Ursache. Übrig bleiben die Funkstrecke und der
Authentifizierungsfehler beim ersten Verbindungsversuch (Punkt 4).

### Nebenbefund: die Waage ist leer, und der Nullpunkt sitzt 0,77 kg daneben

| Zeit | Rohwert | Gewicht |
|---|---|---|
| 11:15 | −569.561 | 22,7 kg |
| 11:24 | +2.508 | −0,7 kg |
| 11:43 | +2.506 | −0,8 kg |

Die Last ist komplett herunter — rund 572.000 counts Sprung. Die
angekündigten 7 kg liegen (noch) nicht auf, eine Linearitätsprüfung steht
also weiterhin aus.

Erklärungsbedürftig ist der Rest: **die leere Waage zeigt −0,8 kg statt 0.**
Der Nullpunkt aus der Kalibrierung lag bei ≈ −16.300 counts, leer misst sie
+2.506 — 0,77 kg Differenz. Entweder lag beim Schritt „Kalibrieren 0 kg" noch
etwas auf, das jetzt weg ist (dann ist alles konsistent), oder der Nullpunkt
ist um diesen Betrag daneben und ein Tara ist fällig. Offen.

> Zu beachten: −0,8 kg liegt noch im Plausibilitätsfenster (−1 bis 150 kg).
> Driftet es weiter, wird das Gewicht verworfen und nur noch Rohwert,
> Streuung und Temperatur gehen raus.

## 7. Die Onboard-LED zeigt die WLAN-Verbindung

Auf Wunsch dazugekommen. Die blaue LED des DevKit V1 sitzt fest an **GPIO2**;
eine Alternative gibt es für sie nicht.

| LED | Bedeutung |
|---|---|
| blitzt | Gerät ist wach **und** im WLAN |
| aus | kein WLAN, oder das Gerät schläft |

Im Tiefschlaf ist sie zwangsläufig dunkel: nur GPIO33 bekommt ein
`gpio_hold_en()`, alle anderen Pads fallen auf Eingang zurück. Bei 200 s
Wachzeit und 20 min Intervall blitzt sie rund 17 % der Zeit — genau das ist
die Aussage.

### Kurzer Blitz statt Blinken

100 ms alle 2 s sind **5 % Einschaltdauer statt 50 %**. Die LED zieht über
ihren Vorwiderstand ein paar Milliampere; für den geplanten Akkubetrieb ist
der Unterschied nicht mehr egal. Erkennbar ist ein Blitz genauso gut — besser
sogar, weil er sich vom Dauerlicht der Durchsicht-LED an GPIO18 klar
unterscheidet.

Die LED ist `internal: true`, also **keine HA-Entity**. Sie ist eine Anzeige
am Gerät, kein Bedienelement — und das spart eine neue Entity, die nach dem
Flash wieder `stockwaage_*` hieße und von Hand umzubenennen wäre (Punkt 5).

### GPIO2 stand auf der Meideliste, und der Grund gilt weiter

GPIO2 ist ein **Boot-Strapping-Pin**: Der ESP32 geht in den Download-Modus,
wenn beim Reset GPIO0 LOW ist, und GPIO2 muss dabei LOW oder offen sein.

Hier ist das vertretbar. Die Onboard-LED liegt über ihren Vorwiderstand gegen
GND, zieht den Pin also nicht hoch, und beim Reset ist er ohnehin noch
Eingang — die Firmware macht erst im `setup()` einen Ausgang daraus. Der
USB-Flash bleibt möglich.

> **Aber nicht folgenlos:** Manche DevKit-Nachbauten haben an GPIO2 einen
> Pull-up. Auf so einem Board kann der USB-Flash nach dieser Änderung
> fehlschlagen. **Sollte das je passieren, ist dieser Pin der erste
> Verdächtige** — steht so auch im Kommentar an der Substitution.

Geprüft: `esphome config` gültig, und der Diff der aufgelösten Konfiguration
enthält **null Entfernungen** — kein bestehender Eintrag hat sich bewegt.

## 8. Die Entities waren da — unter einem dritten Namensschema

Nach dem Flash fehlten `Weckgrund`, `Bootnummer` **und**
`Wachhalten (Schalter)` in Home Assistant. Zwei Vermutungen, beide falsch:

1. „Nicht geflasht." Widerlegt — die Datei enthält `weckgrund`, und der
   Nutzer hatte geflasht.
2. „HA hat die Entity-Liste zwischengespeichert, weil sich die MAC geändert
   hat." Widerlegt durch einen Reload des Config-Entry
   (`homeassistant.reload_config_entry`): die Verbindung kam neu, die Liste
   wurde frisch geholt — die Entities blieben verschwunden.

### Nachgesehen statt geraten

Der Diagnose-Dump der ESPHome-Integration
(`ha_get_system_health(include="diagnostics", …)`, Pfad
`data.storage_data`) zeigt genau, was HA vom Gerät bekommt:

```
device_info.compilation_time: "2026-08-30 14:58:04 +0200"
sensor:      … {"object_id": "bootnummer", "name": "Bootnummer", …}
text_sensor: … {"object_id": "weckgrund",  "name": "Weckgrund",  …}
binary_sensor: … {"object_id": "wachhalten__schalter_", …}
```

**Alle drei waren die ganze Zeit da.** Nur nicht unter den IDs, die ich
gesucht hatte.

Aufgelöst über die unique_id — deren Format ist
`{MAC}/0/{domain}/{Name}`, also z. B.
`20:50:0D:D2:53:64/0/sensor/Bootnummer`:

| tatsächliche entity_id | |
|---|---|
| `sensor.garten_bienenwaage_bootnummer` | 8 |
| `sensor.garten_bienenwaage_weckgrund` | „Neustart" |
| `binary_sensor.garten_bienenwaage_wachhalten_schalter` | `on` |

### Das Präfix ist der Bereich

`garten_`. Home Assistant leitet die entity_id neuer Entities aus
**Bereich + Gerät + Entity** ab, wenn das Gerät einem Bereich zugeordnet ist
— hier „Garten".

Meine Vorhersage in Punkt 24 des Vorberichts lautete `stockwaage_*`, weil
neue Entities die ID aus `geraete_name` bekämen. Das war für die
Umbenennungen vom 28.08. richtig und für **neue** Entities falsch: Es gibt
inzwischen ein drittes Schema, und es hängt am Bereich, nicht am Gerätenamen.

Alle drei sind auf `bienenwaage_*` umbenannt. Die Kachel *Schalter am Gerät*
im Dashboard (Punkt 20 des Vorberichts) zeigt damit wieder auf eine
existierende Entity, und *Weckgrund* und *Bootnummer* stehen jetzt unter
*Technik → Diagnose*.

### Erste Werte

- **Weckgrund: „Neustart"** — erwartungsgemäß, der Boot kam vom Flash bzw.
  vom Reload, nicht aus dem Tiefschlaf.
- **Bootnummer: 8** — der Zähler läuft und liegt in NVS.
- **Wachhalten (Schalter): `on`** — der Kippschalter am Gehäuse ist an. Das
  erklärt, warum das Gerät gerade durchgehend wach ist.

Ab dem nächsten regulären Zyklus steht dort „Timer" bzw. „Schalter
(GPIO33)", und die Bootnummer beantwortet, ob zwischen zwei Meldungen
Wachphasen ohne Verbindung lagen.

### Die Lehre

**Eine Vorhersage über ein Namensschema ist keine Prüfung.** Zweimal in Folge
habe ich hier eine ID geraten, statt sie über die unique_id aufzulösen —
einmal mit `stockwaage_*`, einmal mit `bienenwaage_*`. Der Weg, der beide
Male sofort funktioniert hätte:

```
ha_get_entity(entity_id=<eine bekannte Entity desselben Geräts>)   -> unique_id-Format lernen
ha_get_entity(unique_id="{MAC}/0/{domain}/{Name}")                 -> die gesuchte ID
```

## 9. Langzeitstatistik bereinigt

Ausgangslage: Das Messintervall steht wieder auf 60 min, und in der
Langzeitstatistik standen die Werte des heutigen Vormittags — 0,5 kg aus der
misslungenen Kalibrierung, −0,8 kg bei leerer Waage, 22,2/22,7/22,9 kg vom
Prüfgewicht. Weil `waage_gewicht` `state_class: measurement` trägt, bleiben
diese Stundenwerte **dauerhaft** stehen; der Zustandsverlauf fällt nach ~10
Tagen weg, die Statistik nicht.

### Der Trick: Home Assistant kann keinen Zeitraum löschen — aber neu schreiben

`recorder/clear_statistics` löscht immer eine **ganze** `statistic_id`,
`recorder.purge_entities` ebenso. Einen Bereich herauszuschneiden ist über die
Oberfläche nicht vorgesehen. Der Ausweg ist zweistufig:

1. alle Stundenzeilen auslesen und sichern,
2. `recorder/clear_statistics`,
3. `recorder/import_statistics` mit **nur den sauberen** Zeilen.

`import_statistics` akzeptiert für eine `statistic_id` mit gültiger
`entity_id` die Quelle `recorder` — es ist also nicht auf externe Statistiken
beschränkt. Die zurückgeschriebenen Zahlen sind die von HA selbst berechneten,
nichts wird erfunden; einzig `mean` wurde auf vier Nachkommastellen gerundet
(0,1 mg).

> **Vorher prüfen, ob der Rückweg funktioniert.** Erst eine bereits
> vorhandene Zeile mit ihren eigenen Werten überschreiben (ein No-op), dann
> löschen. Schlägt der Import fehl und ist vorher gelöscht, ist die Reihe weg.

### Was entfernt wurde

Verworfen wurden Stunden, deren `min` unter 26 kg oder deren `max` über 40 kg
lag — das Band, in dem sich das Stockgewicht seit dem 12.08. bewegt.

`sensor.bienenwaage_gewicht` (Board 2, 18.–29.08.), 273 → 263 Zeilen:

| Stunde (UTC) | mean | min | max | Ursache |
|---|---|---|---|---|
| 18.08. 09:00 | 28,49 | 0 | 34,9 | Tara |
| 18.08. 16:00 | 25,70 | −0,6 | 38,3 | Tara |
| 18.08. 17:00 | 34,44 | −1,0 | 38,4 | Tara |
| 24.08. 16:00 | 32,86 | −0,9 | 35,4 | Tara |
| 24.08. 17:00 | 34,26 | −0,9 | 36,2 | Tara |
| 30.08. 08:00 | 10,74 | 0,5 | 22,2 | Boardtausch, Kalibrierung |
| 30.08. 09:00 | 8,51 | −0,8 | 22,7 | Kalibrierung, Prüfgewicht |
| 30.08. 10:00 | −0,80 | −0,8 | −0,8 | leere Waage |
| 30.08. 11:00 | 18,62 | −0,8 | 22,9 | Prüfgewicht |
| 30.08. 12:00 | 22,90 | 22,9 | 22,9 | Prüfgewicht |

`sensor.waage_eg_gewicht` (Board 1/ESP8266, 11.–18.08.), 156 → 145 Zeilen:
der ganze 11.08. ab 19:00 UTC bis 12.08. 02:00 UTC — die flach auf 20,5 kg
stehende Strecke der kaputten Kalibrierung — sowie 12.08. 17:00/18:00 und
13.08. 18:00 (Tara, min 16,1 bzw. −1,0).

Die Tagesmittel liegen jetzt durchgehend zwischen 30,7 und 36,2 kg; vorher zog
der 30.08. das Tagesmittel auf 11,99 kg und der 11.08. auf 21,15 kg.

### Sieben verwaiste statistic_ids gelöscht

Aus den beiden früheren Umbenennungen waren `statistic_id`s stehen geblieben,
zu denen **gar keine Entity mehr existiert** — sie tauchen nur noch unter
Entwicklerwerkzeuge → Statistiken als Problem auf:

`sensor.waage_waage_eg_gewicht`, `sensor.waage_waage_eg_betriebszeit`,
`sensor.waage_eg_waage_eg_rohwert`, `sensor.waage_eg_waage_eg_temperatur`,
`sensor.waage_eg_waage_tagesbilanz`,
`sensor.waage_eg_waage_gewichtsverlust_kurz`,
`sensor.waage_eg_waage_gewichtsanderung`.

Ihr Inhalt war ohnehin unbrauchbar: die Tagesbilanz reichte von −2.265 bis
+3.784 kg pro Tag, die Gewichtsänderung von −140,8 bis +72,1 kg/d.

### Was stehen geblieben ist, und warum

- **Rohwert, Streuung, Temperatur** — nicht angefasst. Die Temperatur ist
  unabhängig davon richtig, was auf der Waage liegt, und der Rohwert ist laut
  harter Regel die Grundlage jeder Nachmessung des Temperaturkoeffizienten.
  Nach einem Boardtausch ist er ohnehin nicht mit dem alten vergleichbar —
  falsch ist er deshalb nicht.
- **`sensor.bienenwaage_tagesbilanz`, `_gewichtsanderung`,
  `_gewichtsverlust_kurz`** — hier ist nicht ein Zeitraum verdorben, sondern
  die ganze Reihe. Auch an ruhigen Tagen stehen dort ±30 bis ±60 kg/d und
  ±9 kg/h. Ursache sind die Lücken durch den Tiefschlaf: die Ableitung sieht
  einen Sprung, wo nur eine Messpause war. Das ist ein Entwurfsproblem der
  Helfer, kein Ausreißer — deshalb hier nicht gelöscht, sondern als offener
  Punkt notiert.
- **`sensor.futtervorrat`** — ebenfalls eine verwaiste `statistic_id` (kg,
  ohne Entity). Nicht angefasst, weil nicht zweifelsfrei zu diesem Projekt
  gehörig.

### Die Bereinigung hält nur, wenn nichts Falsches mehr gesendet wird

Zum Zeitpunkt der Bereinigung lag das Prüfgewicht noch auf der Waage
(22,9 kg), das Gerät sendete weiter. **Jede weitere volle Stunde schreibt eine
neue, ebenso falsche Zeile.** Daraus folgt eine Regel, die die bestehende
Kalibriersperre ergänzt:

> **Die Kalibriersperre deckt Minuten ab, nicht Stunden.** `kalibrier_sperre`
> steht auf 10 Minuten und schützt gegen das Vergessen unmittelbar nach Tara
> und Kalibrierung. Ein Prüfgewicht, das zum Linearitätstest stundenlang
> liegen bleibt, überlebt sie mühelos. Dafür ist der **Durchsichtmodus**
> zuständig (`switch.bienenwaage_durchsichtmodus`): er hält alles mit
> `state_class` zurück, die Diagnose läuft weiter. Wer länger als
> `kalibrier_sperre` mit Prüfgewichten arbeitet, schaltet ihn vorher ein —
> sonst ist die Statistik hinterher wieder zu reparieren.

Der Schalter wurde **nicht** von sich aus gesetzt: der angekündigte
Linearitätstest (+7 kg) braucht die Anzeige.

## 10. Die toten Entitäten von `waage-eg` entfernt

Der ESP8266 ist seit dem 28.08. außer Betrieb, seine 24 Entitäten standen aber
noch in der Registry und waren durchgehend `unavailable`. Entfernt wurden sie
nicht einzeln, sondern über den **Config-Entry** des Geräts — damit gehen
Gerät und Entitäten in einem Zug, und es bleibt kein Geräteeintrag ohne
Entitäten zurück.

| | tot | lebend |
|---|---|---|
| Gerät | Waage eG | Bienenwaage |
| Modell | `d1_mini` | `esp32doit-devkit-v1` |
| MAC | `8c:ce:4e:c9:e5:7f` | `20:50:0d:d2:53:64` |
| Config-Entry | `01KYQMWSGPFDSSMVD6M20ME9NM` („Waage") | `01M08HH8349N52K2MR1RABCV3T` („Bienenwaage") |

**Beide ESPHome-Einträge standen auf `loaded`** — auch der des seit zwei Tagen
abgeschalteten Boards. Der Zustand taugt also nicht zur Unterscheidung; ein
ESPHome-Eintrag bleibt „geladen", solange er die Verbindung erneut versucht.
Zugeordnet wurde deshalb über Modell und MAC, nicht über den Titel: der tote
Eintrag heißt „Waage", der lebende „Bienenwaage" — Namen, bei denen man sich
leicht vergreift.

Vor dem Löschen geprüft und jeweils ohne Treffer: Automationen, Skripte,
Szenen, Helfer (`ha_search`, `config_total_matches: 0`) und sämtliche
Storage-Dashboards (`ha_config_get_dashboard(mode="search")`).

Die Suche nach `waage_eg` findet 23 Entitäten, das Gerät hatte aber **24**:
`update.waage_firmware` trägt das Präfix nicht. Über den Config-Entry zu gehen
hat sie mit erwischt — beim Einzellöschen wäre sie stehen geblieben.

> **Entitäten löschen und Statistik löschen sind zwei verschiedene Dinge.**
> Die Langzeitstatistik der sieben Reihen mit `state_class` hat das Entfernen
> überlebt und lag anschließend als verwaiste `statistic_id` vor.

Diese sieben sind anschließend in zwei Schritten gelöscht worden — erst die
reinen Diagnosereihen (WLAN-Signal, Betriebszeit, Rohwert-Streuung), dann auf
Ansage auch der Rest: Gewicht, Rohwert, Temperatur und Temperatur-Mittel.
**Von `waage-eg` ist damit in Home Assistant nichts mehr übrig.**

Aufgehoben hätte man die Gewichtskurve vom 12.–18.08. (Tagesmittel 34,3 bis
35,5 kg) und das Paar Rohwert/Temperatur, aus dem sich der
Temperaturkoeffizient von Board 1 nachträglich bestimmen ließe. Beides ist
verworfen worden, weil der Aufbau noch Test ist und nicht Wirkbetrieb: Board 1
hatte den nie gegengeprüften Faktor −20.756, seine Kurve ist mit der von
Board 3 ohnehin nicht vergleichbar, und ein Temperaturkoeffizient von Board 1
hilft an Board 3 nicht weiter.

> Für den Wirkbetrieb gilt das nicht mehr. Ab dann ist eine Gewichtsreihe
> Messreihe, kein Testartefakt — dann vor dem Löschen erst sichern
> (die Zeilen auslesen, wie in Punkt 9 beschrieben).

Zuletzt fiel noch `sensor.futtervorrat` (kg) auf — ebenfalls eine verwaiste
`statistic_id` ohne Entity, deren Zugehörigkeit sich von außen nicht klären
ließ. Sie gehört zum Projekt und ist mit gelöscht. Auch inhaltlich war sie
hinüber: Monatsmittel 16,67 kg bei einem **Minimum von −19 kg** — ein
negativer Futtervorrat ist keiner, das ist der Nachhall der kaputten
Kalibrierungen.

Damit ist von diesem Projekt keine verwaiste `statistic_id` mehr übrig; in der
Statistik stehen nur noch die neun Reihen des aktiven Geräts
(`sensor.bienenwaage_*`).

## 11. Offene Punkte

- **Wachhalten wieder ausschalten**, sonst bleibt das Gerät wach. Ebenso den
  Kippschalter, falls er noch anliegt.
- ~~Aktuelle Fassung flashen.~~ **Erledigt** am 30.08.2026 14:58 (Punkt 8).
  Entities umbenannt, Kacheln gesetzt.
- ~~Nullpunkt klären.~~ **Erledigt** am 30.08.2026.
- **Linearität prüfen** — die angekündigten 7 kg zusätzlich zum Prüfgewicht.
  Erwartung: 29,22 kg, Rohwert-Differenz ≈ 170.500 counts.
- **Messintervall von 20 min zurückstellen** — bei 20 min ist das Gerät 17 %
  der Zeit wach.
- **`binary_sensor.bienenwaage_wachhalten_schalter` suchen** und ggf. umbenennen.
- ~~FRITZ!Box aufräumen.~~ **Erledigt** am 30.08.2026. Board 3 führt der Router
  jetzt als `stockwaage`; in HA ist der Tracker
  `device_tracker.stockwaage` (MAC `20:50:0D:D2:53:64`, `home`).
- ~~`device_tracker.bienenwaage` (Board 2) deaktivieren.~~ **Erledigt** am
  30.08.2026: Gerät deaktiviert und in „Stockwaage ALT (Board 2, bis
  29.08.2026)" umbenannt, wie zuvor Board 1. Das aktive Gerät heißt jetzt
  „Bienenwaage (FRITZ!Box, Board 3)".

  > Zu beachten: Die Entity-IDs sind dabei irreführend geblieben — die
  > **Leiche** heißt `device_tracker.bienenwaage`, das **aktive** Board
  > `device_tracker.stockwaage`. Umbenennen wäre möglich, kostet aber die
  > Historie des aktiven Trackers; die Gerätenamen sind eindeutig genug.
- **Authentifizierungsfehler beim ersten Verbindungsversuch** nachgehen.
- ~~Langzeitstatistik bereinigen.~~ **Erledigt** am 30.08.2026 (Punkt 9).
  Die Gewichtsreihe ist in beiden Hälften sauber, sieben verwaiste
  `statistic_id`s sind weg.

  > Nachzuholen, sobald das Prüfgewicht herunter ist: die Stunden ab
  > 30.08. 13:00 UTC, die seither weiterlaufen. Beim nächsten Mal vorher den
  > **Durchsichtmodus** einschalten, dann entfällt das.
- **Tagesbilanz, Gewichtsänderung und Gewichtsverlust überarbeiten.** Ihre
  Langzeitstatistik ist über den gesamten Zeitraum unbrauchbar (±30 bis
  ±60 kg/d an ruhigen Tagen), weil die Ableitungen die Tiefschlaf-Lücken für
  Sprünge halten. Erst den Helfer reparieren, dann die Statistik löschen —
  andersherum ist sie sofort wieder voll.
- **Funkstrecke** — unverändert der wichtigste Hebel.
