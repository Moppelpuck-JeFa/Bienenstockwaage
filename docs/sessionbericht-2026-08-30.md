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

## 6. Offene Punkte

- **Wachhalten wieder ausschalten**, sonst bleibt das Gerät wach. Ebenso den
  Kippschalter, falls er noch anliegt.
- **Aktuelle Fassung flashen** — Weckgrund und Bootnummer, danach die beiden
  Entities auf `bienenwaage_*` umbenennen und erst dann aufs Dashboard.
- **`binary_sensor.bienenwaage_wachhalten_schalter` suchen** und ggf. umbenennen.
- **FRITZ!Box aufräumen:** feste Lease für `20:50:0D:D2:53:64`, und die
  Einträge der Boards 1 und 2 löschen. Zwei Hosts gleichen Namens sind die
  Ursache der `.171`-Verwechslung.
- **`device_tracker.bienenwaage`** (Board 2) ist die zweite Karteileiche in
  HA — wie Board 1 deaktivieren.
- **Authentifizierungsfehler beim ersten Verbindungsversuch** nachgehen.
- **Langzeitstatistik bereinigen.** Die Werte von 0,5 kg und die 22,2 kg des
  Prüfgewichts stehen dauerhaft drin (`state_class: measurement`).
  Entwicklerwerkzeuge → Statistiken.
- **Funkstrecke** — unverändert der wichtigste Hebel.
