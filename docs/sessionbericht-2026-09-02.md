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
| Außentemperatur | `history-graph` 48 h (Temperatur, Taupunkt, Gefühlt) · `statistics-graph` 30 Tage Min/Mittel/Max |
| Innentemperaturen | `history-graph` 48 h (Küche, Schlafzimmer, Nähzimmer, Flur unten, Bad unten) · `statistics-graph` 30 Tage Tagesmittel |
| Feuchte, Druck, Wind | je ein `history-graph` 48 h für rel. Feuchte, Barometer, Wind (Mittel + Spitze) |
| Bienenstockwaage | `history-graph` 48 h (Gewicht + Stocktemperatur) · `statistics-graph` 90 Tage Tagesmittel · Tile „Tagesbilanz" mit `trend-graph` über 7 Tage |

**Unteransicht `energie`:** `history-graph` 24 h (Hausverbrauch, PV-Leistung,
Nachteinspeisung, Akku-Ladeleistung), `history-graph` 48 h Akkuspannung,
`statistics-graph` 30 Tage Hausverbrauch (Mittel und Maximum). Zusätzlich
`back_path` auf `/lovelace/diagramme`.

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
- **Wer eine dieser Karten auf einen längeren Zeitraum stellen will, muss
  zuerst dem Sensor ein `state_class` geben** — sonst bleibt der Graph leer,
  ohne Fehlermeldung.
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
4. In „Außen 48 h" und „Luftfeuchte 48 h" stehen senkrechte Ausreißer
   (Sensoraussetzer des WeatherMan), die vorher in Grafana vermutlich
   weggefiltert waren.

## Werkzeugnotizen

- Der Grafana-Add-on-Ingress ist von anderen Add-ons aus **nicht** abfragbar
  (403 von nginx), auch nicht mit gesetztem `X-Ingress-Path`. Direktport ist
  nicht gemappt. Panels lassen sich also nur aus einer Browser-Session heraus
  oder über das exportierte JSON-Modell lesen.
- `ha_get_dashboard_screenshot(dashboard_url_path=…, view_path=…)` rendert die
  nativen Graph-Karten zuverlässig, wenn `wait_ms` hoch genug steht (12 s);
  Ingress-Iframes bleiben darin leer.
