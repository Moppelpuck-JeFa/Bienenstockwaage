# Sessionbericht 28.08.2026 — Umbenennung Stockwaage → Bienenwaage

**Auftrag:** das Dashboard „Stockwaage" auf das neue Gerät „Bienenwaage"
umbauen, Anlass ist der Wechsel des ESP32-Boards.

**Ergebnis:** erledigt, aber anders als geplant. Der Auftrag ging von einem
*zweiten* Gerät aus, auf das umzuhängen wäre. Das gibt es nicht — das neue
Board hat die Identität des alten übernommen. Aus dem Umhängen wurde deshalb
eine Umbenennung: 30 Entities, das Gerät und 47 Dashboard-Referenzen. Die
Messreihe ist dabei erhalten geblieben.

---

## 1. Der Irrweg, und warum er einer war

Die naheliegende Lesart war: neues Board → neues Gerät in HA → neue Entities
`bienenwaage_*` → Dashboard umhängen. Dafür sprach handfeste Evidenz. In HA
stand ein offener ESPHome-Discovery-Flow:

```
handler  esphome (zeroconf)
key      bienenwaage._esphomelib._tcp.local.
name     "Bienenwaage (bienenwaage)"
MAC      20:50:0d:ca:b2:bc
step     discovery_confirm
```

Ein Gerät namens „Bienenwaage" meldete sich also tatsächlich im Netz an. Der
Schluss daraus — es werde gleich als eigenes Gerät entstehen — war trotzdem
falsch.

**Der Beweis kam erst beim Adoptionsversuch über die IP:**

```
Flow aborted: already_configured_updates
  title  Stockwaage
  name   stockwaage
  mac    20:50:0d:ca:b2:bc
```

HA hat das Board dem **bestehenden** Config-Entry zugeordnet, statt ein neues
anzulegen. Danach stand im Geräteregister:

| | vorher | nachher |
|---|---|---|
| MAC | `f8:b3:b7:49:59:7c` | `20:50:0d:ca:b2:bc` |
| Firmware gebaut | 24.08.2026 12:11 | 28.08.2026 20:14 |
| Config-Entry | `01M08HH8349N52K2MR1RABCV3T` | derselbe |
| Entities | 29 × `stockwaage_*` | dieselben 29 |

Die Regel dahinter, und sie gilt für jeden weiteren Boardwechsel:

> **ESPHome-Config-Entries hängen am Gerätenamen, nicht an der Hardware.**
> Bleibt `name:` gleich, übernimmt neue Hardware die bestehende Identität
> mitsamt allen Entity-IDs — HA tauscht nur MAC und IP. Ein Boardwechsel
> allein erzeugt **keine** neuen Entities.

Warum die mDNS-Ankündigung trotzdem `bienenwaage` lautete, ließ sich in dieser
Sitzung nicht abschließend klären; der Zugriff auf die Add-on-Dateien fehlt
(siehe Punkt 7). Für das Ergebnis ist es ohne Belang: maßgeblich ist der Name,
den das Gerät über die API meldet, und der war `stockwaage`.

**Nebenbefund:** Die Bestätigung über die Oberfläche war vorher an der
Namensauflösung gescheitert, nicht am Schlüssel —
`Timeout while resolving IP address for ['bienenwaage.local']`. Alle anderen
ESPHome-Geräte dieser Installation werden über feste IPs angesprochen. Über
`192.168.1.115` lief es sofort durch.

## 2. Was daraus folgte

Weil das Dashboard bereits auf die richtigen, lebenden Entities zeigte, war
nichts kaputt — es gab technisch nichts umzuhängen. Die gewünschte Benennung
war damit kein Repoint mehr, sondern ein Rename, und das ist die Operation,
vor der `CLAUDE.md` warnt.

Gewählt wurde: **nur in der HA-Registry umbenennen, die ESPHome-YAML
unangetastet lassen.** Begründung:

- Ein `entity_id`-Rename erhält die Historie samt Langzeitstatistik
  (HA ≥ 2022.4 zieht die `statistic_id` mit).
- Registry-Namen gewinnen gegen die von der Integration vorgeschlagenen. Ein
  erneuter Flash macht die Umbenennung deshalb **nicht** rückgängig.
- Der Weg über `geraete_name`/`anzeige_name` in der YAML hätte HA neue
  Entities anlegen lassen und die bisherige Messreihe an toten IDs hängen
  lassen — genau der dokumentierte Schadensfall.

## 3. Was geändert wurde

**Gerät:** „Stockwaage" → „Bienenwaage" (`name_by_user`). Damit ziehen die
Anzeigenamen aller Geräte-Entities automatisch mit — aus „Stockwaage Gewicht"
wird „Bienenwaage Gewicht".

**26 ESPHome-Entities** von `stockwaage_*` auf `bienenwaage_*`. Zwei davon
trugen zusätzlich einen Area-Präfix, der bei den übrigen 24 fehlte; er wurde
bei der Gelegenheit entfernt:

| alt | neu |
|---|---|
| `binary_sensor.garten_stockwaage_wachhalten_schalter` | `binary_sensor.bienenwaage_wachhalten_schalter` |
| `switch.garten_stockwaage_hx711_versorgung` | `switch.bienenwaage_hx711_versorgung` |

**Drei Rechenhelfer.** Erst die Quelle über den Options-Flow auf
`sensor.bienenwaage_gewicht` umgehängt, dann die entity_id umbenannt. Die
Reihenfolge ist nicht beliebig: Entity-Registry-Renames aktualisieren die im
Config-Entry gespeicherte Quelle **nicht**, der Helfer zeigte sonst dauerhaft
auf eine tote ID.

| Helfer | Typ | Fenster |
|---|---|---|
| `sensor.bienenwaage_gewichtsanderung` | derivative | 6 h → kg/d |
| `sensor.bienenwaage_gewichtsverlust_kurz` | derivative | 20 min → kg/h |
| `sensor.bienenwaage_tagesbilanz` | statistics `change` | 24 h |

Anzeigenamen wurden dabei über die Registry gesetzt, weil Options-Flows den
Schlüssel `name` nicht annehmen.

**`input_boolean.stockwaage_wachhalten`** → `input_boolean.bienenwaage_wachhalten`.

**Dashboard `bienen-stockwaage-esp32`:** alle 47 Referenzen, Titel jetzt
„Bienenwaage". Die `url_path` bleibt — sie lässt sich nicht ändern, damit
bleiben Sidebar-Eintrag und Lesezeichen heil. Erfasst wurden neben den
`entity:`-Feldern auch die vier View-Badges, die `entities`-Listen der Graph-
und Entities-Karten und die Jinja2-Ausdrücke in fünf Markdown-Karten. Badges
und Templates hängen neben den Karten, nicht darin; eine kartenorientierte
Suche übersieht sie.

## 4. Prüfung

Nach dem Verfahren aus `references/safe-refactoring.md`: erst alle Konsumenten
suchen, ändern, danach auf Reste prüfen.

- Konsumenten vorher: 47 Dashboard-Referenzen, 4 Helfer. **Keine**
  Automationen, Skripte oder Szenen — die vier in `waage-eg-notes.md`
  beschriebenen Automationen existieren nicht mehr.
- Dashboard-Suche nach `stockwaage` nachher: **0 Treffer.**
- `input_boolean.stockwaage_wachhalten` liefert 404, ist also wirklich weg.
- Alle neuen IDs sind in der State Machine mit korrekten Anzeigenamen.

Der Titel des ESPHome-Config-Entrys wurde nachgezogen (`config_entries/update`),
er lautet in der Integrationsliste jetzt ebenfalls „Bienenwaage".

Verbleibende Nennungen von „Stockwaage" gibt es damit nur noch in den
**Config-Entry-Titeln der drei Helfer** (Einstellungen → Helfer). Das ist
kosmetisch: Entity-IDs und Anzeigenamen sind umgestellt. Options-Flows nehmen
keinen neuen Titel an, und für Helfer gibt es kein Gegenstück zu
`config_entries/update` — Umbenennen ginge dort nur über Löschen und
Neuanlegen, und das kostete die Historie. Bewusst so gelassen.

## 5. Die IP-Verwirrung, und was wirklich dahinterstand

Nach dem Umbau meldete HA weiter Verbindungsfehler, und im Log stand
`192.168.1.171` — die Adresse des alten Boards. Der naheliegende Schluss war
ein veralteter Host im Config-Entry. Er war falsch.

**Der Host stand längst auf `192.168.1.115`.** Belegt zweifach: die aktuelle
Reconnect-Zeile lautete `Can't connect to ESPHome API for stockwaage @
192.168.1.115`, und das Reconfigure-Formular kam mit `default:
"192.168.1.115"` zurück, also mit dem gespeicherten Wert.

> **HA fasst wiederkehrende Logmeldungen zusammen und zeigt sie ab dem ersten
> Auftreten.** Der Eintrag hatte `count: 15` und reichte bis vor den
> Boardwechsel zurück. Die `.171`-Zeilen darin waren Altlasten der Anzeige,
> keine laufenden Versuche. Wer nur auf die erste Zeile schaut, jagt ein
> Gespenst — der Zeitstempel der Gruppe gehört zum *ältesten* Vorkommen.

Die tatsächliche Ursache war banal: **das Gerät schlief.** Der Fehler lautete
`Errno 113 — No route to host`, also eine fehlgeschlagene ARP-Auflösung, nicht
ein abgewiesener Port. Der FRITZ!Box-Eintrag stand dabei weiter auf „home" —
ein gecachter Lease, der über den Tiefschlaf hinweg stehen bleibt und als
Erreichbarkeitsnachweis nichts taugt.

Sobald der Kippschalter am Gehäuse auf an stand, war die Verbindung um
21:59:27 da. Ein Druck auf „Jetzt messen" brachte sofort Werte.

Eine zwischenzeitliche Meldung `Invalid encryption key` erwies sich als
Nebenprodukt der parallelen Flow-Versuche und nicht als echtes Problem — mit
dem gespeicherten Schlüssel verbindet sich das Gerät einwandfrei.

## 6. Endstand nach der Kalibrierung

| Größe | Wert |
|---|---|
| Verbindung | an, seit 21:59:27 |
| Gewicht | 35,5 kg |
| Rohwert | −514.704,75 |
| Temperatur | 20,7 °C |
| Kalibrierfaktor | **−14.081,15** |
| Kalibriert bei | 20,9 °C |
| Messintervall | 60 min |
| WLAN-Signal | −76 dBm |
| Wachhalten (Schalter) | an |

„Kalibriert bei" ist gesetzt, die Temperaturkompensation läuft also. Der
Platzhalter 3.500 ist weg.

**Der Kalibrierfaktor liegt allerdings außerhalb des dokumentierten
Erwartungsbereichs** von −18.000 bis −21.000 counts/kg; das alte Board stand
zuletzt bei −20.780,66. Die Signatur „nur der Referenzpunkt gesetzt" ist es
nicht — die läge bei rund der Hälfte, also etwa −10.000. Denkbar sind eine
geänderte HX711-Verstärkung, eine andere Verschraubung der Mechanik oder ein
ungenauer Kalibriervorgang. **Mit einem bekannten Gewicht gegenprüfen:** legt
man ein Prüfgewicht auf, muss die Anzeige um dessen Masse steigen. Weicht sie
systematisch ab, beide Kalibrierschritte wiederholen.

## 7. Das Repo holt die Wirklichkeit ein

Bis hierher war der Bericht eine HA-Geschichte. Beim Versuch, aus dem Repo zu
flashen, kam heraus, dass das gar nicht ging: **`bienenwaage.yaml` war nie
eingecheckt.** Im Root lagen nur die drei ESP8266-Stockdateien, und
`packages/waage-basis.yaml` trug einen `esp8266:`-Block mit `board: d1_mini`.
Ein Flash von dort hätte nicht etwa Schaden angerichtet, sondern schlicht nicht
gebaut — ESP8266-Firmware lässt sich auf einen ESP32 nicht aufspielen.

An die Datei kam ich selbst nicht heran (Supervisor gesperrt, siehe Punkt 9),
sie kam per Hand. Beim Einchecken fielen zwei Dinge auf.

**Erstens ein Fehler, den die Umbenennung aus Punkt 3 verursacht hatte:**

```yaml
- platform: homeassistant
  id: wachhalten_ha
  entity_id: input_boolean.stockwaage_wachhalten
```

Diesen Helfer hatte ich umbenannt. Der `homeassistant`-Sensor holt seinen
Zustand über genau diese entity_id — sie zeigte danach ins Leere. Folge:
`wachhalten_ha` bekommt nie einen Zustand, `deep_sleep.prevent` läuft nicht,
und der `wait_until` im `on_boot` fällt jedes Mal in seinen 30-Sekunden-Timeout.
Das Wachhalten über HA war damit tot, ohne dass irgendwo eine Fehlermeldung
stand.

> **Die Konsumentensuche vor einer Umbenennung endet nicht in Home Assistant.**
> Dashboards, Automationen, Skripte, Szenen und Helfer waren vollständig
> geprüft — die Firmware nicht, weil sie nicht lesbar war. Genau dort lag die
> einzige verbliebene Referenz. Wer die Datei nicht lesen kann, muss danach
> fragen, statt anzunehmen, dass HA alle Konsumenten kennt.

Die Datei vermerkt jetzt im Kopf, dass dies die **einzige** Stelle ist, an der
die Firmware umgekehrt eine HA-Entity referenziert.

**Zweitens die Aufräumaktion, die der Dateikopf selbst als ausstehend
markiert hatte.** Entfernt wurden `waage-eg.yaml`, `waage-stock2.yaml`,
`waage-stock3.yaml`, der `esp8266:`-Block aus der Basis und die nur von ihm
gelesene Substitution `board: d1_mini`; in `bienenwaage.yaml` fiel das dazu
gehörende `esp8266: !remove` weg. `waage-eg-notes.md` und
`waage-eg-claude-code-kontext.md` bleiben als Archiv stehen.

Mit dem Block ist `restore_from_flash: true` entfallen — eine der harten
Regeln in `CLAUDE.md`. Sie wurde nicht gelöscht, sondern umformuliert: auf dem
ESP8266 zwingend, auf dem ESP32 ohne Entsprechung, und **wer wieder ein
ESP8266-Gerät anlegt, muss beides zurückholen**, dann aber in dessen
Gerätedatei.

**Belegt statt behauptet:** Diff der aufgelösten Konfiguration vor und nach dem
Aufräumen. Einziger Unterschied ist die entfernte Zeile `board: d1_mini`. Alle
Entity-Namen, Globals, Pins und Lambdas identisch.

> **Der Diff ist nicht deterministisch** — hier zum ersten Mal aufgefallen.
> Zwei Läufe von `esphome config` über denselben, unveränderten Baum
> vertauschen die Schlüssel `tag` und `level` in den `logger.log`-Aktionen.
> Falschmeldungen also. Gegenprüfen durch zweimaliges Laufen auf demselben
> Baum, oder gleich `diff <(sort vorher.txt) <(sort nachher.txt)`. In
> `CLAUDE.md` ergänzt.

## 8. GPIO33 weckt nicht

Der Kippschalter hält das Gerät wach, holt es aber nicht aus dem Schlaf.

**Die naheliegenden Erklärungen waren falsch.** Vermutet hatte ich einen
fehlenden `wakeup_pin` oder einen verkehrten Weckpegel. Die Datei entlastet
beides:

```yaml
wakeup_pin:
  number: GPIO33
  inverted: true          # -> level = !is_inverted() = 0, weckt auf LOW
wakeup_pin_mode: KEEP_AWAKE
```

`deep_sleep_esp32.cpp` rechnet `level = !wakeup_pin_->is_inverted()`, hier also
`esp_sleep_enable_ext0_wakeup(GPIO33, 0)`. Der Schalter zieht gegen GND, liefert
also LOW. Das ist korrekt.

**Der Denkfehler steckte in der Pin-Begründung**, und sie klang plausibel:
GPIO33 sei weckfähig *und* habe einen internen Pull-up, anders als GPIO34-39.
Stimmt im Wachbetrieb — im Tiefschlaf nicht:

> ESPHome setzt den Pull-up über den digitalen GPIO-Treiber, also über die
> IO-MUX-Konfiguration. Sobald `esp_sleep_enable_ext0_wakeup()` greift, wird
> der Pad auf die **RTC-Funktion** umgemuxt; dort gilt nur, was über
> `rtc_gpio_pullup_en()` gesetzt wurde, und das ruft ESPHome nicht auf. Der
> Pin schwebt im Schlaf.

Ein schwebender Pin hat keinen definierten Ruhepegel: entweder Phantomweckungen,
oder `KEEP_AWAKE` greift beim Einschlafen und das Gerät legt sich gar nicht erst
hin. Beides sieht von außen aus wie „der Schalter tut nichts". **Abhilfe: extern
10 kΩ von GPIO33 nach 3V3** — dieselbe Beschaltung wie an GPIO34, aber aus einem
anderen Grund: nicht weil der Pin keinen internen Pull-up *hat*, sondern weil er
ihn im Schlaf *verliert*.

**Zweiter Verdächtiger, jetzt im Test:** GPIO33 hing gleichzeitig am
`binary_sensor` `wachhalten_schalter` (digitale IO-Matrix) und am `wakeup_pin`
(RTC-Mux). Der Sensor ist testweise entfernt, mitsamt **beider**
`allow_other_uses` — die waren nur nötig, weil sich zwei Verwender den Pin
teilten; mit nur noch einem wäre die Freigabe selbst ein Validierungsfehler.
`esphome config` bestätigt das: Exit 0, GPIO33 steht in der aufgelösten
Konfiguration nur noch einmal.

Auswertung des Tests:

- **Weckt es jetzt** → der Konflikt ist nachgewiesen. Der Sensor kommt dann
  *nicht* einfach zurück; die Anzeige muss über `esp_sleep_get_wakeup_cause()`
  gelöst werden statt über eine zweite Pin-Belegung.
- **Weckt es nicht** → der Block gehört wieder her, Zustand in der
  Git-Historie.

Solange der Sensor draußen ist, wird
`binary_sensor.bienenwaage_wachhalten_schalter` nach dem Flash `unavailable`;
auf dem Dashboard hängt daran die Kachel „Schalter am Gerät" in der Übersicht.

## 9. Warum die Entities im Schlaf leer sind

Am Morgen des 29.08. standen alle Entities auf `unavailable`, was vorher nicht
so war. Der erste Verdacht — das Gerät sei ausgefallen — war falsch: die
Stundenstatistik von `sensor.bienenwaage_gewicht` zeigt **17 lückenlose
Stunden** von 13:00 bis 05:00, jede mit Messwert. Die Waage hat die ganze Nacht
zuverlässig gemessen.

**HA behandelt Deep-Sleep-Geräte eigentlich anders.** Meldet ein Gerät beim
Verbinden `has_deep_sleep`, behält HA die letzten Werte, statt beim Trennen auf
`unavailable` zu gehen. Das Gerät meldet es auch — nachgesehen im
Diagnose-Dump der Integration:

```json
"device_info": { "name": "stockwaage", "has_deep_sleep": true }
```

Der Mechanismus ist also scharf. Er greift trotzdem nicht, und der Grund steht
im Log:

```
02:18:04  disconnect request failed
          TimeoutAPIError: Timeout waiting for DisconnectResponse after 10.0s
05:11:54  handshake timeout; disconnecting
          Home Assistant 2026.8.3 (192.168.1.34): is unresponsive; disconnecting
```

> **Es kommt darauf an, WIE die Verbindung endet.** Nur ein sauberer,
> angekündigter Abschied vor dem Einschlafen sagt HA „halt die Werte". Bricht
> die Verbindung unerwartet ab — Timeout, Reset, „unresponsive" —, wertet HA
> das als Ausfall und setzt alles auf `unavailable`, `has_deep_sleep` hin oder
> her.

**Ursache ist die Funkstrecke.** −76 dBm gegenüber −67 dBm am alten Board, also
rund 8 dB weniger und damit grob ein Sechstel der Empfangsleistung. Auf dem
alten Board kam der Abschied durch, jetzt nicht mehr zuverlässig. Gleiches
Gerät, gleiche Firmware-Logik — nur schlechterer Empfang.

**Verstärkt wurde es durch eine HA-Option**, die in den Config-Entry-Options
stand:

```json
"options": {"allow_service_calls": true, "subscribe_logs": true}
```

`subscribe_logs` lässt HA die Logausgabe des Geräts live abonnieren. In einem
200-Sekunden-Weckfenster auf schwacher Strecke kostet das spürbar Bandbreite,
und es erklärt die 468 gesammelten Logzeilen samt HA-Meldung „logging too
frequently, 200 messages". **Am 29.08. auf `false` gesetzt** — sofort wirksam,
kein Flash nötig, jederzeit zurückzunehmen.

**Am 29.08. um 07:15 schien es zu reichen — die Schlussfolgerung war falsch.**
Die Wachphase lief von ~07:04 bis 07:07:16, danach Tiefschlaf, und die Sensoren
hielten ihre Werte:

| Entity | Wert | zuletzt aktualisiert |
|---|---|---|
| Gewicht | 35,2 kg | 07:06:23 |
| Temperatur | 17,4 °C | 07:06:23 |
| Betriebszeit | 90,2 s | 07:06:23 |
| Rohwert Streuung | 14,8 counts | 07:06:23 |
| WLAN-Signal | −76,0 dBm | 07:04:01 |
| Verbindung | off | 07:07:16 |

Keine der drei kritischen Meldungen trat im Weckfenster erneut auf: das letzte
`Timeout waiting for DisconnectResponse` steht weiterhin auf 02:18:04, das
letzte `handshake timeout` / `is unresponsive` auf 05:11:54, das letzte
`Timeout waiting for DeviceInfoResponse` auf 02:17:54. Übrig bleibt nur
`Can't connect … Errno 113` um 07:07:34 — HA, das während des Schlafs
weiterversucht, harmlos und bauartbedingt.

**Beim nächsten Zyklus war es wieder kaputt.** Um 08:01:58 stand erneut

```
Error getting setting up connection for 192.168.1.115: Connection closed
stockwaage @ 192.168.1.115: disconnect request failed
  TimeoutAPIError: Timeout waiting for DisconnectResponse after 10.0s
```

im Log, und alle Entities gingen wieder auf `unavailable`.

> **Ein einzelner guter Zyklus beweist bei einem sporadischen Fehler nichts.**
> Genau das war der Denkfehler: aus einer Stichprobe von eins wurde eine
> Bestätigung gemacht. Bei einem Fehler, der vorher auch nicht in jedem Zyklus
> auftrat, muss über mehrere Zyklen gemessen werden — mindestens so viele, wie
> vorher zwischen zwei Ausfällen lagen.

`subscribe_logs: false` bleibt trotzdem richtig: es nimmt Last aus dem
Weckfenster und kostet nichts. Es reicht nur nicht.

**Nebenbei am 29.08. um 07:38:59: Home Assistant wurde neu gestartet.** Zu
erkennen daran, dass alle `first_occurred`-Zeitstempel der Log-Gruppen auf
diesen Moment zurücksprangen und die Helfer um 07:38:56 kurz auf `unavailable`
gingen. Wer die Logs danach liest, darf die Gruppen nicht für frisch halten —
dasselbe Muster wie die zusammengefassten Meldungen aus Punkt 5, nur
andersherum.

Nebenbefund aus derselben Messung: die Rohwert-Streuung liegt bei 14,8 counts,
also unter einem Gramm (200 counts ≈ 10 g). Mechanik und HX711 arbeiten sauber.
Die Betriebszeit von 90,2 s beim Veröffentlichen passt exakt zur eingestellten
`einschwingzeit` — der `on_boot`-Ablauf läuft wie entworfen.

**Nicht verwechseln:** `binary_sensor.bienenwaage_verbindung` geht beim Trennen
auf `off` und nicht auf `unavailable`. Das ist Absicht — im Diagnose-Dump
trägt sie `is_status_binary_sensor: true` und ist genau dafür da, den
Verbindungszustand anzuzeigen.

## 10. Offene Punkte

- **Kalibrierfaktor gegenprüfen.** −14.081,15 statt der erwarteten −18.000 bis
  −21.000, siehe Punkt 6. Mit bekanntem Gewicht: die Anzeige muss um dessen
  Masse steigen.
- **10 kΩ von GPIO33 nach 3V3 nachrüsten**, siehe Punkt 8. Hardware, unabhängig
  von jeder YAML-Änderung.
- **Den GPIO33-Test auswerten** und je nach Ergebnis den `binary_sensor`
  zurückholen oder die Anzeige neu lösen.
- **Feste IP vergeben.** Für MAC `20:50:0D:CA:B2:BC` eine Lease-Reservierung in
  der FRITZ!Box. Das alte Board hatte `.171`, dieses hat `.115` — ohne
  Reservierung wandert die Adresse wieder. `bienenwaage.local` ist keine
  Alternative: die mDNS-Auflösung ist hier nachweislich unzuverlässig, daran
  scheiterte schon die erste Adoption.
- **Kippschalter wieder umlegen**, sonst gibt es keinen Tiefschlaf.
- **Kein Supervisor-Zugriff über den MCP-Server.** `/addons` antwortet
  `Unauthorized`, und auch der Umweg über den HA-Core-Proxy `supervisor/api`
  wird abgewiesen. Damit sind Add-on-Dateien und die WebSocket-Schnittstelle
  des Device Builders von außen nicht lesbar — jede YAML muss von Hand kommen.
- **Helfer-Titel.** Die drei Rechenhelfer heißen in Einstellungen → Helfer
  weiterhin „Stockwaage …". Kosmetisch; Umbenennen ginge nur über Löschen und
  Neuanlegen und kostete die Historie.
- **Funkstrecke verbessern — der wichtigste Punkt.** −76 dBm gegenüber −67 dBm
  am alten Board. Unter −80 dBm wird es laut eigener Dashboard-Doku wackelig,
  und schon jetzt kostet es den sauberen Abschied vor dem Einschlafen (Punkt 9).
  Standort, Antennenausrichtung oder ein Repeater in Reichweite.
- **Die Entities gehen weiterhin in den Schlafphasen auf `unavailable`.**
  `subscribe_logs: false` hat nicht gereicht (Punkt 9). Offene Hebel: längere
  `run_duration`, bessere Funkstrecke, oder das Dashboard so bauen, dass es den
  zuletzt gemessenen Wert zeigt statt den Live-Zustand. Welcher davon der
  richtige ist, hängt daran, ob das Gerät am Netzteil oder am Akku hängt — der
  Kommentar bei `power_save_mode` in `bienenwaage.yaml` spricht von Netzteil,
  ist aber aus der Portierungsphase und womöglich veraltet.
