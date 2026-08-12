// =====================================================================
// Plausibilitaetsgrenzen des veroeffentlichten Gewichts
// =====================================================================
// Ein Stock kann nicht weniger als nichts und nicht beliebig viel wiegen.
// Werte ausserhalb dieses Bereichs stammen nie aus dem Volk, sondern aus
// einem der drei bekannten Vorgaenge:
//
//   - eine halbe oder kaputte Kalibrierung (am 11.08.2026 real: Faktor
//     +753 statt -20.874, die Anzeige lief dabei auf -3.749 kg und
//     +2.299 kg),
//   - die Sekunden zwischen "Kalibrieren 0kg" und "Kalibrieren
//     Referenzgewicht", in denen der Span noch nicht stimmt,
//   - ein Tara auf ein aufliegendes Gewicht, das danach abgeraeumt wird.
//
// WARUM DAS UEBERHAUPT WEHTUT: Der Sensor "Gewicht" hat
// state_class: measurement. Jeder dieser Werte landet damit dauerhaft in
// der Langzeitstatistik von Home Assistant - anders als der Zustandsverlauf,
// den der Recorder nach ~10 Tagen aufraeumt. Ein einziger Ausreisser von
// -3.749 kg zieht das Stundenmittel dieser Stunde auf +70 kg und macht jede
// Tages- oder Wochenauswertung unbrauchbar; die Skalierung jeder
// statistics-graph-Karte gleich mit. Herausrechnen laesst sich das
// hinterher nur noch von Hand.
//
// Deshalb wird ein unplausibler Wert GAR NICHT veroeffentlicht statt
// gekappt: Ein gekappter Wert (150,0 kg) waere eine Behauptung ueber das
// Volk, die genauso falsch ist wie die 2.299 - nur unauffaelliger. Bleibt
// der Wert aus, behaelt die Entity ihren letzten guten Stand, und der
// Zaehler "Gewicht verworfen" sagt, dass etwas passiert ist.
//
// WIE packages/waage-temperatur.h und packages/waage-mittelwert.h: reine
// Zahlen, kein id(), damit die Regel mit g++ pruefbar bleibt
// (tests/waage-temperatur-test.cpp, Punkt 11).
// =====================================================================
#pragma once

#include <cmath>

/// Darf dieses Gewicht nach Home Assistant?
///
/// @param kg           der fertig gerechnete Wert (nach Kompensation, Tara
///                     und Rundung - genau das, was veroeffentlicht wuerde)
/// @param untergrenze  kleinster plausibler Wert in kg (einschliesslich)
/// @param obergrenze   groesster plausibler Wert in kg (einschliesslich)
///
/// NAN ist nie plausibel - dafuer gibt es nichts zu veroeffentlichen.
///
/// Ist die Obergrenze nicht groesser als die Untergrenze, ist die Pruefung
/// ABGESCHALTET und alles ausser NAN kommt durch. Das ist der Ausweg fuer
/// einen Stock, der die Grenzen nicht will (z. B. eine Waage, die bewusst
/// negativ anzeigen soll, weil ohne Beute tariert wurde), ohne dass jemand
/// diesen Code anfassen muss - es reicht, in der Stock-Datei
/// plausibel_kg_max <= plausibel_kg_min zu setzen.
inline bool waage_gewicht_plausibel(float kg, float untergrenze,
                                    float obergrenze) {
  if (std::isnan(kg)) {
    return false;
  }
  // Bewusst so herum geschrieben: sind die Grenzen selbst NAN, ist der
  // Vergleich falsch und die Pruefung damit aus - nicht andersherum, sonst
  // wuerde eine kaputte Konfiguration jeden Messwert verschlucken.
  if (!(obergrenze > untergrenze)) {
    return true;
  }
  return kg >= untergrenze && kg <= obergrenze;
}
