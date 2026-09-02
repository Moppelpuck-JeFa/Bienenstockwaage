# Sessionbericht 2026-09-02 — Diagramme ohne Grafana

## Auftrag

Die Diagramme im Dashboard **Übersicht** (`lovelace`), die bisher als
Grafana-Panels eingebettet waren, mit HA-Bordmitteln nachbauen — ohne Grafana
und ohne InfluxDB.

## Ausgangslage

Sechs Grafana-Panels aus dem Dashboard `adqqz2b` („temperatur") hingen als
`custom:addon-iframe-card` im Dashboard:

| View | Panels |
|---|---|
| View 7 (Icon `mdi:chart-areaspline`, ohne Titel/Pfad) | `panel-1`, `panel-2`, `6` und in einem zweiten Abschnitt `panel-3`, `panel-4` |
| Unteransicht `energie` | `panel-5`, Überschrift „Stromverbrauch letzte 24 h" |

Die Karten zeigten auf die Ingress-URL des Grafana-Add-ons.

**Der Panel-Inhalt ließ sich nicht auslesen.** Der Grafana-Ingress liefert für
API-Zugriffe von außerhalb einer Browser-Session HTTP 403 (nginx im Add-on
lässt nur den Supervisor durch), und der Screenshot-Renderer (Puppet) bekommt
die Ingress-Iframes ebenfalls nicht authentifiziert — die Karten blieben im
Rendering leer. Der Nachbau erfolgte deshalb **nach Entitäten, nicht nach
Vorlage**; welche Serien Grafana genau zeigte, ist nicht belegt.

## Was gebaut wurde

Beide Views wurden ersetzt (`ha_config_set_dashboard(python_transform=…,
config_hash=…)`), die Iframe-Karten sind raus.

**View `diagramme`** (bekam einen stabilen Pfad, vorher `path: ""`):

| Abschnitt | Karten |
|---|---|
| Außentemperatur | `statistics-graph` 48 h Min/Mittel/Max (Stundenwerte) · `statistics-graph` 30 Tage Min/Mittel/Max · `history-graph` 48 h Taupunkt und Gefühlt |
| Innentemperaturen | `statistics-graph` 48 h Stundenmittel (Küche, Schlafzimmer, Nähzimmer, Flur unten, Bad unten) · dieselben fünf als 30-Tage-Tagesmittel |
| Feuchte, Druck, Wind | `statistics-graph` 48 h Feuchte Min/Mittel/Max · `statistics-graph` 48 h Wind Mittel/Max · `history-graph` 48 h Barometer |
| Bienenstockwaage | `statistics-graph` 48 h Gewicht (Stundenmittel) · `statistics-graph` 90 Tage Gewicht (Tagesmittel) · `statistics-graph` 48 h Stocktemperatur Min/Mittel/Max · Tile „Tagesbilanz" mit `trend-graph` über 7 Tage |

**Unteransicht `energie`:** `statistics-graph` 24 h Stundenmittel (Hausverbrauch,
PV-Leistung, Nachteinspeisung, Akku-Ladeleistung), `statistics-graph` 30 Tage
Hausverbrauch Min/Mittel/Max, `history-graph` 48 h PV-Leistung und Akkuspannung
als Rohverlauf. Zusätzlich `back_path` auf `/lovelace/diagramme`.

**Die PV-Serie in „Leistung 24 h" bleibt leer, solange
`sensor.pv_leistung_momentan` kein `state_class` hat.** Der Sensor kommt aus
YAML (kein Helfer, kein Config-Flow), und über ha-mcp ist er nicht erreichbar:
`ha_config_set_yaml` ist in dieser Installation nicht freigeschaltet, die
File-Editor-API antwortet außerhalb einer Browser-Session mit HTTP 420
„Policy not fulfilled". Die zwei Zeilen sind deshalb von Hand in
`configuration.yaml` einzutragen:

```yaml
homeassistant:
  customize:
    sensor.pv_leistung_momentan:
      state_class: measurement
      device_class: power
```

Danach „Kernkonfiguration neu laden" (oder `ha_reload_core(target="core")`).
Die Statistik beginnt ab diesem Zeitpunkt; rückwirkend entsteht nichts. Sobald
sie läuft, kann die Roh-Karte auf die Akkuspannung allein zusammengestrichen
werden.

**Warum überall `statistics-graph`:** Der `history-graph` zeichnet den
Rohverlauf und wird dadurch stufig und verrauscht. `statistics-graph` zeichnet
aggregierte Werte als weiche Kurve, mit `min`/`mean`/`max` zusätzlich als Band —
das ist der Stil, der gewünscht war. Für 48-h-Karten steht deshalb
`period: hour` mit `days_to_show: 2`, für die Langzeitkarten `period: day`.

**Nur drei Karten sind noch `history-graph`** — Taupunkt/Gefühlt, Barometer und
PV-Leistung/Akkuspannung. Diesen Sensoren fehlt `state_class`, sie haben also
gar keine Langzeitstatistik, aus der ein `statistics-graph` zeichnen könnte.

## Was dabei zu beachten ist

- **`statistics-graph` braucht `state_class`.** Geprüft über
  `ha_get_history(source="statistics")`: **ohne** Langzeitstatistik sind
  `sensor.weatherman_barometer`, `sensor.weatherman_taupunkt`,
  `sensor.weatherman_regen_mm_heute`, `sensor.pv_leistung_momentan`,
  `sensor.pv_akku_ladung`, `sensor.pv_leistung_heute`,
  `sensor.weatherman_sonnenstunden_heute` und
  `sensor.bienenwaage_temperaturkorrektur`. Für diese Werte steht deshalb ein
  `history-graph` in der Karte — der zieht aus dem Recorder und braucht kein
  `state_class`, reicht aber nur bis zur Purge-Grenze (~10 Tage) zurück.
- **Wer eine dieser Karten auf `statistics-graph` oder einen längeren Zeitraum
  umstellen will, muss zuerst dem Sensor ein `state_class` geben** — sonst
  bleibt der Graph leer, ohne Fehlermeldung. Die Statistik beginnt dann
  allerdings erst ab dem Zeitpunkt der Umstellung; rückwirkend entsteht nichts.
- **`statistics-graph` ignoriert die pro-Entity gesetzte `color`, sobald mehr
  als ein `stat_type` gezeigt wird** — Min/Mittel/Max kommen immer in der
  Blau-Palette. Nur bei `stat_types: ['mean']` schlägt die eigene Farbe durch;
  deshalb sind die Raumtemperaturen farbig und die Bandkarten blau.
- Die Langzeitstatistik des Stockgewichts beginnt erst am 19.08.; das 90-Tage-
  Diagramm ist entsprechend links leer.

## Offene Punkte

1. **Abgleich mit dem Grafana-Original steht aus.** Falls dort andere Serien
   liefen, das JSON-Modell aus Grafana (Dashboard-Einstellungen → JSON Model)
   heranziehen und die Karten nachziehen.
2. **Grafana- und InfluxDB-Add-on laufen weiter.** Ob sie noch gebraucht
   werden, entscheidet der Abgleich aus Punkt 1.
3. `sensor.pv_leistung_momentan` stand beim Bau auf `unknown` — in der
   Energie-Karte fehlt die Serie dann.
4. Die Sensoraussetzer des WeatherMan (senkrechte Ausreißer auf 0 bzw. −20)
   stehen jetzt als Ausschläge im Min-Band von „Außen 48 h" und „Luftfeuchte
   48 h" und in „Taupunkt und Gefühlt 48 h". In Grafana waren sie vermutlich
   weggefiltert. Ein `filter`-Helfer (Outlier) wäre der Bordmittel-Weg dagegen.
5. „Gewicht 48 h" hat eine sehr enge y-Achse (~0,1 kg Spanne), weil das Gewicht
   im Fenster kaum schwankt — jedes Rauschen sieht dort dramatisch aus.

## Werkzeugnotizen

- Der Grafana-Add-on-Ingress ist von anderen Add-ons aus **nicht** abfragbar
  (403 von nginx), auch nicht mit gesetztem `X-Ingress-Path`. Direktport ist
  nicht gemappt. Panels lassen sich also nur aus einer Browser-Session heraus
  oder über das exportierte JSON-Modell lesen.
- `ha_get_dashboard_screenshot(dashboard_url_path=…, view_path=…)` rendert die
  nativen Graph-Karten zuverlässig, wenn `wait_ms` hoch genug steht (12 s);
  Ingress-Iframes bleiben darin leer.
