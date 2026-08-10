// Prueft die Temperaturkompensation der Bienenstockwaage.
//
//   cd tests
//   g++ -std=c++17 -Wall -Wextra -O2 waage-temperatur-test.cpp -o test && ./test
//
// Eingebunden wird die ECHTE Header-Datei aus packages/ - geprueft wird also
// genau der Code, der auf dem ESP laeuft, keine Abschrift. Die drei
// Lambda-Koerper (Gewicht, Tara, Taktgeber) sind dagegen nachgebaut; sie
// stehen in packages/waage-basis.yaml und lassen sich nicht einbinden. Wer
// dort etwas aendert, muss den Nachbau hier mitziehen.
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

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

// --- Zustand der Waage, wie er auf dem Geraet in Globals liegt ---
// Werte von waage-eg, Stand 10.08.2026 (aus Home Assistant ausgelesen).
struct Waage {
  float calib_raw_zero = 25943.166015625f;   // leer, 03.08. 16:36
  float calib_raw_ref = -20291.0f;           // 2,218 kg, 03.08. 16:41
  float calib_kg_ref = 2.218f;
  float calib_temp = 24.5625f;               // "Kalibriert bei"
  float tare_offset = 0.0f;
  float letzte_temperatur = NAN;
  float koeffizient = 32.5f;                 // g/K
};

// 1:1 der Lambda-Koerper von sensor "waage_gewicht"
static float gewicht(const Waage &w, float raw) {
  if (std::isnan(raw)) return NAN;
  float span = w.calib_raw_ref - w.calib_raw_zero;
  float kg = 0.0f;
  if (span != 0.0f) {
    kg = (raw - w.calib_raw_zero) * w.calib_kg_ref / span;
  }
  kg -= waage_temperaturkorrektur(w.letzte_temperatur, w.calib_temp,
                                  w.koeffizient);
  kg -= w.tare_offset;
  float gerundet = std::round(kg * 10.0f) / 10.0f;
  if (std::fabs(gerundet) < 0.001f) gerundet = 0.0f;
  return gerundet;
}

// 1:1 der Lambda-Koerper des Tara-Buttons
static void tara(Waage &w, float raw) {
  if (std::isnan(raw)) return;
  float span = w.calib_raw_ref - w.calib_raw_zero;
  if (span == 0.0f) return;
  float brutto = (raw - w.calib_raw_zero) * w.calib_kg_ref / span;
  float korrektur = waage_temperaturkorrektur(w.letzte_temperatur,
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
      w.letzte_temperatur = t;
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
    w.letzte_temperatur = 18.0f;
    tara(w, roh_fuer(w, 30.0f, 18.0f));       // morgens tarieren
    pruefe("direkt nach dem Tarieren 0,0 kg",
           gewicht(w, roh_fuer(w, 30.0f, 18.0f)), 0.0f, 1e-6f);
    w.letzte_temperatur = 32.0f;               // 14 K waermer, gleiche Last
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
    w.letzte_temperatur = 12.3f;
    for (float kg = 0.0f; kg <= 200.0f; kg += 0.1f) {
      float raw = w.calib_raw_zero + kg * (w.calib_raw_ref - w.calib_raw_zero) / w.calib_kg_ref;
      pruefe("Koeffizient 0 rechnet wie bisher", gewicht(w, raw),
             std::round(kg * 10.0f) / 10.0f, 0.001f);
    }
    // Und bei genau der Kalibriertemperatur ebenfalls, trotz aktiver Kompensation
    Waage v;
    v.letzte_temperatur = v.calib_temp;
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
    w.letzte_temperatur = 20.0f;
    float raw = roh_fuer(w, 30.0f, 20.0f);
    float mit = gewicht(w, raw);
    // Der DS18B20 setzt aus: letzte_temperatur bleibt stehen (on_value
    // schreibt NAN nicht fort), die Anzeige aendert sich also nicht
    float nach_aussetzer = gewicht(w, raw);
    pruefe("einzelner Aussetzer aendert die Anzeige nicht", nach_aussetzer, mit, 1e-6f);
    // Nie eine Temperatur bekommen (frisch gebootet, Sensor defekt)
    Waage n;
    n.letzte_temperatur = NAN;
    pruefe("ohne jede Temperatur wird unkompensiert angezeigt",
           gewicht(n, roh_fuer(n, 30.0f, 24.5625f)), 30.0f, 0.051f);
  }

  std::printf("\n== 6. Nie kalibrierter Nullpunkt ==\n");
  {
    Waage w;
    w.calib_temp = NAN;               // "Kalibriert bei" leer
    w.letzte_temperatur = 10.0f;
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
        w.letzte_temperatur = T;
        ohne.letzte_temperatur = T;
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
      float letzte_temperatur = NAN;
      uint32_t minuten_seit_messung = 0;
      bool noch_kein_wert = true;
      for (int minute = 1; minute <= 30; minute++) {
        if (temp_ab != 0 && minute >= temp_ab) letzte_temperatur = 20.0f;
        if (std::isnan(letzte_temperatur) && warte < 3 && kompensation_aktiv) {
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

  std::printf("\n%d Pruefungen, %d Fehler\n", geprueft, fehler);
  return fehler == 0 ? 0 : 1;
}
