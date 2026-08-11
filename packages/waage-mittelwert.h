// =====================================================================
// Mittelung der Rohwerte über ein Messfenster - die Rechnung dazu
// =====================================================================
// Ein veröffentlichter Messwert ist seit dem 11.08.2026 nicht mehr der
// Momentanwert der Filterkette (~60 s Fenster), sondern der Mittelwert ALLER
// Rohwerte seit der letzten Veröffentlichung. Bei 6 h Messintervall sind das
// rund 4.300 Werte statt 12 - das Rauschen sinkt theoretisch um Wurzel(N),
// also etwa um den Faktor 19.
//
// WARUM EIN HEADER: Dieselbe Rechnung wird an mehreren Stellen gebraucht
// (Gewicht, Rohwert, Streuung, Temperaturmittel) und ist genau die Sorte
// Arithmetik, deren Fehler man auf dem Gerät nicht sieht - ein falsch
// gemittelter Rohwert sieht immer plausibel aus. Wie
// packages/waage-temperatur.h nimmt sie deshalb reine Zahlen, kennt weder
// id() noch Globals und ist mit g++ durchrechenbar
// (tests/waage-temperatur-test.cpp, Punkt 9 und 10).
//
// EINGEBUNDEN über "esphome: includes:" in waage-basis.yaml. Der Pfad dort
// lautet "packages/waage-mittelwert.h" und löst gegen das Verzeichnis der
// GEFLASHTEN Stock-Datei auf, also gegen /config/esphome/.
//
// WARUM DER BEZUGSWERT: Summiert wird nicht x, sondern (x - bezug), wobei
// bezug der erste Wert des Fensters ist. Der HX711 ist 24 bit breit und
// liefert bis zu ±8.388.608 counts. Bei 7 Tagen Messintervall kämen rund
// 120.000 Werte zusammen; die direkte Quadratsumme läge dann bei ~1e18 und
// damit an der Grenze dessen, was ein double (53 bit Mantisse) noch sauber
// aufaddiert - ausgerechnet die Streuung, die als Differenz zweier großer
// Zahlen entsteht, würde darin untergehen. Mit dem Bezugswert sind die
// Summanden so groß wie das Rauschen selbst, also klein, und die Frage stellt
// sich nicht mehr.
// =====================================================================
#pragma once

#include <cmath>
#include <cstdint>

/// Mittelwert eines Messfensters.
///
/// @param bezug    Wert, gegen den summiert wurde (erster Wert des Fensters).
///                 Für Größen ohne Bezugswert - z. B. die Temperatur, die nur
///                 zweistellig wird - einfach 0.0 übergeben.
/// @param summe    Summe aller (x - bezug)
/// @param anzahl   Anzahl der aufsummierten Werte
///
/// Ein leeres Fenster liefert NAN. Das ist die ehrliche Antwort: es gibt
/// nichts zu mitteln. Die aufrufende Seite entscheidet, ob sie stattdessen
/// den Momentanwert nimmt (Buttons) oder gar nichts veröffentlicht.
inline float waage_fenster_mittelwert(double bezug, double summe,
                                      uint32_t anzahl) {
  if (anzahl == 0) {
    return NAN;
  }
  return (float) (bezug + summe / (double) anzahl);
}

/// Streuung (Standardabweichung) der Werte innerhalb des Messfensters.
///
/// @param summe          Summe aller (x - bezug)
/// @param quadratsumme   Summe aller (x - bezug)²
/// @param anzahl         Anzahl der aufsummierten Werte
///
/// Der Bezugswert kürzt sich hier heraus und wird deshalb nicht gebraucht.
///
/// WOZU: Der gemittelte Rohwert allein verschweigt, wie unruhig das Signal
/// war - genau die Information, an der man Wackelkontakt, Wind und eine
/// zusammenbrechende Stromversorgung erkennt. Vor der Mittelung sah man das
/// direkt am springenden Rohwert; dieser Wert ersetzt das.
///
/// Gerechnet wird mit n-1 (Stichproben-Standardabweichung), weil das Fenster
/// eine Stichprobe des Rauschens ist und keine Vollerhebung. Unter zwei
/// Werten gibt es keine Streuung - dann NAN.
inline float waage_fenster_streuung(double summe, double quadratsumme,
                                    uint32_t anzahl) {
  if (anzahl < 2) {
    return NAN;
  }
  double n = (double) anzahl;
  double varianz = (quadratsumme - summe * summe / n) / (n - 1.0);
  // Bei einem völlig ruhigen Signal ist die Varianz rechnerisch null und kann
  // durch Rundung minimal negativ herauskommen - sqrt() lieferte dann NAN und
  // die Entity fiele grundlos aus.
  if (varianz < 0.0) {
    varianz = 0.0;
  }
  return (float) std::sqrt(varianz);
}
