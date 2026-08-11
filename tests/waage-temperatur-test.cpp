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

// 1:1 der Lambda-Koerper von sensor "waage_gewicht"
static float gewicht(const Waage &w, float raw) {
  float kg = gewicht_ungerundet(w, raw);
  if (std::isnan(kg)) return NAN;
  float gerundet = std::round(kg * 10.0f) / 10.0f;
  if (std::fabs(gerundet) < 0.001f) gerundet = 0.0f;
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

  std::printf("\n%d Pruefungen, %d Fehler\n", geprueft, fehler);
  return fehler == 0 ? 0 : 1;
}
