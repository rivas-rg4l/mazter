#pragma once

// DSP puro (solo libreria estandar, sin JUCE): biquads, compresor, limitador
// con look-ahead y formas de saturacion.  Se prueba por separado de JUCE.

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace MasterDsp
{
    constexpr double pi = 3.14159265358979323846;

    inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    inline float linToDb (float lin) noexcept { return 20.0f * std::log10 (std::max (lin, 1.0e-9f)); }

    //==========================================================================
    // Biquad (cookbook de RBJ), forma directa II transpuesta
    //==========================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        void reset() noexcept { z1 = z2 = 0.0; }

        float process (float x) noexcept
        {
            const double in = (double) x;
            const double y = b0 * in + z1;
            z1 = b1 * in - a1 * y + z2;
            z2 = b2 * in - a2 * y;
            return (float) y;
        }

        // Magnitud de la respuesta en frecuencia (para pruebas / graficas)
        double magnitude (double freq, double sampleRate) const
        {
            const double w = 2.0 * pi * freq / sampleRate;
            const std::complex<double> e1 = std::polar (1.0, -w), e2 = std::polar (1.0, -2.0 * w);
            return std::abs ((b0 + b1 * e1 + b2 * e2) / (1.0 + a1 * e1 + a2 * e2));
        }

        void makePeak (double sr, double f, double q, double gainDb) noexcept
        {
            double c, s, al; prepare (sr, f, q, c, s, al);
            const double A = std::pow (10.0, gainDb / 40.0);
            set (1.0 + al * A, -2.0 * c, 1.0 - al * A, 1.0 + al / A, -2.0 * c, 1.0 - al / A);
        }

        void makeLowShelf (double sr, double f, double q, double gainDb) noexcept
        {
            double c, s, al; prepare (sr, f, q, c, s, al);
            const double A = std::pow (10.0, gainDb / 40.0), t = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) - (A - 1) * c + t),  2.0 * A * ((A - 1) - (A + 1) * c),  A * ((A + 1) - (A - 1) * c - t),
                      (A + 1) + (A - 1) * c + t,  -2.0 * ((A - 1) + (A + 1) * c),           (A + 1) + (A - 1) * c - t);
        }

        void makeHighShelf (double sr, double f, double q, double gainDb) noexcept
        {
            double c, s, al; prepare (sr, f, q, c, s, al);
            const double A = std::pow (10.0, gainDb / 40.0), t = 2.0 * std::sqrt (A) * al;
            set (A * ((A + 1) + (A - 1) * c + t), -2.0 * A * ((A - 1) + (A + 1) * c),  A * ((A + 1) + (A - 1) * c - t),
                      (A + 1) - (A - 1) * c + t,   2.0 * ((A - 1) - (A + 1) * c),           (A + 1) - (A - 1) * c - t);
        }

        void makeHighPass (double sr, double f, double q = 0.70710678) noexcept
        {
            double c, s, al; prepare (sr, f, q, c, s, al);
            set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + al, -2.0 * c, 1.0 - al);
        }

    private:
        static void prepare (double sr, double f, double q, double& c, double& s, double& al) noexcept
        {
            f = std::clamp (f, 10.0, sr * 0.45);
            q = std::max (q, 0.05);
            const double w = 2.0 * pi * f / sr;
            c = std::cos (w); s = std::sin (w); al = s / (2.0 * q);
        }

        void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
        {
            b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
        }
    };

    //==========================================================================
    // Compresor estereo enlazado (detector de picos, rodilla suave)
    //==========================================================================
    struct Compressor
    {
        void prepare (double sampleRate) noexcept { sr = (float) sampleRate; hpL.reset(); hpR.reset(); gr = 0.0f; }

        void setParams (float thresholdDb, float ratioIn, float attackMs, float releaseMs,
                        float kneeDb, float makeupDb, float mixIn, float scHpHz) noexcept
        {
            thr = thresholdDb; ratio = std::max (1.0f, ratioIn); knee = std::max (0.0f, kneeDb);
            makeup = makeupDb; mix = std::clamp (mixIn, 0.0f, 1.0f);
            coefA = std::exp (-1.0f / (0.001f * std::max (0.01f, attackMs) * sr));
            coefR = std::exp (-1.0f / (0.001f * std::max (1.0f, releaseMs) * sr));
            scOn = scHpHz > 21.0f;

            if (scOn)
            {
                hpL.makeHighPass ((double) sr, (double) scHpHz);
                hpR.makeHighPass ((double) sr, (double) scHpHz);
            }
        }

        // Procesa un par estereo; devuelve la reduccion de ganancia actual (dB, <= 0)
        float process (float& l, float& r) noexcept
        {
            float dl = l, dr = r;

            if (scOn) { dl = hpL.process (l); dr = hpR.process (r); }

            const float level = linToDb (std::max (std::abs (dl), std::abs (dr)));
            const float over = level - thr;
            float out;

            if (2.0f * over < -knee)
                out = level;
            else if (knee > 0.0f && 2.0f * std::abs (over) <= knee)
            {
                const float x = over + knee * 0.5f;
                out = level + (1.0f / ratio - 1.0f) * x * x / (2.0f * knee);
            }
            else
                out = thr + over / ratio;

            const float target = out - level;                 // <= 0
            const float c = target < gr ? coefA : coefR;
            gr = c * gr + (1.0f - c) * target;

            const float gain = dbToLin (gr + makeup);
            l += (l * gain - l) * mix;
            r += (r * gain - r) * mix;
            return gr;
        }

        float gr = 0.0f;

    private:
        float sr = 44100.0f, thr = -12.0f, ratio = 2.0f, knee = 6.0f, makeup = 0.0f, mix = 1.0f;
        float coefA = 0.0f, coefR = 0.0f;
        bool scOn = false;
        Biquad hpL, hpR;
    };

    //==========================================================================
    // Limitador con look-ahead (la ganancia baja ANTES de que llegue el pico)
    //==========================================================================
    struct Limiter
    {
        void prepare (double sampleRate, float lookAheadMs = 1.5f)
        {
            sr = (float) sampleRate;
            look = std::max (2, (int) std::lround (sampleRate * lookAheadMs * 0.001));
            dl.assign ((size_t) look, 0.0f); dr = dl; req.assign ((size_t) look, 1.0f);
            pos = 0; gain = 1.0f;
            coefA = 1.0f - std::exp (-5.0f / (float) look);
            setParams (ceilingDb, releaseMs);
        }

        void setParams (float ceilDb, float relMs) noexcept
        {
            ceilingDb = ceilDb; releaseMs = relMs;
            ceiling = dbToLin (ceilingDb);
            coefR = 1.0f - std::exp (-1.0f / (0.001f * std::max (1.0f, relMs) * sr));
        }

        int latency() const noexcept { return look - 1; }

        void process (float& l, float& r) noexcept
        {
            const float peak = std::max (std::abs (l), std::abs (r));
            dl[(size_t) pos] = l; dr[(size_t) pos] = r;
            req[(size_t) pos] = peak > ceiling ? ceiling / peak : 1.0f;

            float target = 1.0f;
            for (float g : req) target = std::min (target, g);

            gain += (target - gain) * (target < gain ? coefA : coefR);

            const int rd = (pos + 1) % look;                  // la muestra mas antigua
            l = std::clamp (dl[(size_t) rd] * gain, -ceiling, ceiling);
            r = std::clamp (dr[(size_t) rd] * gain, -ceiling, ceiling);
            pos = (pos + 1) % look;
        }

        float currentGainDb() const noexcept { return linToDb (gain); }

    private:
        float sr = 44100.0f, ceilingDb = -0.3f, releaseMs = 100.0f, ceiling = 0.966f;
        float gain = 1.0f, coefA = 0.5f, coefR = 0.001f;
        int look = 64, pos = 0;
        std::vector<float> dl, dr, req;
    };

    //==========================================================================
    // Saturacion
    //==========================================================================
    inline float shapeTape (float x) noexcept { return std::tanh (x); }

    inline float shapeTube (float x) noexcept        // asimetrica: agrega armonicos pares
    {
        return x >= 0.0f ? std::tanh (x) : std::tanh (x * 1.4f) / 1.4f * 1.1f;
    }

    inline float shapeSoft (float x) noexcept        // cubica suave
    {
        const float c = std::clamp (x, -1.0f, 1.0f);
        return 1.5f * (c - c * c * c / 3.0f);
    }

    inline float shapeHard (float x) noexcept { return std::clamp (x, -1.0f, 1.0f); }

    struct DcBlocker
    {
        float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
        float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    };
}
