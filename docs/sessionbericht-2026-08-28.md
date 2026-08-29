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

## 10. Dashboard von der Verbindungsqualität entkoppelt

Nachdem `subscribe_logs: false` nicht gereicht hat, wurde die Anzeige von der
Verbindung getrennt, statt weiter an der Verbindung zu drehen. Begründung: Bei
einer Waage, die stündlich misst, ist „der zuletzt gemessene Wert" ohnehin die
ehrlichere Darstellung als „gerade nicht verbunden". Die Wachzeit blieb auf
Wunsch unangetastet.

**Vier Template-Helfer**, die den letzten gültigen Wert halten:

| Helfer | Quelle |
|---|---|
| `sensor.bienenwaage_gewicht_zuletzt` | `sensor.bienenwaage_gewicht` |
| `sensor.bienenwaage_temperatur_zuletzt` | `sensor.bienenwaage_temperatur` |
| `sensor.bienenwaage_tagesbilanz_zuletzt` | `sensor.bienenwaage_tagesbilanz` |
| `sensor.bienenwaage_gewichtsanderung_zuletzt` | `sensor.bienenwaage_gewichtsanderung` |

Alle nach demselben Muster:

```jinja
{% set v = states('sensor.…') %}
{{ v if v not in ['unknown','unavailable'] else this.state }}
```

Der Skill bestätigt, dass es dafür keinen dedizierten Helfer gibt — „letzten
Wert halten" ist genau der Fall, für den der Template-Helfer als Notausgang
vorgesehen ist. Angelegt über den Config-Flow, nicht als YAML.

**Bewusst ohne `state_class`.** Mit einer wäre eine zweite, inhaltlich
identische Langzeitstatistik entstanden. Die Statistik gehört weiter zu den
Originalsensoren, und genau deshalb bleiben auch alle `statistics-graph`- und
`history-graph`-Karten auf den Originalen: die lesen aus dem Recorder und
funktionieren unabhängig von der Verbindung ohnehin.

**Additiv, nicht ersetzend.** Die bestehenden Helfer wurden *nicht* umgehängt —
das wäre die dritte Quelländerung an Entities mit Historie gewesen, für einen
reinen Anzeigezweck. Die vier neuen Helfer lassen sich rückstandsfrei löschen.

**Geändert im Dashboard**, nur in der Ansicht *Übersicht*: die Badges Gewicht
und Temperatur, die Kacheln Gewicht, Bilanz 24 h, Änderung und Temperatur. Die
Ansicht *Technik* bleibt auf den Originalsensoren — dort ist „nicht verfügbar"
eine ehrliche Diagnoseinformation und keine Störung. Die Karte „Zuletzt
aktualisiert" zeigt jetzt die letzte Übertragung und dazu, ob das Gerät gerade
wach ist oder der angezeigte Wert der letzte gemessene ist.

Nebenbei korrigiert: der Tiefschlaf-Text sprach von „120 s Wachzeit", die
`run_duration` steht aber auf 200 s.

> **Grenze des Verfahrens:** `this.state` überlebt keinen Neustart von Home
> Assistant. Danach stehen die Haltesensoren wieder auf `unknown`, bis die
> nächste Messung kommt — bei stündlichem Takt also höchstens eine Stunde.

**Noch nicht verifiziert.** Zum Zeitpunkt des Umbaus (09:03) hatte sich das
Gerät seit 08:01:58 nicht gemeldet, die Haltesensoren standen deshalb noch auf
`unknown`. Die Wirkung lässt sich erst nach mindestens zwei aufeinander
folgenden Schlafphasen beurteilen — siehe den Fehlschluss in Punkt 9.

## 11. Der Nachschlag: es war nicht nur die Anzeige

Nach dem Umbau aus Punkt 10 standen weiter Kacheln auf „nicht verfügbar". Die
Prüfung zeigte, dass zwei verschiedene Dinge dahintersteckten.

**Erstens: die Rechenhelfer waren selbst kaputt, nicht nur ihre Darstellung.**
Alle drei gingen mit der Quelle auf `unavailable`, und
`sensor.bienenwaage_gewichtsanderung` hatte deshalb seit dem Anlegen des
Haltesensors *nie* einen gültigen Wert. Ein weiterer Wrapper hätte daran
nichts geändert — er kann nur halten, was er einmal gesehen hat.

> **Wenn der Wrapper leer bleibt, ist nicht der Wrapper das Problem, sondern
> die Quelle.** Ein Haltesensor über einer Quelle, die nie einen gültigen Wert
> liefert, bleibt auf `unknown`. Das ist kein Fehler des Verfahrens, sondern
> sein korrektes Verhalten — und ein Hinweis, eine Ebene tiefer zu schauen.

Deshalb wurden die drei Helfer doch noch auf `sensor.bienenwaage_gewicht_zuletzt`
umgehängt, also auf die lückenlose Quelle. Damit sind sie dauerhaft verfügbar,
und die beiden Wrapper `tagesbilanz_zuletzt` und `gewichtsanderung_zuletzt`
wurden überflüssig und gelöscht. Übrig bleiben zwei Haltesensoren statt vier.

Für die Rechnung ist das kein Nachteil, sondern ein Vorteil: eine
stufenförmig gehaltene Reihe ist für eine Ableitung über 6 h und für
`statistics/change` über 24 h genau das Richtige — Lücken sind es nicht.

Nach dem Umhängen stehen die beiden Ableitungen zunächst auf `unknown`: sie
wurden neu geladen und brauchen zwei Messpunkte, füllen sich also innerhalb
weniger Stunden. `unknown` ist dabei nicht dasselbe wie `unavailable` — die
Entity existiert und ist erreichbar, sie hat nur noch keinen Wert.

**Zweitens: Buttons kann man nicht halten.** `button.bienenwaage_jetzt_messen`,
`button.bienenwaage_tara` und `binary_sensor.bienenwaage_wachhalten_schalter`
sind im Schlaf zu Recht nicht verfügbar — ein schlafendes Gerät nimmt keinen
Tastendruck an, und eine Kachel, die Verfügbarkeit vortäuscht, wäre gelogen.

Gelöst über `conditional`-Karten, die an
`binary_sensor.bienenwaage_verbindung` hängen: im Wachbetrieb erscheinen die
Bedienelemente, im Schlaf an ihrer Stelle ein Text, der erklärt, warum sie
gerade fehlen und wie man sie erreicht. Ausgeblendet statt ausgegraut.

**Die Kachel „Temp.-Korrektur" wurde aus der Übersicht entfernt.** Sie ist ein
Diagnosewert und steht in der Ansicht *Technik* ohnehin — dort gehört sie hin,
und dort ist „nicht verfügbar" die richtige Auskunft.

**Stand danach:** Gewicht 35,2 kg, Temperatur 18,8 °C, Tagesbilanz 0,0 kg —
alle belegt, während das Gerät seit 10:38:21 schläft.

## 12. Auch die Ansicht *Technik*

In Punkt 10 war *Technik* bewusst ausgelassen worden, mit dem Argument, dort
sei „nicht verfügbar" eine ehrliche Diagnoseauskunft. Das war aus der
Schreibtischperspektive richtig und aus der Benutzerperspektive falsch: wer
täglich auf eine Seite voller grauer Kacheln schaut, liest sie als Defekt, nicht
als Auskunft.

Umgestellt nach demselben Muster, aber mit einer bewussten Trennung:

**Messwerte werden gehalten** — neun weitere Template-Helfer für
Kalibrierfaktor, Kalibriert bei, Kalibriert mit, Rohwert, Rohwert Streuung,
WLAN-Signal, Temperatur Mittel, Temp.-Korrektur und Gewicht verworfen. Die
beiden Markdown-Karten, die `Kalibriert mit` und `Temp.-Korrektur` per Jinja2
einsetzen, zeigen jetzt ebenfalls auf die Haltesensoren.

> Achtung bei der Benennung: „Temp.-Korrektur" wird zu
> `sensor.bienenwaage_temp_korrektur_zuletzt` — der Punkt entfällt, es wird
> **nicht** `temperaturkorrektur_zuletzt`. Wer die ID rät, rät falsch.

**Bedienelemente werden ausgeblendet** — Messintervall, Referenzgewicht,
Koeffizient, Durchsichtmodus, Durchsichtdauer und die vier Kalibrier-Buttons
hängen an `conditional`-Karten. Ein schlafender ESP32 nimmt keine Änderung an;
eine Kachel, die Bedienbarkeit vortäuscht, führt in die Irre. Im Schlaf steht
an ihrer Stelle ein Text, der erklärt warum und wie man herankommt.

**Die Kalibrier-Sperre** ist ebenfalls konditional statt gehalten: sie zählt
Restminuten herunter und ist im Schlaf bedeutungslos.

**Die Betriebszeit** zeigte durch `| int(0)` bisher „0d 0h 0m", wenn die Quelle
weg war — eine Null, die wie eine Messung aussieht. Jetzt erscheint stattdessen
ein Gedankenstrich mit dem Hinweis, dass das Gerät schläft.

### Die Bilanz dieses Verfahrens, ehrlich

Es stehen jetzt **elf Template-Helfer** nur für die Anzeige. Das ist viel
Maschinerie für ein Problem, das eine einzige Zahl in der Firmware auch löst:
Das Gerät hängt am Netzteil, eine deutlich längere `run_duration` — oder gar
kein Tiefschlaf — würde das Verschwinden der Entities von vornherein
verhindern und alle elf Helfer überflüssig machen.

Dafür ist der gewählte Weg von der Verbindungsqualität unabhängig: er wirkt
auch dann, wenn die Funkstrecke schlechter wird, und er braucht keinen Flash.
Beides sind vertretbare Positionen; die Entscheidung fiel bewusst für die
Anzeige und gegen den Eingriff in die Firmware.

**Messwerte beim Umbau** (Gerät kurz wach): Kalibrierfaktor −14.081,15,
Kalibriert bei 20,9 °C, Kalibriert mit **26,22 kg**, Rohwert −509.448,94,
Rohwert Streuung **152,0 counts**, WLAN **−78 dBm**, Temperatur Mittel
19,7 °C, Temp.-Korrektur −0,040 kg, Gewicht verworfen 0.

Zwei davon sind erwähnenswert: „Kalibriert mit" steht auf 26,22 kg statt der
früheren 33,62 kg — es wurde also mit einem anderen Prüfgewicht neu
kalibriert. Und das WLAN ist von −76 auf **−78 dBm** gerutscht, womit bis zur
Wackelgrenze von −80 dBm kaum noch Luft bleibt.

## 13. Zurückgenommen: das Ausblenden der Bedienelemente

Punkt 12 hat einen Fehler eingebaut, und zwar einen handfesten. Die
`conditional`-Karten haben die vier Kalibrier-Buttons, Messintervall,
Referenzgewicht, Koeffizient, Durchsichtmodus und Durchsichtdauer
ausgeblendet, solange `binary_sensor.bienenwaage_verbindung` nicht `on` war.

**Die Rechnung dazu war nie aufgestellt worden.** Bei 60 min Schlafdauer und
200 s Wachzeit ist das Gerät rund **3 von 60 Minuten** erreichbar, also 5 % der
Zeit. Die Bedienelemente waren damit in 95 % aller Blicke aufs Dashboard nicht
vorhanden — nicht ausgegraut, sondern weg. Wer kalibrieren wollte, fand die
Buttons nicht und hatte keinen Anhaltspunkt, wonach er suchen sollte.

Ausgegraut wäre in jeder Hinsicht besser gewesen: die Kachel ist auffindbar,
ihre Position ist gelernt, und im richtigen Moment lässt sie sich drücken.

Die Trennung aus Punkt 12 war also an der falschen Kante gezogen. Sie lautet
jetzt:

- **Alles, was man drückt oder einstellt, ist immer sichtbar** — Buttons,
  Number-Felder, Schalter. Im Schlaf ausgegraut, aber da.
- **Reine Anzeigen** zeigen entweder den gehaltenen letzten Wert (die elf
  `_zuletzt`-Helfer) oder bleiben ausgeblendet, wo es keinen gehaltenen Wert
  gibt.

Danach ist genau **eine** `conditional`-Karte übrig: die Kalibrier-Sperre unter
*Diagnose*. Sie zählt Restminuten herunter und ist im Schlaf tatsächlich
bedeutungslos — für sie gibt es bewusst keinen Haltesensor.

Die Erklärtexte, die im Schlaf an Stelle der Bedienelemente standen, sind
entfallen. Ihr Inhalt steckt jetzt als Vorbemerkung in den ohnehin vorhandenen
Markdown-Karten daneben, mit „ausgegraut" statt „ausgeblendet".

**Mit entfernt: die Kachel *Schalter am Gerät*** (Übersicht, Abschnitt
Bedienung). Sie hing an `binary_sensor.bienenwaage_wachhalten_schalter`, und
genau dieser `binary_sensor` ist für den GPIO33-Test aus der Firmware
genommen worden (Punkt 8). Sie unbedingt sichtbar zu machen hätte eine Kachel
ergeben, die nach dem nächsten Flash dauerhaft auf „nicht verfügbar" steht.
Der erklärende Text darunter bleibt — der Kippschalter existiert weiter, er
wird nur nicht mehr als Entity mitgelesen. Fällt der Test aus wie erhofft und
kommt der `binary_sensor` zurück, kommt auch die Kachel zurück.

### Was dieser Hin- und Rückweg zeigt

Graue Kacheln und versteckte Bedienelemente sind dieselbe Münze. Ein Gerät,
das 95 % der Zeit nicht erreichbar ist, lässt sich im Dashboard nicht so
darstellen, dass beides gleichzeitig gut ist — man verschiebt den Mangel nur
von einer Stelle an die andere.

Der einzige Weg, beides zu haben, ist das Gerät erreichbar zu machen. Es
hängt am Netzteil; `run_duration` von 200 s auf etwa 600 s zu erhöhen oder den
Tiefschlaf ganz abzuschalten, löst das Problem an der Wurzel und macht
nebenbei alle elf Haltesensoren überflüssig. Das steht weiterhin offen.

## 14. Das Gerät ließ sich nicht mehr flashen — zwei Ursachen, eine davon hausgemacht

Meldung: „gerät lässt sich nicht flashen, da es für ESPHome auch in der
Wachphase nicht erreichbar ist". Dahinter steckten zwei voneinander
unabhängige Fehler, die sich gegenseitig verdeckt haben.

### 14.1 Das Wachhalten war stumm ausgefallen — durch die Umbenennung

Die Firmware, die auf dem ESP32 **läuft**, wurde vor dem 28.08.2026
geflasht. Sie enthält:

```yaml
- platform: homeassistant
  id: wachhalten_ha
  entity_id: input_boolean.stockwaage_wachhalten
```

Am 28.08.2026 wurde der Helfer in der Entity-Registry auf
`input_boolean.bienenwaage_wachhalten` umbenannt. Damit gab es die alte ID
nicht mehr. Der `homeassistant`-binary_sensor bekommt dann **nie** einen
Zustand: `on_state` feuert nicht, `deep_sleep.prevent` läuft nicht, und der
`wait_until` im Weckfenster läuft jedes Mal in seinen 30-s-Timeout.

Genau davor warnt der Kommentar an der Stelle — geschrieben, nachdem der
Fehler beim Nachziehen der Datei aufgefallen war. Übersehen wurde dabei, dass
die **Korrektur in der Datei den Schaden am Gerät nicht behebt**: sie wirkt
erst mit dem nächsten Flash. Genau der war dadurch blockiert.

Belegt am 29.08.2026: `input_boolean.bienenwaage_wachhalten` stand seit 12:12
auf `on`, das Gerät war um 12:24 trotzdem wieder weg.

**Der zweite Weg war gleichzeitig tot.** Der Kippschalter am Gehäuse wirkt
über `wakeup_pin_mode: KEEP_AWAKE` an GPIO33 — und GPIO33 schwebt im
Tiefschlaf, solange der externe 10-kΩ-Pull-up fehlt (Punkt 8). Beide
Wachhaltewege gleichzeitig aus, aus zwei völlig verschiedenen Gründen.

**Behoben ohne Flash:** In HA einen zweiten `input_boolean` angelegt und
seine entity_id auf `input_boolean.stockwaage_wachhalten` gesetzt — exakt die
ID, nach der die laufende Firmware fragt. Er steht auf `on` und hat eine
eigene Kachel in der Übersicht, mit dem Hinweis, dass er nach dem Flash zu
löschen ist. Der Helfer-Name lässt sich dabei nicht wiederverwenden: der
umbenannte Helfer trägt intern weiterhin die object_id `stockwaage_wachhalten`
und blockiert den Namen. Angelegt wurde er deshalb als „Stockwaage Wachhalten
OTA-Brücke" und danach die entity_id gesetzt.

> **Die allgemeine Lehre**, teurer als der Einzelfall: Eine
> Firmware-Referenz auf eine HA-Entity darf nicht umbenannt werden, ohne
> vorher zu flashen — oder ohne die alte ID als Brücke stehen zu lassen. Wird
> die Referenz gebraucht, um überhaupt flashen zu können, ist die Reihenfolge
> nicht mehr frei wählbar: **erst flashen, dann umbenennen.** Umgekehrt sperrt
> man sich aus.

### 14.2 ESPHome suchte das Gerät über mDNS

Der zweite Grund steckte im aufgelösten Config und war im YAML nicht zu
sehen. Ohne `use_address` setzt ESPHome die Adresse selbst:

```
use_address: stockwaage.local
```

mDNS ist in diesem Netz nachweislich nicht auflösbar — daran scheiterte schon
die erste Adoption. Das Device-Builder-Add-on zeigt das Gerät deshalb auch
dann als OFFLINE, wenn es wach ist und HA längst über die API damit redet:
**HA hat die IP im Config-Entry stehen, das Add-on hat nur den Namen.** Die
beiden Wege sind unabhängig, und aus „HA sieht es" folgt nicht „ESPHome
erreicht es".

Gesetzt: `use_address: 192.168.1.115`. Das ist ausdrücklich **nicht**
`manual_ip` — auf dem ESP32 ändert sich nichts, er bleibt am DHCP. Wandert
die Lease, schlägt nur der Upload fehl und die Zeile ist nachzuziehen;
unerreichbar wird das Gerät dadurch nie. Die feste Lease in der FRITZ!Box
bleibt der saubere Weg.

#### Woher die .171 kommt — nachgelesen, nicht vermutet

Nachdem der Builder danach *wieder* auf `192.168.1.171` zugriff, wurde die
Kette in ESPHome 2026.6.5 im Quelltext verfolgt statt geraten:

1. **`dashboard/web_server.py:433`** — die Weboberfläche ruft die CLI mit
   `--device <port>` auf, wobei `port` die Auswahl im Install-Dialog ist.
   Für „Wirelessly" ist das die Zeichenkette `OTA`.
2. **`dashboard/web_server.py:381–392`** — vorher baut sie
   `--mdns-address-cache`/`--dns-address-cache`-Argumente. Endet
   `entry.address` auf `.local`, wird der **mDNS-Cache der Weboberfläche**
   mitgegeben. Dort steht die Adresse, unter der `stockwaage.local` zuletzt
   gesehen wurde — beim alten Board `192.168.1.171`.
3. **`dashboard/entries.py:389`** — `entry.address` kommt aus
   `.esphome/<datei>.json`, nicht aus der YAML. Geschrieben wird die Datei
   erst beim **Übersetzen** (`writer.py:143–168`), sie hinkt also hinterher.
4. **`__main__.py:293–316`** — die CLI löst `OTA` so auf:

   ```python
   elif device == "OTA":
       # ensure IP adresses are used first
       if is_ip_address(CORE.address) and ...:
           resolved.extend(_resolve_with_cache(CORE.address, purpose))
       ...
           if has_ota() and has_non_ip_address() and has_resolvable_address():
               resolved.extend(_ota_hostnames_for_default(purpose))
   ```

   `CORE.address` ist `use_address` aus der **gerade geladenen YAML**. Ist es
   eine IP, wird sie direkt genommen und gar nicht mehr aufgelöst. Ist es ein
   Name, geht es über die Namensauflösung — und dort greift der unter 2.
   mitgegebene Cache mit der `.171`.

   Der Fehlertext der Stelle empfiehlt von sich aus: *„If you know the IP, set
   'use_address' in your network config."*

Zwei Folgerungen, beide nicht offensichtlich:

- **Die Änderung im Repo tut gar nichts, solange die Datei nicht in
  `/config/esphome/` liegt.** Das Add-on übersetzt ausschließlich das, was
  dort steht. Wer „aus GitHub flasht", kopiert von Hand — und wer das
  vergisst, bekommt exakt das alte Verhalten samt alter Adresse.
- **Ein vorheriges Übersetzen ist nicht nötig.** Weil `CORE.address` aus der
  YAML kommt und nicht aus `.esphome/<datei>.json`, wirkt `use_address` schon
  beim ersten Versuch. Die veraltete gespeicherte Adresse beeinflusst nur die
  Cache-Argumente, und die sind bedeutungslos, sobald nichts mehr aufzulösen
  ist.

Was dagegen **nicht** hilft: im Install-Dialog einen Eintrag zu wählen, der
buchstäblich `192.168.1.171` anzeigt. Der landet über den `else`-Zweig
(`resolved.append(device)`) unverändert als Ziel. Es muss der Eintrag **OTA**
sein.

### 14.3 Ein laufendes OTA fällt in den Tiefschlaf

Beim Prüfen mitgefunden, noch nicht aufgetreten: `deep_sleep` schaltet nach
`run_duration` ab, ganz gleich ob gerade ein Update läuft. Bei 200 s Wachzeit
abzüglich 90 s Einschwingen bleibt für einen ESP32-Upload zu wenig Luft.

`packages/waage-basis.yaml` hat dafür eine `id: ota_esphome` bekommen — nur
über sie kann die Gerätedatei den Eintrag per `!extend` ergänzen; ein zweiter
`ota:`-Eintrag wäre ein Validierungsfehler. In `bienenwaage.yaml`:

```yaml
ota:
  - id: !extend ota_esphome
    on_begin:
      then: [deep_sleep.prevent: tiefschlaf, ...]
    on_error:
      then: [deep_sleep.allow: tiefschlaf, ...]
```

Das ersetzt das Wachhalten nicht — wach sein muss das Gerät, wenn der Upload
beginnt. Es verhindert nur, dass ein laufender Upload mittendrin
abgeschnitten wird.

### 14.4 Prüfung

`esphome config bienenwaage.yaml` ist gültig. Der Diff der **aufgelösten**
Konfiguration gegen den Stand davor, reihenfolgeunabhängig verglichen, zeigt
ausschließlich: die beiden OTA-Trigger samt Logzeilen, die neue `id`, und

```
< use_address: stockwaage.local
> use_address: 192.168.1.115
```

Keine Entity, kein Global, kein Messwert hat sich bewegt.

### 14.5 Nur die halbe Datei kopiert

Beim ersten Übersetzungsversuch nach der Änderung:

```
Source for extension of ID 'ota_esphome' was not found.
Found multiple target platform blocks: esp8266, esp32. Only one is allowed.
```

Zwei Meldungen, eine Ursache: in `/config/esphome/packages/` lag eine
`waage-basis.yaml` von **vor** dem 28.08.2026. Kopiert worden war nur
`bienenwaage.yaml`.

- `ota_esphome` gibt es in der alten Basis nicht — das `!extend` aus der
  Gerätedatei findet nichts.
- Die alte Basis trägt noch den `esp8266:`-Block. Früher nahm die Gerätedatei
  ihn per `esp8266: !remove` heraus; seit dem 28.08. ist beides gemeinsam
  entfallen. Alte Basis plus neue Gerätedatei ergibt deshalb zwei
  Plattformblöcke.

Keine der beiden Meldungen zeigt auf die eigentliche Ursache, und beide
entstehen erst aus der **Kombination** zweier Dateistände. Das ist der Preis
der Package-Aufteilung, und er fällt genau dann an, wenn eine Änderung über
beide Dateien läuft.

**Regel, jetzt auch in der README:** aus GitHub immer `bienenwaage.yaml`
**und** `packages/waage-basis.yaml` kopieren. Schnelle Probe, ob die Basis
aktuell ist — sie darf kein `esp8266:` und kein `board: d1_mini` mehr
enthalten und muss `id: ota_esphome` haben.

### 14.6 Die Reihenfolge am Gerät

1. **Brücke steht schon auf an** — nichts zu tun.
2. Eine Schlafperiode abwarten (bis zu 60 min). Beim Aufwachen liest die
   laufende Firmware `input_boolean.stockwaage_wachhalten`, findet ihn auf
   `on` und setzt den Tiefschlaf aus. *Gerät wach* bleibt danach an.
3. Im Device Builder **Install → Wirelessly**. Der Upload geht jetzt an
   `192.168.1.115` statt an einen nicht auflösbaren Namen.
4. Nach dem Flash: Kalibrierfaktor und „Kalibriert bei" prüfen.
5. Danach wirkt wieder `input_boolean.bienenwaage_wachhalten`. Die Brücke und
   ihre beiden Karten in der Übersicht löschen.

## 15. Offene Punkte

- **Kalibrierfaktor gegenprüfen.** −14.081,15 statt der erwarteten −18.000 bis
  −21.000, siehe Punkt 6. Mit bekanntem Gewicht: die Anzeige muss um dessen
  Masse steigen.
- **Flashen, dann die Brücke löschen.** Ablauf in Punkt 14.6. Solange
  `input_boolean.stockwaage_wachhalten` existiert, ist es der einzige Weg,
  das Gerät wach zu halten — beide anderen sind aus (Punkt 14.1).
- **`use_address` nachziehen, falls die Lease wandert.** Steht fest auf
  `192.168.1.115`. Erledigt sich mit der Lease-Reservierung.
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
- **Funkstrecke verbessern.** Zuletzt −78 dBm gegenüber −67 dBm
  am alten Board. Unter −80 dBm wird es laut eigener Dashboard-Doku wackelig,
  und schon jetzt kostet es den sauberen Abschied vor dem Einschlafen (Punkt 9).
  Standort, Antennenausrichtung oder ein Repeater in Reichweite.
- **Die Entities gehen weiterhin in den Schlafphasen auf `unavailable`.**
  `subscribe_logs: false` hat nicht gereicht (Punkt 9). Das Dashboard fängt es
  jetzt über die elf Haltesensoren ab (Punkt 10–13), die Ursache ist damit aber
  nicht weg.
- **`run_duration` erhöhen — der wichtigste offene Punkt.** Bestätigt: das
  Gerät hängt am **Netzteil**, der Kommentar bei `power_save_mode` in
  `bienenwaage.yaml` stimmt also. Damit gibt es keinen Grund für 200 s
  Wachzeit. Bei 60 min Schlafdauer ist das Gerät heute 5 % der Zeit
  erreichbar; das ist die gemeinsame Ursache hinter grauen Kacheln,
  versteckten Bedienelementen und dem unsauberen Verbindungsabbruch. Auf
  etwa 600 s zu gehen oder den Tiefschlaf abzuschalten würde alle drei
  erledigen und die elf Haltesensoren überflüssig machen. Bislang bewusst
  nicht gemacht — die Entscheidung fiel für den Dashboard-Weg.
