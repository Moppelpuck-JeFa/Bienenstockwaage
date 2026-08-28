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

## 7. Offene Punkte

- **`bienenwaage.yaml` ist nicht im Repo.** Sie konnte auch nicht geholt
  werden: der MCP-Token hat keinen Supervisor-Zugriff (`/addons` →
  *Unauthorized*), damit sind weder die Add-on-Dateiliste noch die
  WebSocket-Schnittstelle des Device Builders erreichbar. Von Hand einchecken,
  sonst beschreibt das Repo dauerhaft ein Gerät, das es nicht mehr gibt.
- **Kalibrierfaktor gegenprüfen**, siehe Punkt 6.
- **Feste IP vergeben.** Für MAC `20:50:0D:CA:B2:BC` eine Lease-Reservierung
  in der FRITZ!Box setzen. Das alte Board hatte `.171`, dieses hat `.115` —
  ohne Reservierung wandert die Adresse beim nächsten Mal wieder.
  `bienenwaage.local` ist keine Alternative: die mDNS-Auflösung ist in dieser
  Installation nachweislich unzuverlässig, daran scheiterte schon die erste
  Adoption (`Timeout while resolving IP address`).
- **Kippschalter wieder umlegen**, sonst gibt es keinen Tiefschlaf und der
  Akku läuft leer.
- **Die Umbenennung lebt nur in der HA-Registry.** Die YAML sagt weiterhin
  `stockwaage`. Das ist stabil und überlebt Flashes, aber wer nur die YAML
  liest, findet den Namen nicht wieder. Beim nächsten Anfassen der Datei einen
  Kommentar hinterlassen.
- **`waage-eg.yaml` beschreibt Hardware, die nicht mehr läuft** — der
  Config-Entry „Waage" steht auf `unavailable`. Kennzeichnen oder entfernen.
- Die Pin-Belegung in `waage-eg.yaml` ist D1-Mini-Notation und passt nicht zu
  einem ESP32. Der Durchsicht-Taster liegt laut Dashboard-Text auf GPIO34 mit
  externem 10-kΩ-Pull-up.
- **WLAN-Signal beobachten:** −76 dBm gegenüber −67 dBm am alten Board. Laut
  eigener Dashboard-Doku wird es unter −80 dBm wackelig.
