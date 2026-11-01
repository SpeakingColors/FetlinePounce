// FetlinePounce core tests. Build and run with: sh tests/run.sh
// "measured" below means measured on this core by these tests. Hardware behaviour is NOT measured:
// ratio values, thresholds, knee, time maps and colour amounts come from the approved mockup (our choices).
#include "../src/core/FetlinePounce.h"
#include <cfloat>
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>

using namespace fp;
static int fails = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++fails; std::printf ("FAIL: "); } else std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static const double PI = 3.14159265358979323846;

static Pounce make (const Params& p, double fs = 48000.0)
{
    Pounce c; c.setParams (p); c.prepare (fs); return c;
}
static std::vector<float> sine (double fs, double f, double amp, int n)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2.0 * PI * f * i / fs));
    return v;
}
static double rms (const std::vector<float>& x, int a, int n)
{
    double s = 0; for (int i = 0; i < n; ++i) s += (double) x[(size_t) (a + i)] * x[(size_t) (a + i)];
    return std::sqrt (s / n);
}
static std::vector<float> run (Pounce& c, std::vector<float> x)
{
    std::vector<float> r = x;   // second channel is a copy, so the link has no effect
    std::vector<float> y = x;
    c.process (y.data(), r.data(), (int) y.size());
    return y;
}
static bool finiteAll (const std::vector<float>& v) { for (float a : v) if (! std::isfinite (a)) return false; return true; }
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

// radix-2 FFT, power of two
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * PI / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
}

int main()
{
    const double FS = 48000.0;

    // 1. Maps and defaults
    CHECK (std::fabs (attackUsFromKnob (0) - 800.0f) < 0.01f && std::fabs (attackUsFromKnob (10) - 20.0f) < 0.01f, "attack knob 0 = 800 us, 10 = 20 us");
    CHECK (std::fabs (releaseMsFromKnob (0) - 1100.0f) < 0.1f && std::fabs (releaseMsFromKnob (10) - 50.0f) < 0.01f, "release knob 0 = 1.1 s, 10 = 50 ms");
    CHECK (std::fabs (knobFromAttackUs (attackUsFromKnob (6.0f)) - 6.0f) < 1e-3f && std::fabs (knobFromReleaseMs (releaseMsFromKnob (4.0f)) - 4.0f) < 1e-3f, "knob maps round trip");
    CHECK (std::fabs (attackUsFromKnob (6) - 87.4f) < 0.2f && std::fabs (releaseMsFromKnob (4) - 319.5f) < 1.0f, "default knobs 6 and 4 = 87 us and 319 ms");
    CHECK (grStatic (-30, 0, false) == 0.0f && std::fabs (grStatic (0.0f, 0, false) - 7.5f) < 0.01f && std::fabs (grStatic (0.0f, 3, false) - 0.95f * 4.0f) < 0.01f, "static curve: 4:1 at 0 dB = 7.5 dB, 20:1 at 0 dB = 3.8 dB");
    {
        bool mono = true; float prev = -1;
        for (float l = -30; l <= 10; l += 0.25f) { const float g = grStatic (l, 1, false); mono = mono && g >= prev - 1e-6f; prev = g; }
        CHECK (mono, "static curve is monotonic");
    }

    // 2. Latency: impulse peak sits exactly at latencySamples(), at 44.1, 48 and 96 kHz
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Params p; p.inputDb = 0; p.outputDb = 0; p.mix = 1;
        auto c = make (p, fs);
        std::vector<float> x (512, 0.0f); x[100] = 0.01f;
        auto y = run (c, x);
        int pk = 0; for (int i = 0; i < 512; ++i) if (std::fabs (y[(size_t) i]) > std::fabs (y[(size_t) pk])) pk = i;
        CHECK (pk - 100 == c.latencySamples(), "impulse peak %d samples after input, reported latency %d (%.0f Hz)", pk - 100, c.latencySamples(), fs);
        Params q = p; q.mix = 0;
        auto d = make (q, fs); auto z = run (d, x);
        int pk2 = 0; for (int i = 0; i < 512; ++i) if (std::fabs (z[(size_t) i]) > std::fabs (z[(size_t) pk2])) pk2 = i;
        CHECK (pk2 == pk, "dry path (Mix 0) is delayed by the same latency (%.0f Hz)", fs);
    }

    // 3. Half-band chain is flat to 20 kHz (linear level, no limiting) and kills the images
    {
        Params p; p.inputDb = 0; p.outputDb = 0;
        double worst = 0;
        for (double f : { 100.0, 1000.0, 5000.0, 10000.0, 16000.0, 20000.0 })
        {
            auto c = make (p);
            auto y = run (c, sine (FS, f, 0.003, 48000));
            const double g = db (rms (y, 24000, 24000) / (0.003 / std::sqrt (2.0)));
            worst = std::max (worst, std::fabs (g));
        }
        CHECK (worst < 0.1, "flat within %.3f dB from 100 Hz to 20 kHz at a linear level", worst);
        double dc; { auto c = make (p); std::vector<float> d (48000, 0.003f); auto y = run (c, d); dc = db (y[40000] / 0.003); }
        CHECK (std::fabs (dc) < 0.05, "DC gain %.3f dB", dc);
    }

    // 4. Level calibration: a quiet sine passes at 0 dB with Input 0 and Output 0
    {
        Params p; p.inputDb = 0; p.outputDb = 0;
        for (int ri = 0; ri < 4; ++ri)
        {
            p.ratioIndex = ri;
            auto c = make (p); auto y = run (c, sine (FS, 1000, 0.1, 48000));
            const double g = db (rms (y, 24000, 24000) / (0.1 / std::sqrt (2.0)));
            CHECK (std::fabs (g) < 0.1 && c.grDb() < 0.01f, "-20 dBFS sine at ratio %g:1 passes at %+.3f dB, no gain reduction", kRatioVal[ri], g);
        }
        Params q = p; q.outputDb = 6.0f;
        auto c = make (q); auto y = run (c, sine (FS, 1000, 0.1, 48000));
        CHECK (std::fabs (db (rms (y, 24000, 24000) / (0.1 / std::sqrt (2.0))) - 6.0) < 0.15, "Output +6 dB gives +6 dB");
        Params m = p; m.outputDb = -20.0f;
        auto c2 = make (m); auto y2 = run (c2, sine (FS, 1000, 0.1, 48000));
        CHECK (std::fabs (db (rms (y2, 24000, 24000) / (0.1 / std::sqrt (2.0))) + 20.0) < 0.15, "Output -20 dB gives -20 dB");
        Params k = p; k.inputDb = 10.0f;
        auto c3 = make (k); auto y3 = run (c3, sine (FS, 1000, 0.01, 48000));
        CHECK (std::fabs (db (rms (y3, 24000, 24000) / (0.01 / std::sqrt (2.0))) - 10.0) < 0.15, "Input +10 dB below the threshold gives +10 dB");
    }

    // 5. Gain reduction slope per ratio button (measured steady GR at two levels 3 dB apart)
    for (int ri = 0; ri < 4; ++ri)
    {
        Params p; p.inputDb = 0; p.outputDb = 0; p.ratioIndex = ri; p.attackUs = 20; p.releaseMs = 1100;
        auto grAt = [&] (double ampDb)
        {
            auto c = make (p); (void) run (c, sine (FS, 1000, std::pow (10.0, ampDb / 20.0), 96000)); return (double) c.grDb();
        };
        const double l1 = kThrDb[ri] + 3.0, l2 = kThrDb[ri] + 6.0;     // above the knee
        const double g1 = grAt (l1), g2 = grAt (l2);
        const double slope = (g2 - g1) / 3.0, want = 1.0 - 1.0 / kRatioVal[ri];
        CHECK (std::fabs (slope - want) < 0.05, "ratio %g:1 measured slope %.3f of dB in (ideal %.3f), GR %.1f dB at %+.0f dBFS and %.1f dB at %+.0f dBFS", kRatioVal[ri], slope, want, g1, l1, g2, l2);
    }

    // 6. Attack time: 63 percent of the final gain reduction after a step, per attack setting
    for (float us : { 20.0f, 87.0f, 300.0f, 800.0f })
    {
        Params p; p.inputDb = 0; p.outputDb = 0; p.ratioIndex = 0; p.attackUs = us; p.releaseMs = 1100;
        auto c = make (p);
        float final = 0; std::vector<float> g;
        for (int i = 0; i < 4800; ++i) { float a = 0.7f, b = a; c.process (&a, &b, 1); g.push_back (c.grDb()); }
        final = g.back();
        int t63 = 0; while (t63 < 4800 && g[(size_t) t63] < 0.632f * final) ++t63;
        const double ms = 1000.0 * (t63 - Pounce::kLatencyOs2 / 2) / FS, want = attackEffUs (us, 0, false) / 1000.0;   // the detector sees the signal after the 15 sample up-sampler delay
        CHECK (ms < 2.0 * want + 0.12 && ms > 0.4 * want - 0.03, "attack %.0f us: a level step reaches 63%% of its gain reduction in %.3f ms (time constant setting %.3f ms, measured from the moment the detector sees the step)", us, ms, want);
    }

    // 7. Release is program dependent: slower after a long loud passage
    {
        Params p; p.inputDb = 0; p.outputDb = 0; p.attackUs = 50; p.releaseMs = 200;
        auto rel63 = [&] (double loudSec)
        {
            auto c = make (p);
            (void) run (c, sine (FS, 500, 0.8, (int) (FS * loudSec)));
            const float g0 = c.grDb(); std::vector<float> sil (48000, 0.0f), s2 = sil;
            int t = 0;
            for (; t < 48000; ++t) { float a = 0, b = 0; c.process (&a, &b, 1); if (c.grDb() < g0 * 0.368f) break; }
            return 1000.0 * t / FS;
        };
        const double shortMs = rel63 (0.05), longMs = rel63 (4.0);
        CHECK (longMs > shortMs * 1.4, "release to 37%% takes %.0f ms after a 50 ms burst and %.0f ms after 4 s of loud sound", shortMs, longMs);
        Params q = p; q.releaseMs = 1100; q.attackUs = 800;
        auto c = make (q); (void) run (c, sine (FS, 500, 0.8, 96000));
        CHECK (std::isfinite (c.grDb()), "slowest settings are stable");
    }

    // 8. Listen HP: the detector ignores the bass (the sound itself is not filtered)
    {
        Params p; p.inputDb = 6; p.outputDb = 0; p.attackUs = 50;
        auto gr = [&] (bool hp) { p.hp = hp; auto c = make (p); (void) run (c, sine (FS, 40, 0.7, 96000)); return c.grDb(); };
        const float a = gr (false), b = gr (true);
        CHECK (b < a * 0.5f, "40 Hz bass: GR %.1f dB with Listen HP off, %.1f dB with it on", a, b);
        p.hp = true; auto c = make (p); auto y = run (c, sine (FS, 40, 0.001, 48000));
        CHECK (std::fabs (db (rms (y, 24000, 24000) / (0.001 / std::sqrt (2.0))) - 6.0) < 0.2, "Listen HP does not filter the audio itself (40 Hz still comes out at the +6 dB input gain)");
    }

    // 9. Mix: 0 is dry, 50 percent is the average of dry and limited, 100 percent is limited
    {
        Params p; p.inputDb = 12; p.outputDb = 0; p.mix = 1.0f;
        auto x = sine (FS, 1000, 0.5, 96000);
        auto c1 = make (p); auto w = run (c1, x);
        p.mix = 0.0f; auto c0 = make (p); auto d = run (c0, x);
        p.mix = 0.5f; auto ch = make (p); auto m = run (ch, x);
        double err = 0; for (int i = 60000; i < 90000; ++i) err = std::max (err, (double) std::fabs (m[(size_t) i] - 0.5f * (w[(size_t) i] + d[(size_t) i])));
        bool exact = true; for (int i = 100; i < 96000; ++i) exact = exact && d[(size_t) i] == x[(size_t) i - 30];
        CHECK (exact, "Mix 0 is the delayed input, bit for bit");
        CHECK (err < 0.01, "Mix 50%% is the average of limited and dry (max error %.5f)", err);
    }

    // 10. Bypass is bit exact (delayed input) and crossfades without clicks
    {
        Params p; p.bypass = true; p.inputDb = 20; p.outputDb = 10;
        auto x = sine (FS, 777, 0.9, 20000);
        auto c = make (p); auto y = run (c, x);
        bool exact = true; for (int i = 60; i < 20000; ++i) exact = exact && y[(size_t) i] == x[(size_t) i - 30];
        CHECK (exact, "Bypass on: output is the delayed input, bit for bit");
        Params q = p; q.bypass = false;
        q.inputDb = 6; q.outputDb = 0;
        auto xl = sine (FS, 777, 0.9, 40000);
        auto c2 = make (q);
        std::vector<float> first (xl.begin(), xl.begin() + 20000), r1 = first;
        c2.process (first.data(), r1.data(), 20000);
        double stepWet = 0; for (size_t i = 1; i < first.size(); ++i) stepWet = std::max (stepWet, (double) std::fabs (first[i] - first[i - 1]));
        q.bypass = true; c2.setParams (q);
        std::vector<float> second (xl.begin() + 20000, xl.end()), r2 = second;
        c2.process (second.data(), r2.data(), 20000);
        double step = std::fabs (second[0] - first.back());
        for (size_t i = 1; i < second.size(); ++i) step = std::max (step, (double) std::fabs (second[i] - second[i - 1]));
        CHECK (step < 1.3 * std::max (stepWet, 2.0 * PI * 777 / FS * 0.9), "switching bypass on while playing: largest sample step %.3f (limited sine alone steps %.3f)", step, stepWet);
    }

    // 11. Stability: silence, DC, full scale, NaN, Inf, huge values, all mode at max drive, denormals
    {
        Params p; p.inputDb = 30; p.outputDb = 20; p.all = true; p.attackUs = 20; p.releaseMs = 50;
        auto c = make (p);
        std::vector<float> s (48000, 0.0f);
        auto y = run (c, s);
        bool silent = true; for (float v : y) silent = silent && v == 0.0f;
        CHECK (silent, "silence in, silence out (no noise, no denormals)");
        std::vector<float> dcv (48000, 1.0f); y = run (c, dcv);
        CHECK (finiteAll (y), "DC at full scale and +30 dB input stays finite");
        std::vector<float> sq (48000); for (size_t i = 0; i < sq.size(); ++i) sq[i] = (i / 24) % 2 ? 1.0f : -1.0f;
        y = run (c, sq); double pk = 0; for (float v : y) pk = std::max (pk, (double) std::fabs (v));
        CHECK (finiteAll (y) && pk < 40.0, "full scale square at +30 dB, all buttons, Output +20: finite, peak %.1f", pk);
        std::vector<float> bad (4800, 0.5f); bad[100] = NAN; bad[200] = INFINITY; bad[300] = -INFINITY; bad[400] = 1e30f; bad[500] = -1e30f;
        y = run (c, bad); bool ok = finiteAll (y);
        std::vector<float> after = sine (FS, 1000, 0.1, 9600); auto z = run (c, after);
        CHECK (ok && finiteAll (z), "NaN, Inf and 1e30 inputs: output stays finite and the limiter recovers");
        Params q; q.inputDb = 0; q.outputDb = 0; auto c2 = make (q);
        (void) run (c2, sine (FS, 1000, 0.9, 9600));
        std::vector<float> tiny (48000, 1e-30f); y = run (c2, tiny);
        bool dn = true; for (float v : y) dn = dn && (v == 0.0f || std::fabs (v) >= FLT_MIN);
        CHECK (dn, "no denormal numbers in the output after loud sound");
        std::vector<float> mono = sine (FS, 440, 0.8, 4800); auto c3 = make (p); c3.process (mono.data(), nullptr, (int) mono.size());
        CHECK (finiteAll (mono), "mono (one channel) works");
    }

    // 12. Aliasing, measured: 9105 Hz sine (coherent bin), hard drive. Energy off the harmonic bins is alias.
    for (int mode = 0; mode < 2; ++mode)
    {
        Params p; p.inputDb = 30; p.outputDb = 0; p.ratioIndex = 3; p.all = mode == 1; p.attackUs = 20; p.releaseMs = 50;
        const int N = 65536, k0 = 12432;
        const double f0 = FS * k0 / N;
        auto c = make (p);
        auto x = sine (FS, f0, 0.5, 2 * N);                // exactly periodic every N samples, so the last N samples are coherent
        auto y = run (c, x);
        std::vector<std::complex<double>> a (N);
        for (int i = 0; i < N; ++i) a[(size_t) i] = y[(size_t) (N + i)];
        fft (a);
        double fund = 0, off = 0;
        for (int b = 1; b < N / 2; ++b)
        {
            const double e = std::norm (a[(size_t) b]);
            if (b == k0) fund = e;
            else if (b != 2 * k0 && b > 2) off += e;
        }
        std::printf ("info: alias level %.1f dB\n", db (std::sqrt (off / fund)));
        CHECK (db (std::sqrt (off / fund)) < -60.0, "%s, 30 dB input: energy off the harmonic bins (alias) is %.1f dB below the fundamental", mode ? "all buttons" : "20:1", db (std::sqrt (off / fund)));
    }

    // 13. Knob to sound time: a step of Output and Input is heard in the first sample and 63 percent done within 8 ms
    {
        Params p; p.inputDb = 0; p.outputDb = 0;
        auto c = make (p);
        std::vector<float> dcv (48000, 0.003f); (void) run (c, dcv);
        p.outputDb = 6.0f; c.setParams (p);
        std::vector<float> y (4800, 0.003f); std::vector<float> r = y; c.process (y.data(), r.data(), 4800);
        int first = 0; while (first < 4800 && std::fabs (y[(size_t) first] - 0.003f) < 0.003f * 0.001f) ++first;
        const float fin = 0.003f * 2.0f, st = 0.003f; int t63 = 0; while (t63 < 4800 && y[(size_t) t63] < st + 0.632f * (fin - st)) ++t63;
        CHECK (first <= 1, "Output step is audible after %d samples", first);
        CHECK (1000.0 * t63 / FS < 8.0, "Output step reaches 63%% after %.2f ms", 1000.0 * t63 / FS);
    }

    // 14. CPU
    {
        Params p; p.inputDb = 12; p.all = true;
        auto c = make (p);
        std::vector<float> x = sine (FS, 220, 0.5, 48000), r = x;
        const auto t0 = std::chrono::steady_clock::now();
        for (int rep = 0; rep < 20; ++rep) { auto a = x, b = r; c.process (a.data(), b.data(), 48000); }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double load = sec / 20.0 * 100.0;   // percent of one core for real time stereo at 48 kHz
        std::printf ("info: stereo CPU %.2f%% of one core at 48 kHz (this machine, with sanitizers on)\n", load);
        CHECK (load < 30.0, "light CPU (sanitizer build %.2f%%)", load);
    }

    std::printf (fails == 0 ? "ALL CORE TESTS PASSED\n" : "%d CORE TESTS FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
