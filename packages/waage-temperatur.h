// =====================================================================
// Temperaturkompensation der Wägezellen - die eine Formel
// =====================================================================
// Diese Datei enthält genau eine Rechnung, dafür an genau einer Stelle.
//
// WARUM ÜBERHAUPT EIN HEADER: Die Korrektur wird an drei Stellen gebraucht -
// beim Berechnen des Gewichts, beim Tarieren und im Diagnose-Sensor
// "Temperaturkorrektur". Würde die Formel dort dreimal ausgeschrieben, könnte
// sie auseinanderlaufen; die Waage würde dann gegen eine andere Rechnung
// tarieren, als sie anzeigt, und der Diagnosewert würde etwas anderes
// behaupten als beide. Das ist die Sorte Fehler, die man erst Wochen später
// an einer krummen Tagesbilanz bemerkt.
//
// EINGEBUNDEN wird die Datei über "esphome: includes:" in waage-basis.yaml.
// Der Pfad dort ist "packages/waage-temperatur.h" und löst - genau wie das
// !include des Packages selbst - gegen das Verzeichnis der GEFLASHTEN
// Stock-Datei auf, also gegen /config/esphome/. Beim Kopieren ins
// Add-on-Verzeichnis muss packages/ deshalb vollständig mit.
//
// BEWUSST OHNE id()-ZUGRIFFE: Die Funktion bekommt reine Zahlen und kennt
// weder Globals noch Sensoren. Nur so lässt sie sich ohne ESPHome mit g++
// übersetzen und durchrechnen (siehe docs/sessionbericht-2026-08-10.md) -
// und zwar genau der Code, der später auch auf dem ESP läuft.
// =====================================================================
#pragma once

#include <cmath>

/// Temperaturbedingter Anteil des angezeigten Gewichts, in kg.
///
/// Der Rückgabewert wird vom Bruttogewicht ABGEZOGEN:
///     kg -= waage_temperaturkorrektur(...)
///
/// @param temperatur         aktuelle Temperatur in °C
/// @param bezugstemperatur   Temperatur bei der Nullpunkt-Kalibrierung
///                           (Global calib_temp) in °C. Bei genau dieser
///                           Temperatur ist die Korrektur null.
/// @param koeffizient_g_pro_k  Drift der Waage in g/K. Positiv heißt: die
///                           Waage zeigt bei Wärme zu viel. 0 schaltet die
///                           Kompensation ab.
///
/// Fehlt eine der drei Angaben (NAN), wird NICHT kompensiert. Das ist die
/// sichere Richtung: ohne Bezugstemperatur ist jede Korrektur geraten, und
/// eine geratene Korrektur ist schlechter als gar keine.
inline float waage_temperaturkorrektur(float temperatur,
                                       float bezugstemperatur,
                                       float koeffizient_g_pro_k) {
  if (std::isnan(temperatur) || std::isnan(bezugstemperatur) ||
      std::isnan(koeffizient_g_pro_k)) {
    return 0.0f;
  }
  return (temperatur - bezugstemperatur) * koeffizient_g_pro_k / 1000.0f;
}
