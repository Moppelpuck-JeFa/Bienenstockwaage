// Prueft die Temperaturkompensation und die Messfenster-Mittelung der
// Bienenstockwaage.
//
//   cd tests
//   g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
//
// Eingebunden werden die ECHTEN Header-Dateien aus packages/ - geprueft wird
// also genau der Code, der auf dem ESP laeuft, keine Abschrift. Die
// Lambda-Koerper (Gewicht, Tara, Taktgeber, Messfenster) sind dagegen
// nachgebaut; sie stehen in packages/waage-basis.yaml und lassen sich nicht
// einbinden. Wer dort etwas aendert, muss den Nachbau hier mitziehen.
//
// Warum ueberhaupt: Ein vollstaendiger "esphome compile" ist in dieser
// Umgebung nicht moeglich (PlatformIO-Registry gesperrt). Dieser Test ist der
// Ersatz dafuer und deckt genau die Rechnung ab, deren Fehler man auf dem
// Geraet nicht sehen wuerde - das Ergebnis saehe immer plausibel aus.
//
// Punkt 7 rechnet zusaetzlich die echte Messreihe vom 03.-10.08.2026 durch.
// Die dafuer noetige paare.csv (Spalten: Temperatur, Rohwert) liegt nicht im
// Repo; fehlt sie, wird der Punkt uebersprungen und der Rest laeuft normal.

#include "../packages/waage-temperatur.h"
#include "../packages/waage-mittelwert.h"
#include "../packages/waage-grenzen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <string>
#include <vector>
#include <cstdint>

// --- Zustand der Waage, wie er auf dem Geraet in Globals liegt ---
// Werte von waage-eg, Stand 10.08.2026 (aus Home Assistant ausgelesen).
//
// mittel_temperatur/raw sind seit dem 11.08.2026 die Mittelwerte des
// abgelaufenen Messfensters, nicht mehr Momentanwerte. Fuer die Rechnung
// aendert das nichts - es gehen zwei Zahlen hinein wie vorher -, wohl aber
// fuer die Genauigkeit; das prueft Punkt 10.
struct Waage {
  float calib_raw_zero = 25943.166015625f;   // leer, 03.08. 16:36
  float calib_raw_ref = -20291.0f;           // 2,218 kg, 03.08. 16:41
  float calib_kg_ref = 2.218f;
  float calib_temp = 24.5625f;               // "Kalibriert bei"
  float tare_offset = 0.0f;
  float mittel_temperatur = NAN;
  float koeffizient = 32.5f;                 // g/K
};

// 1:1 der Lambda-Koerper von sensor "waage_gewicht", aber VOR dem Runden.
// Der Nachbau ist hier aufgeteilt, weil die 0,1-kg-Rundung jeden Effekt
// unterhalb von 50 g verschluckt - und genau darum geht es bei der Mittelung.
static float gewicht_ungerundet(const Waage &w, float raw) {
  if (std::isnan(raw)) return NAN;
  float span = w.calib_raw_ref - w.calib_raw_zero;
  float kg = 0.0f;
  if (span != 0.0f) {
    kg = (raw - w.calib_raw_zero) * w.calib_kg_ref / span;
  }
  kg -= waage_temperaturkorrektur(w.mittel_temperatur, w.calib_temp,
                                  w.koeffizient);
  kg -= w.tare_offset;
  return kg;
}

// 1:1 der Lambda-Koerper von sensor "waage_gewicht", aber OHNE das
// Plausibilitaetsfenster - das prueft veroeffentlichtes_gewicht() unten.
// Getrennt, weil die Punkte 2 bis 10 den Rechenweg pruefen und nicht die
// Frage, ob das Ergebnis noch nach HA darf.
static float gewicht(const Waage &w, float raw) {
  float kg = gewicht_ungerundet(w, raw);
  if (std::isnan(kg)) return NAN;
  float gerundet = std::round(kg * 10.0f) / 10.0f;
  if (std::fabs(gerundet) < 0.001f) gerundet = 0.0f;
  return gerundet;
}

// 1:1 der Lambda-Koerper von sensor "waage_gewicht", vollstaendig - also
// inklusive Plausibilitaetsfenster. NAN heisst hier "return {}", die Entity
// behaelt in HA ihren letzten Wert.
static float veroeffentlichtes_gewicht(const Waage &w, float raw,
                                       float untergrenze, float obergrenze) {
  float gerundet = gewicht(w, raw);
  if (!waage_gewicht_plausibel(gerundet, untergrenze, obergrenze)) return NAN;
  return gerundet;
}

// --- Nachbau des Messfensters aus waage-basis.yaml ---
// Entspricht dem on_value des HX711 plus dem Skript "messfenster_abschliessen".
struct Messfenster {
  double bezug = 0.0;
  double summe = 0.0;
  double quadratsumme = 0.0;
  uint32_t anzahl = 0;

  void hinzufuegen(float x) {
    if (std::isnan(x)) return;
    if (anzahl == 0) bezug = x;
    double abweichung = (double) x - bezug;
    summe += abweichung;
    quadratsumme += abweichung * abweichung;
    anzahl++;
  }
  float mittelwert() const {
    return waage_fenster_mittelwert(bezug, summe, anzahl);
  }
  float streuung() const {
    return waage_fenster_streuung(summe, quadratsumme, anzahl);
  }
};

// --- Nachbau der Filterkette des HX711 ---
// median(5, send_every 5) -> sliding_window_moving_average(12, send_every 1).
// Wichtig fuer Punkt 10: die Ausgaben ueberlappen sich (jeder Medianwert steckt
// in 12 aufeinanderfolgenden Ausgaben). Wer den Genauigkeitsgewinn ohne diese
// Korrelation abschaetzt, rechnet ihn zu gross.
struct Filterkette {
  std::vector<float> median_puffer;
  std::vector<float> fenster;
  // Nimmt einen Sekundenwert an; liefert true, wenn ein gefilterter Wert
  // herausfaellt (alle 5 s, sobald das Fenster gefuellt ist).
  bool schritt(float roh, float &ausgabe) {
    median_puffer.push_back(roh);
    if (median_puffer.size() < 5) return false;
    std::vector<float> s = median_puffer;
    std::sort(s.begin(), s.end());
    float med = s[2];
    median_puffer.clear();
    fenster.push_back(med);
    if (fenster.size() > 12) fenster.erase(fenster.begin());
    double summe = 0.0;
    for (float v : fenster) summe += v;
    ausgabe = (float) (summe / fenster.size());
    return true;
  }
};

// 1:1 der Lambda-Koerper des Tara-Buttons
static void tara(Waage &w, float raw) {
  if (std::isnan(raw)) return;
  float span = w.calib_raw_ref - w.calib_raw_zero;
  if (span == 0.0f) return;
  float brutto = (raw - w.calib_raw_zero) * w.calib_kg_ref / span;
  float korrektur = waage_temperaturkorrektur(w.mittel_temperatur,
                                              w.calib_temp, w.koeffizient);
  w.tare_offset = brutto - korrektur;
}

// Rohwert, den die Waage bei gegebener echter Last und Temperatur liefert.
// Die Drift wird dabei so eingerechnet, wie sie gemessen wurde.
static float roh_fuer(const Waage &w, float echte_kg, float temp_c) {
  float counts_pro_kg = (w.calib_raw_ref - w.calib_raw_zero) / w.calib_kg_ref;
  float scheinbar_kg = echte_kg + (temp_c - w.calib_temp) * w.koeffizient / 1000.0f;
  return w.calib_raw_zero + scheinbar_kg * counts_pro_kg;
}

static int fehler = 0;
static int geprueft = 0;

static void pruefe(const char *was, double ist, double soll, double toleranz) {
  geprueft++;
  bool ok = (std::isnan(soll) && std::isnan(ist)) ||
            (std::fabs(ist - soll) <= toleranz);
  if (!ok) {
    fehler++;
    std::printf("  FEHLER  %-58s ist %.4f, soll %.4f\n", was, ist, soll);
  }
}

int main() {
  std::printf("== 1. Die Formel selbst ==\n");
  pruefe("Bezugstemperatur -> keine Korrektur",
         waage_temperaturkorrektur(24.5625f, 24.5625f, 32.5f), 0.0f, 1e-9f);
  pruefe("7 K kaelter -> -0,229 kg",
         waage_temperaturkorrektur(17.5f, 24.5625f, 32.5f), -0.22953f, 1e-4f);
  pruefe("2,4 K waermer -> +0,077 kg",
         waage_temperaturkorrektur(26.9f, 24.5625f, 32.5f), 0.07597f, 1e-4f);
  pruefe("Koeffizient 0 -> keine Korrektur",
         waage_temperaturkorrektur(5.0f, 24.5625f, 0.0f), 0.0f, 1e-9f);
  pruefe("negativer Koeffizient kehrt das Vorzeichen um",
         waage_temperaturkorrektur(34.5625f, 24.5625f, -32.5f), -0.325f, 1e-4f);
  pruefe("Temperatur fehlt -> keine Korrektur",
         waage_temperaturkorrektur(NAN, 24.5625f, 32.5f), 0.0f, 1e-9f);
  pruefe("Bezugstemperatur fehlt -> keine Korrektur",
         waage_temperaturkorrektur(17.5f, NAN, 32.5f), 0.0f, 1e-9f);
  pruefe("Koeffizient fehlt -> keine Korrektur",
         waage_temperaturkorrektur(17.5f, 24.5625f, NAN), 0.0f, 1e-9f);
  std::printf("   %d Pruefungen\n", geprueft);

  std::printf("\n== 2. Anzeige bei konstanter Last ueber den Temperaturhub ==\n");
  {
    Waage w;
    // 30 kg liegen auf, die Temperatur laeuft von 15 auf 35 C
    std::printf("   Temp    unkompensiert   kompensiert\n");
    float min_k = 1e9f, max_k = -1e9f, min_u = 1e9f, max_u = -1e9f;
    for (float t = 15.0f; t <= 35.001f; t += 2.5f) {
      w.mittel_temperatur = t;
      float raw = roh_fuer(w, 30.0f, t);
      Waage ohne = w;
      ohne.koeffizient = 0.0f;
      float u = gewicht(ohne, raw);
      float k = gewicht(w, raw);
      std::printf("   %5.1f C   %8.1f kg   %9.1f kg\n", t, u, k);
      min_k = std::fmin(min_k, k); max_k = std::fmax(max_k, k);
      min_u = std::fmin(min_u, u); max_u = std::fmax(max_u, u);
      pruefe("kompensierte Anzeige bleibt bei 30,0 kg", k, 30.0f, 0.051f);
    }
    std::printf("   Spannweite ohne Kompensation %.0f g, mit %.0f g\n",
                (max_u - min_u) * 1000.0f, (max_k - min_k) * 1000.0f);
  }

  std::printf("\n== 3. Tara haelt ueber einen Temperaturwechsel ==\n");
  {
    Waage w;
    w.mittel_temperatur = 18.0f;
    tara(w, roh_fuer(w, 30.0f, 18.0f));       // morgens tarieren
    pruefe("direkt nach dem Tarieren 0,0 kg",
           gewicht(w, roh_fuer(w, 30.0f, 18.0f)), 0.0f, 1e-6f);
    w.mittel_temperatur = 32.0f;               // 14 K waermer, gleiche Last
    float abw = gewicht(w, roh_fuer(w, 30.0f, 32.0f));
    std::printf("   nach +14 K: %.1f kg (ohne Kompensation waeren es %.3f kg)\n",
                abw, 14.0f * 32.5f / 1000.0f);
    pruefe("Tara haelt ueber 14 K", abw, 0.0f, 1e-6f);
    // echte Laständerung wird weiterhin sauber angezeigt
    pruefe("2,5 kg Zuwachs bei 32 C",
           gewicht(w, roh_fuer(w, 32.5f, 32.0f)), 2.5f, 0.051f);
  }

  std::printf("\n== 4. Rueckwaertskompatibilitaet ==\n");
  {
    // Bei ausgeschalteter Kompensation muss exakt das Alte herauskommen.
    Waage w;
    w.koeffizient = 0.0f;
    w.mittel_temperatur = 12.3f;
    for (float kg = 0.0f; kg <= 200.0f; kg += 0.1f) {
      float raw = w.calib_raw_zero + kg * (w.calib_raw_ref - w.calib_raw_zero) / w.calib_kg_ref;
      pruefe("Koeffizient 0 rechnet wie bisher", gewicht(w, raw),
             std::round(kg * 10.0f) / 10.0f, 0.001f);
    }
    // Und bei genau der Kalibriertemperatur ebenfalls, trotz aktiver Kompensation
    Waage v;
    v.mittel_temperatur = v.calib_temp;
    for (float kg = 0.0f; kg <= 200.0f; kg += 0.1f) {
      float raw = v.calib_raw_zero + kg * (v.calib_raw_ref - v.calib_raw_zero) / v.calib_kg_ref;
      pruefe("bei Bezugstemperatur wie bisher", gewicht(v, raw),
             std::round(kg * 10.0f) / 10.0f, 0.001f);
    }
    std::printf("   2x 2001 Stufen von 0 bis 200 kg geprueft\n");
  }

  std::printf("\n== 5. Ausfall des DS18B20 ==\n");
  {
    Waage w;
    w.mittel_temperatur = 20.0f;
    float raw = roh_fuer(w, 30.0f, 20.0f);
    float mit = gewicht(w, raw);
    // Der DS18B20 setzt aus: mittel_temperatur bleibt stehen (on_value
    // schreibt NAN nicht fort), die Anzeige aendert sich also nicht
    float nach_aussetzer = gewicht(w, raw);
    pruefe("einzelner Aussetzer aendert die Anzeige nicht", nach_aussetzer, mit, 1e-6f);
    // Nie eine Temperatur bekommen (frisch gebootet, Sensor defekt)
    Waage n;
    n.mittel_temperatur = NAN;
    pruefe("ohne jede Temperatur wird unkompensiert angezeigt",
           gewicht(n, roh_fuer(n, 30.0f, 24.5625f)), 30.0f, 0.051f);
  }

  std::printf("\n== 6. Nie kalibrierter Nullpunkt ==\n");
  {
    Waage w;
    w.calib_temp = NAN;               // "Kalibriert bei" leer
    w.mittel_temperatur = 10.0f;
    pruefe("ohne Bezugspunkt keine Korrektur",
           gewicht(w, roh_fuer(Waage{}, 30.0f, 24.5625f)), 30.0f, 0.051f);
  }

  std::printf("\n== 7. Die echte Messreihe (164 Punkte, 03.-10.08.2026) ==\n");
  {
    std::ifstream f("paare.csv");
    if (!f) {
      std::printf("   paare.csv fehlt - uebersprungen\n");
    } else {
      Waage w;
      Waage ohne = w;
      ohne.koeffizient = 0.0f;
      std::vector<float> mit_v, ohne_v;
      std::string zeile;
      while (std::getline(f, zeile)) {
        auto k = zeile.find(',');
        if (k == std::string::npos) continue;
        float T = std::stof(zeile.substr(0, k));
        float raw = std::stof(zeile.substr(k + 1));
        w.mittel_temperatur = T;
        ohne.mittel_temperatur = T;
        mit_v.push_back(gewicht(w, raw));
        ohne_v.push_back(gewicht(ohne, raw));
      }
      auto spanne = [](const std::vector<float> &v) {
        float lo = v[0], hi = v[0];
        for (float x : v) { lo = std::fmin(lo, x); hi = std::fmax(hi, x); }
        return (hi - lo) * 1000.0f;
      };
      std::printf("   %zu Messpunkte eingelesen\n", mit_v.size());
      std::printf("   Anzeigespanne ohne Kompensation: %.0f g\n", spanne(ohne_v));
      std::printf("   Anzeigespanne mit  Kompensation: %.0f g\n", spanne(mit_v));
      pruefe("Kompensation verkleinert die Spanne deutlich",
             spanne(mit_v) < spanne(ohne_v) * 0.6f ? 1.0f : 0.0f, 1.0f, 0.5f);
    }
  }

  std::printf("\n== 8. Warten auf die erste Temperatur nach dem Boot ==\n");
  {
    // Nachbau der neuen Sperre im interval:-Block, minutenweise.
    // temp_ab = Minute, in der der DS18B20 den ersten Wert liefert
    // (0 = Sensor defekt, kommt nie).
    auto erste_messung = [](int temp_ab, bool kompensation_aktiv) {
      uint8_t warte = 0;
      float mittel_temperatur = NAN;
      uint32_t minuten_seit_messung = 0;
      bool noch_kein_wert = true;
      for (int minute = 1; minute <= 30; minute++) {
        if (temp_ab != 0 && minute >= temp_ab) mittel_temperatur = 20.0f;
        if (std::isnan(mittel_temperatur) && warte < 3 && kompensation_aktiv) {
          warte++;
          continue;
        }
        minuten_seit_messung++;
        if (minuten_seit_messung >= 360 || noch_kein_wert) return minute;
      }
      return -1;
    };
    pruefe("Temperatur da: erster Wert nach 1 min", erste_messung(1, true), 1, 0);
    pruefe("Temperatur ab Minute 2: erster Wert nach 2 min",
           erste_messung(2, true), 2, 0);
    pruefe("Temperatur ab Minute 3: erster Wert nach 3 min",
           erste_messung(3, true), 3, 0);
    pruefe("DS18B20 defekt: erster Wert nach 4 min (Notausstieg)",
           erste_messung(0, true), 4, 0);
    pruefe("Kompensation aus: kein Warten, erster Wert nach 1 min",
           erste_messung(0, false), 1, 0);
    std::printf("   5 Pruefungen\n");
  }

  std::printf("\n== 9. Die Mittelwert-Rechnung selbst ==\n");
  {
    pruefe("leeres Fenster -> NAN",
           waage_fenster_mittelwert(0.0, 0.0, 0), NAN, 0.0);
    pruefe("ein Wert -> genau dieser Wert",
           waage_fenster_mittelwert(1234.5, 0.0, 1), 1234.5f, 1e-6f);
    pruefe("unter zwei Werten keine Streuung",
           waage_fenster_streuung(0.0, 0.0, 1), NAN, 0.0);
    pruefe("voellig ruhiges Signal -> Streuung 0, nicht NAN",
           waage_fenster_streuung(0.0, 0.0, 100), 0.0f, 1e-9f);

    // 1..9 gegen einen krummen Bezugswert. Mittelwert 5, Streuung (n-1) exakt
    // sqrt(7.5) = 2,7386.
    Messfenster f;
    for (int i = 1; i <= 9; i++) f.hinzufuegen((float) i);
    pruefe("1..9 -> Mittelwert 5", f.mittelwert(), 5.0f, 1e-5f);
    pruefe("1..9 -> Streuung sqrt(7,5)", f.streuung(), std::sqrt(7.5f), 1e-4f);

    // Derselbe Datensatz, aber um 8 Mio counts verschoben - der Bereich, in
    // dem der HX711 wirklich arbeitet. Genau hier scheitert die naive
    // Quadratsumme; mit dem Bezugswert darf sich nichts aendern.
    Messfenster g;
    const double weit = 8000000.0;
    for (int i = 1; i <= 9; i++) g.hinzufuegen((float) (weit + i));
    pruefe("um 8 Mio verschoben -> Mittelwert stimmt",
           g.mittelwert(), (float) (weit + 5.0), 1.0f);
    pruefe("um 8 Mio verschoben -> Streuung unveraendert",
           g.streuung(), std::sqrt(7.5f), 1e-3f);

    // Und ein 7-Tage-Fenster in voller Laenge (120.960 Werte im 5-s-Takt),
    // hoch im Zahlenbereich und mit realistischem Rauschen. Das ist der
    // Grenzfall, fuer den der Bezugswert ueberhaupt eingebaut ist.
    Messfenster lang;
    std::mt19937 rng(1);
    std::normal_distribution<double> rauschen(0.0, 300.0);
    double soll_summe = 0.0;
    const uint32_t n_lang = 7 * 24 * 12 * 60;
    for (uint32_t i = 0; i < n_lang; i++) {
      float wert = (float) (weit + rauschen(rng));
      lang.hinzufuegen(wert);
      soll_summe += (double) wert - weit;
    }
    pruefe("7-Tage-Fenster: Mittelwert bis auf 0,5 counts genau",
           lang.mittelwert() - (float) weit,
           (float) (soll_summe / n_lang), 0.5f);
    pruefe("7-Tage-Fenster: Streuung trifft die 300 counts",
           lang.streuung(), 300.0f, 5.0f);
    std::printf("   %u Werte bei 8 Mio counts: Mittel %+.2f, Streuung %.1f\n",
                (unsigned) n_lang, lang.mittelwert() - (float) weit,
                lang.streuung());
  }

  std::printf("\n== 10. Was die Mittelung bringt ==\n");
  {
    // Simulation eines vollen Messintervalls: konstante Last, Temperaturrampe,
    // HX711-Rauschen im Sekundentakt durch die echte Filterkette.
    //
    // Verglichen werden die beiden Verfahren:
    //   alt  - der letzte gefilterte Wert des Intervalls (~60 s Mittel),
    //          kompensiert mit der Temperatur des Sendezeitpunkts
    //   neu  - Mittel ALLER gefilterten Werte des Intervalls,
    //          kompensiert mit der mittleren Temperatur desselben Fensters
    //
    // 300 counts Rauschen je Sekundenwert entsprechen an waage-eg rund 14 g
    // (bei -20.874 counts/kg) und liegen damit in der Groessenordnung, die die
    // Messreihe zeigt.
    const float last_kg = 30.0f;
    const double sigma_counts = 300.0;
    const int laeufe = 100;

    auto simuliere = [&](int minuten, double &abw_alt, double &abw_neu,
                         double &abw_neu_falsche_temp, double &streuung_mittel) {
      std::mt19937 rng(20260811);
      std::normal_distribution<double> rauschen(0.0, sigma_counts);
      double q_alt = 0.0, q_neu = 0.0, q_falsch = 0.0, summe_streuung = 0.0;
      for (int lauf = 0; lauf < laeufe; lauf++) {
        Waage w;
        Filterkette kette;
        Messfenster fenster;
        double temp_summe = 0.0;
        uint32_t temp_anzahl = 0;
        float letzte_ausgabe = NAN, temp_jetzt = NAN;
        // 60 s Vorlauf: auf dem Geraet laeuft die Filterkette durchgehend, ihr
        // gleitendes Fenster ist beim Start eines Messintervalls also laengst
        // gefuellt. Ohne diesen Vorlauf steckten die ersten, noch halb
        // gefuellten Ausgaben im Fenster und das kurze Intervall saehe
        // schlechter aus, als es ist.
        for (int sekunde = -60; sekunde < minuten * 60; sekunde++) {
          // Temperaturrampe mit fester RATE (1 K/h, so wie ein Sommervormittag
          // verlaeuft) - nicht mit festem Gesamthub. Sonst haenge der
          // Temperaturfehler an der Intervalllaenge, und die Spalten waeren
          // nicht mehr vergleichbar.
          temp_jetzt = 18.0f + (float) sekunde / 3600.0f;
          float roh = roh_fuer(w, last_kg, temp_jetzt) +
                      (float) rauschen(rng);
          float ausgabe;
          if (kette.schritt(roh, ausgabe)) {
            letzte_ausgabe = ausgabe;
            if (sekunde >= 0) fenster.hinzufuegen(ausgabe);
          }
          if (sekunde >= 0 && sekunde % 60 == 0) {  // DS18B20 im 60-s-Takt
            temp_summe += temp_jetzt;
            temp_anzahl++;
          }
        }
        float temp_mittel =
            waage_fenster_mittelwert(0.0, temp_summe, temp_anzahl);

        Waage alt = w;
        alt.mittel_temperatur = temp_jetzt;         // Momentanwert
        double e_alt = gewicht_ungerundet(alt, letzte_ausgabe) - last_kg;

        Waage neu = w;
        neu.mittel_temperatur = temp_mittel;        // Fenstermittel
        double e_neu = gewicht_ungerundet(neu, fenster.mittelwert()) - last_kg;

        // Die verworfene Variante: gemittelter Rohwert, aber mit der
        // Temperatur des Sendezeitpunkts korrigiert.
        Waage falsch = w;
        falsch.mittel_temperatur = temp_jetzt;
        double e_falsch =
            gewicht_ungerundet(falsch, fenster.mittelwert()) - last_kg;

        q_alt += e_alt * e_alt;
        q_neu += e_neu * e_neu;
        q_falsch += e_falsch * e_falsch;
        summe_streuung += fenster.streuung();
      }
      abw_alt = std::sqrt(q_alt / laeufe) * 1000.0;
      abw_neu = std::sqrt(q_neu / laeufe) * 1000.0;
      abw_neu_falsche_temp = std::sqrt(q_falsch / laeufe) * 1000.0;
      streuung_mittel = summe_streuung / laeufe;
    };

    std::printf("   %d Laeufe je Intervall, %.0f counts Rauschen je Sekunde\n",
                laeufe, sigma_counts);
    std::printf("   Intervall   alt (Momentanwert)   neu (Fenstermittel)"
                "   neu mit Momentan-Temp\n");
    for (int minuten : {1, 15, 60, 360}) {
      double alt, neu, falsch, streuung;
      simuliere(minuten, alt, neu, falsch, streuung);
      std::printf("   %4d min      %8.1f g            %8.1f g"
                  "            %8.1f g\n", minuten, alt, neu, falsch);
      if (minuten >= 15) {
        pruefe("Mittelung ist besser als der Momentanwert",
               neu < alt ? 1.0 : 0.0, 1.0, 0.5);
        pruefe("mittlere Temperatur ist besser als die Momentan-Temperatur",
               neu < falsch ? 1.0 : 0.0, 1.0, 0.5);
      }
      if (minuten == 360) {
        // Der Kern der Aenderung: bei 6 h muss der Rauschanteil deutlich
        // unter dem des Momentanwerts liegen. Die Schranke ist absichtlich
        // grosszuegig (Faktor 3 statt der theoretischen 19) - die
        // ueberlappenden Fenster der Filterkette korrelieren die Werte, und
        // ein zu scharfer Grenzwert waere ein falsches Versprechen.
        pruefe("bei 6 h mindestens dreimal genauer",
               neu * 3.0 < alt ? 1.0 : 0.0, 1.0, 0.5);
        std::printf("   6-h-Fenster: Streuung der Rohwerte %.0f counts "
                    "(~%.0f g)\n", streuung, streuung / 20.874);
      }
    }
  }

  std::printf("\n== 11. Plausibilitaetsfenster des Gewichts ==\n");
  {
    // Die Grenzen aus den substitutions von waage-basis.yaml.
    const float unten = -1.0f, oben = 150.0f;

    // --- Die Regel selbst ---
    pruefe("-1,0 kg ist plausibel (Grenze einschliesslich)",
           waage_gewicht_plausibel(-1.0f, unten, oben), 1.0, 0.5);
    pruefe("150,0 kg ist plausibel (Grenze einschliesslich)",
           waage_gewicht_plausibel(150.0f, unten, oben), 1.0, 0.5);
    pruefe("0,0 kg ist plausibel",
           waage_gewicht_plausibel(0.0f, unten, oben), 1.0, 0.5);
    pruefe("35,8 kg (Istwert waage-eg) ist plausibel",
           waage_gewicht_plausibel(35.8f, unten, oben), 1.0, 0.5);
    // Das Rauschen einer frisch tarierten Waage MUSS durchkommen. Eine
    // Grenze bei exakt 0 hat genau hier abgeschnitten und damit den
    // Mittelwert nach oben gezogen - unsichtbar in der Statistik.
    for (float nahe_null : {-0.1f, -0.2f, -0.5f, -0.9f}) {
      pruefe("Rauschen um die Null nach dem Tara kommt durch",
             waage_gewicht_plausibel(nahe_null, unten, oben), 1.0, 0.5);
    }
    pruefe("-1,1 kg faellt heraus",
           waage_gewicht_plausibel(-1.1f, unten, oben), 0.0, 0.5);
    pruefe("150,1 kg faellt heraus",
           waage_gewicht_plausibel(150.1f, unten, oben), 0.0, 0.5);
    pruefe("NAN faellt heraus",
           waage_gewicht_plausibel(NAN, unten, oben), 0.0, 0.5);

    // Die vier Werte, die am 11.08.2026 um 20:27-20:33 wirklich nach HA
    // gingen und das Stundenmittel dieser Stunde auf +70,8 kg gezogen haben.
    // Genau dafuer gibt es dieses Fenster.
    for (float ausreisser : {2299.4f, 1035.3f, -3749.7f, -48.6f}) {
      pruefe("realer Ausreisser vom 11.08. faellt heraus",
             waage_gewicht_plausibel(ausreisser, unten, oben), 0.0, 0.5);
    }

    // --- Abschalten der Pruefung ---
    pruefe("Obergrenze <= Untergrenze schaltet die Pruefung ab",
           waage_gewicht_plausibel(-3749.7f, 0.0f, 0.0f), 1.0, 0.5);
    pruefe("Grenzen als NAN schalten die Pruefung ab",
           waage_gewicht_plausibel(-3749.7f, NAN, NAN), 1.0, 0.5);
    pruefe("abgeschaltet heisst trotzdem: kein NAN nach HA",
           waage_gewicht_plausibel(NAN, NAN, NAN), 0.0, 0.5);

    // --- Im Zusammenspiel mit der Anzeige ---
    {
      Waage w;
      w.mittel_temperatur = w.calib_temp;   // Korrektur = 0, stoert hier nur

      // Normalbetrieb: was der Stock ueber den Temperaturhub wiegt, kommt
      // vollstaendig durch.
      for (float t = 15.0f; t <= 35.001f; t += 5.0f) {
        Waage v = w;
        v.mittel_temperatur = t;
        float raw = roh_fuer(v, 30.0f, t);
        pruefe("30 kg gehen bei jeder Temperatur nach HA",
               veroeffentlichtes_gewicht(v, raw, unten, oben), 30.0f, 0.051f);
      }

      // Die tarierte Waage: was sie um die Null herum anzeigt, geht
      // vollstaendig nach HA. -0,04 kg rundet ohnehin schon in der Anzeige
      // auf 0,0; ab -0,05 kg erscheint der negative Wert und wird jetzt
      // auch veroeffentlicht.
      pruefe("-0,04 kg wird zu 0,0 und kommt durch",
             veroeffentlichtes_gewicht(w, roh_fuer(w, -0.04f, w.calib_temp),
                                       unten, oben), 0.0f, 1e-6f);
      pruefe("-0,06 kg kommt als -0,1 kg durch",
             veroeffentlichtes_gewicht(w, roh_fuer(w, -0.06f, w.calib_temp),
                                       unten, oben), -0.1f, 1e-6f);
      pruefe("-0,95 kg kommt noch durch",
             veroeffentlichtes_gewicht(w, roh_fuer(w, -0.95f, w.calib_temp),
                                       unten, oben), -1.0f, 1e-6f);
      pruefe("-1,4 kg faellt aus",
             veroeffentlichtes_gewicht(w, roh_fuer(w, -1.4f, w.calib_temp),
                                       unten, oben), NAN, 1e-6f);
      // Zum Vergleich die frueher gesetzte Grenze bei exakt 0: dort waere
      // derselbe echte Messwert verschwunden.
      pruefe("mit Untergrenze 0 waere -0,1 kg ausgefallen",
             veroeffentlichtes_gewicht(w, roh_fuer(w, -0.06f, w.calib_temp),
                                       0.0f, oben), NAN, 1e-6f);

      // Die halbe Kalibrierung - der Fall, der das Fenster ueberhaupt noetig
      // macht. Nullpunkt frisch gesetzt, Referenzpunkt noch der alte:
      // der Span schrumpft auf ein paar hundert counts (die Span-Pruefung im
      // Kalibrier-Button greift ab 500 nicht mehr), der Umrechnungsfaktor
      // explodiert und die Anzeige laeuft in die Tausende - genau wie am
      // 11.08.2026 beobachtet.
      Waage kaputt = w;
      kaputt.calib_raw_ref = kaputt.calib_raw_zero + 600.0f;
      float raw_30kg = roh_fuer(w, 30.0f, w.calib_temp);
      float unsinn = gewicht(kaputt, raw_30kg);
      std::printf("   halbe Kalibrierung (Span 600 counts): Anzeige %.1f kg\n",
                  unsinn);
      pruefe("halbe Kalibrierung wird verworfen",
             veroeffentlichtes_gewicht(kaputt, raw_30kg, unten, oben), NAN,
             1e-6f);
      pruefe("... und zwar weit ausserhalb, nicht knapp",
             std::fabs(unsinn) > 1000.0f ? 1.0 : 0.0, 1.0, 0.5);
    }

    // --- Was die Untergrenze kostet, und ab wann sie greift ---
    // Die entscheidende Eigenschaft: sie darf das Rauschen einer frisch
    // tarierten Waage NICHT anschneiden. Taete sie das, fiele nur die untere
    // Haelfte weg und der Mittelwert wanderte nach oben - ein Fehler, den man
    // der Statistik hinterher nicht ansieht.
    //
    // Gerechnet mit 14 g Streuung (= 300 counts Rauschen je Sekundenwert,
    // dieselbe Annahme wie in Punkt 10) - und das ist der ungefilterte Fall,
    // ein 6-h-Fenstermittel rauscht erheblich weniger.
    {
      std::mt19937 rng(20260812);
      const int zuege = 20000;
      std::printf("   Anteil verworfener Werte bei sigma = 14 g:\n");
      for (double mittel : {0.0, -0.5, -0.9, -1.2}) {
        std::normal_distribution<double> rauschen(mittel, 0.014);
        int raus = 0;
        for (int i = 0; i < zuege; i++) {
          float kg = (float) rauschen(rng);
          float gerundet = std::round(kg * 10.0f) / 10.0f;
          if (std::fabs(gerundet) < 0.001f) gerundet = 0.0f;
          if (!waage_gewicht_plausibel(gerundet, unten, oben)) raus++;
        }
        std::printf("     Stock bei %4.1f kg   %6.2f %%\n", mittel,
                    100.0 * raus / zuege);
        if (mittel >= -0.9) {
          pruefe("bis -0,9 kg verwirft die Untergrenze nichts",
                 raus, 0.0, 0.5);
        } else {
          pruefe("unterhalb der Grenze faellt alles aus", raus, zuege, 0.5);
        }
      }
    }
  }

  std::printf("\n== 12. Kalibrier-Sperre im Taktgeber ==\n");
  {
    // Nachbau des interval:-Blocks aus waage-basis.yaml, minutenweise.
    // Enthaelt die drei Sperren in genau der Reihenfolge, in der sie dort
    // stehen: Durchsicht -> Nachlauf -> Kalibrier-Sperre -> Messintervall.
    struct Taktgeber {
      int durchsicht = 0;
      int nachlauf = 0;
      int kalibrier = 0;
      uint32_t minuten_seit_messung = 0;
      uint32_t soll = 60;
      int gesammelt = 0;      // Rohwerte, die ins Messfenster gelaufen sind

      // Eine Minute. Liefert true, wenn veroeffentlicht wurde.
      bool minute() {
        // Das on_value des HX711 laeuft unabhaengig vom Taktgeber - hier
        // stellvertretend einmal pro Minute geprueft.
        if (durchsicht == 0 && nachlauf == 0 && kalibrier == 0) gesammelt++;

        if (durchsicht > 0) {
          durchsicht--;
          if (durchsicht == 0) nachlauf = 2;   // script durchsicht_beenden
          return false;
        }
        if (nachlauf > 0) {
          nachlauf--;
          if (nachlauf > 0) return false;
          if (kalibrier > 0) return false;     // Sperre hat Vorrang
          minuten_seit_messung = 0;
          return true;
        }
        if (kalibrier > 0) {
          kalibrier--;
          if (kalibrier > 0) return false;
          minuten_seit_messung = 0;
          return true;
        }
        minuten_seit_messung++;
        if (minuten_seit_messung >= soll) {
          minuten_seit_messung = 0;
          return true;
        }
        return false;
      }
      // Der Button-Pfad: script kalibrier_sperre_starten
      void taste(int dauer) { if (dauer > 0) kalibrier = dauer; }
      // Der Button "Jetzt messen"
      bool jetzt_messen() { kalibrier = 0; minuten_seit_messung = 0; return true; }
    };

    const int SPERRE = 10;   // substitution kalibrier_sperre

    // --- Ohne Sperre laeuft der Takt wie bisher ---
    {
      Taktgeber t;
      int erste = 0;
      for (int m = 1; m <= 120 && erste == 0; m++) if (t.minute()) erste = m;
      pruefe("ohne Sperre: planmaessige Messung in Minute 60", erste, 60, 0);
    }

    // --- Ein Tara sperrt genau die eingestellte Zeit ---
    {
      Taktgeber t;
      t.taste(SPERRE);
      int werte = 0, erster = 0;
      for (int m = 1; m <= SPERRE; m++) {
        if (t.minute()) { werte++; if (!erster) erster = m; }
      }
      pruefe("Tara: genau ein Wert innerhalb der Sperre", werte, 1, 0);
      pruefe("... und zwar erst in der letzten Minute", erster, SPERRE, 0);
      // Auch in der letzten Minute ist die Sperre beim Sammeln noch aktiv -
      // heruntergezaehlt wird erst danach. Der Wert, der dann rausgeht, ist
      // deshalb der Momentanwert der Filterkette und kein Fenstermittel.
      pruefe("waehrend der Sperre wird gar nichts gesammelt", t.gesammelt, 0, 0);
    }

    // --- Jeder weitere Druck startet die Zeit neu ---
    {
      Taktgeber t;
      t.taste(SPERRE);
      int erster = 0;
      for (int m = 1; m <= 30 && erster == 0; m++) {
        if (t.minute()) erster = m;
        // Der zweite Kalibrierschritt, gedrueckt zwischen zwei Takten
        if (m == 5) t.taste(SPERRE);
      }
      pruefe("zweiter Druck nach Minute 5 -> Wert erst in Minute 15", erster,
             15, 0);
    }

    // --- "Jetzt messen" ist der ausdrueckliche Ausweg ---
    {
      Taktgeber t;
      t.taste(SPERRE);
      t.minute();
      t.minute();
      pruefe("\"Jetzt messen\" veroeffentlicht sofort", t.jetzt_messen(), 1.0,
             0.5);
      pruefe("... und hebt die Sperre auf", t.kalibrier, 0, 0);
      int naechster = 0;
      for (int m = 1; m <= 120 && naechster == 0; m++) if (t.minute()) naechster = m;
      pruefe("danach laeuft wieder ein volles Intervall", naechster, 60, 0);
    }

    // --- Der Intervall-Zaehler ruht waehrend der Sperre ---
    {
      Taktgeber t;
      for (int m = 1; m <= 30; m++) t.minute();   // 30 min vorgelaufen
      t.taste(SPERRE);
      int erster = 0, zweiter = 0;
      for (int m = 1; m <= 200; m++) {
        if (t.minute()) { if (!erster) erster = m; else if (!zweiter) zweiter = m; }
      }
      pruefe("Sperre laeuft ab -> Wert in Minute 10", erster, SPERRE, 0);
      pruefe("danach volles Intervall, nicht die Restzeit", zweiter - erster,
             60, 0);
    }

    // --- Durchsicht und Kalibrieren gleichzeitig ---
    // Wer waehrend einer Durchsicht tariert, soll danach EINEN Wert bekommen,
    // nicht zwei: der Nachlauf tritt zurueck, die Sperre veroeffentlicht.
    {
      Taktgeber t;
      t.durchsicht = 5;
      t.taste(SPERRE);
      int werte = 0;
      for (int m = 1; m <= 20; m++) if (t.minute()) werte++;
      pruefe("Durchsicht + Tara: genau ein Wert", werte, 1, 0);
      pruefe("... und keine Sammlung dazwischen", t.gesammelt, 3, 0);
    }

    // --- Abgeschaltete Sperre ---
    {
      Taktgeber t;
      t.taste(0);          // substitution kalibrier_sperre: "0"
      pruefe("Sperre 0 sperrt nicht", t.kalibrier, 0, 0);
    }
    std::printf("   Sperre %d min, Messintervall 60 min\n", SPERRE);
  }

  std::printf("\n%d Pruefungen, %d Fehler\n", geprueft, fehler);
  return fehler == 0 ? 0 : 1;
}
