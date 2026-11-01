#pragma once
#include "Widgets.h"
#include "../core/FetlinePounce.h"

namespace ui
{
// Everything the views draw, refreshed by the editor timer (message thread only).
struct Live
{
    float inPk = 0, outPk = 0;       // linear, decaying peak hold
    float grNow = 0;                 // dB, positive
    float outRmsDb = -120.0f;        // output level for the +8 and +4 meter modes
    // parameters in real units
    float inputDb = 4.5f, outputDb = 1.2f, atkUs = 87.0f, relMs = 319.0f;
    int ratio = 0, meter = 0;
    bool all = false, hp = false, bypass = false;
    float hist[360] {}; int hi = 0;
};

inline float l2d (float x) { return 20.0f * std::log10 (std::max (x, 1.0e-9f)); }

// Base of the three display canvases: drawn in the mockup's canvas units, scaled into a bordered box.
class CanvasView : public Component, public TipClient
{
public:
    CanvasView (const Ctx& c, const Live& l, float cw, float ch) : ctx (c), live (l), cvW (cw), cvH (ch) {}
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat();
        g.setColour (t.disp);
        g.fillRoundedRectangle (r, 3.0f);
        auto inner = r.reduced (1.0f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (inner.toNearestInt());
            g.addTransform (AffineTransform::scale (inner.getWidth() / cvW, inner.getHeight() / cvH).translated (inner.getX(), inner.getY()));
            draw (g, t, cvW, cvH);
        }
        g.setColour (t.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
    }
protected:
    virtual void draw (Graphics&, const theme::Look&, float w, float h) = 0;
    static void label (Graphics& g, const String& s, float x, float y, Justification j, Colour c)
    {
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        g.setColour (c);
        g.drawSingleLineText (s, roundToInt (x), roundToInt (y), j);
    }
    static void dashedLine (Graphics& g, Line<float> ln, float a, float b, float w)
    {
        const float d[2] = { a, b };
        g.drawDashedLine (ln, d, 2, w);
    }
    const Ctx& ctx;
    const Live& live;
    float cvW, cvH;
};

class CurveView : public CanvasView
{
public:
    CurveView (const Ctx& c, const Live& l) : CanvasView (c, l, 316, 104) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float x0 = -48, x1 = 6, y0 = -48, y1 = 24;
        auto X = [=] (float v) { return 30.0f + (v - x0) / (x1 - x0) * (w - 36.0f); };
        auto Y = [=] (float v) { return h - 14.0f - (v - y0) / (y1 - y0) * (h - 22.0f); };
        for (float v : { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f })
        {
            g.setColour (t.line); g.drawLine (X (v), 4, X (v), h - 14, 1.0f);
            label (g, String ((int) v), X (v), h - 4, Justification::horizontallyCentred, t.dim);
        }
        for (float v : { -48.0f, -24.0f, 0.0f, 24.0f })
        {
            g.setColour (t.line); g.drawLine (30, Y (v), w - 6, Y (v), 1.0f);
            label (g, String ((int) v), 27, Y (v) + 3, Justification::right, t.dim);
        }
        g.setColour (Colour (0xff4a4d55));
        dashedLine (g, { X (x0), Y (x0), X (x1), Y (x1) }, 3, 3, 1.0f);
        const float tx = fp::thrOf (live.ratio, live.all) - live.inputDb;
        if (tx >= x0)
        {
            g.setColour (theme::pink);
            dashedLine (g, { X (tx), 4, X (tx), h - 14 }, 4, 3, 1.0f);
            label (g, "thr", X (tx) + 3, 12, Justification::left, theme::pink);
        }
        Path curve;
        bool first = true;
        for (float v = x0; v <= x1 + 0.01f; v += 0.5f)
        {
            const float gr = live.bypass ? 0.0f : fp::grStatic (v + live.inputDb, live.ratio, live.all);
            const float o = live.bypass ? v : v + live.inputDb - gr + live.outputDb;
            const Point<float> pt (X (v), Y (jlimit (y0, y1, o)));
            if (first) curve.startNewSubPath (pt); else curve.lineTo (pt);
            first = false;
        }
        g.setColour (t.accent);
        g.strokePath (curve, PathStrokeType (2.0f));
        const float li = l2d (live.inPk), lo = l2d (live.outPk);
        if (std::isfinite (li) && li > -48.0f)
        {
            g.setColour (Colours::white);
            g.fillEllipse (X (jmin (x1, li)) - 4, Y (jlimit (y0, y1, lo)) - 4, 8, 8);
        }
    }
};

class HistView : public CanvasView
{
public:
    HistView (const Ctx& c, const Live& l) : CanvasView (c, l, 316, 104) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        auto Y = [=] (float d) { return 8.0f + d / 24.0f * (h - 22.0f); };
        for (int d = 0; d <= 24; d += 6)
        {
            g.setColour (t.line); g.drawLine (28, Y ((float) d), w - 4, Y ((float) d), 1.0f);
            label (g, d != 0 ? "-" + String (d) : String ("0"), 25, Y ((float) d) + 3, Justification::right, t.dim);
        }
        Path p;
        p.startNewSubPath (30, Y (0));
        for (int i = 0; i < 360; ++i)
            p.lineTo (30.0f + (float) i / 359.0f * (w - 34.0f), Y (jmin (24.0f, live.hist[(live.hi + i) % 360])));
        p.lineTo (w - 4, Y (0));
        p.closeSubPath();
        g.setColour (Colour::fromFloatRGBA (164 / 255.0f, 61 / 255.0f, 1.0f, 0.35f));
        g.fillPath (p);
        g.setColour (theme::selb);
        g.strokePath (p, PathStrokeType (1.6f));
        g.setFont (theme::font (theme::FontKind::mono, 11.0f));
        g.setColour (t.ink);
        g.drawSingleLineText ("GR " + String (live.grNow, 1) + " dB", 36, roundToInt (h - 4));
    }
};

class TimeView : public CanvasView
{
public:
    TimeView (const Ctx& c, const Live& l) : CanvasView (c, l, 316, 104) {}
protected:
    void draw (Graphics& g, const theme::Look& t, float w, float h) override
    {
        const float g0 = fp::grStatic (fp::thrOf (live.ratio, live.all) + 18.0f, live.ratio, live.all);
        const float aU = fp::attackEffUs (live.atkUs, live.ratio, live.all);
        const float yMax = std::max (8.0f, std::ceil (g0 / 4.0f) * 4.0f);
        auto Y = [=] (float d) { return 8.0f + d / yMax * (h - 24.0f); };
        const float pw = (w - 48.0f) / 2.0f, x1a = 30.0f, x1b = 30.0f + pw + 16.0f;
        auto panel = [&] (float x, const char* title, float tmax, std::vector<float> ticks, std::vector<std::tuple<std::function<float (float)>, Colour, bool>> curves)
        {
            for (float tk : ticks)
            {
                const float px = x + tk / tmax * pw;
                g.setColour (t.line); g.drawLine (px, 6, px, h - 14, 1.0f);
                label (g, String ((int) std::round (tk * 10.0f) % 10 == 0 ? String ((int) tk) : String (tk, 1)), px, h - 4,
                       tk == ticks.back() ? Justification::right : Justification::horizontallyCentred, t.dim);
            }
            for (float d = 0; d <= yMax + 0.01f; d += yMax / 2.0f)
            {
                g.setColour (t.line); g.drawLine (x, Y (d), x + pw, Y (d), 1.0f);
                if (x == x1a) label (g, d > 0 ? "-" + String ((int) std::round (d)) : String ("0"), x - 3, Y (d) + 3, Justification::right, t.dim);
            }
            label (g, title, x + pw - 2, 16, Justification::right, t.title);
            for (auto& [fn, col, dash] : curves)
            {
                Path path;
                for (int i = 0; i <= 80; ++i)
                {
                    const float tt = tmax * (float) i / 80.0f, px = x + tt / tmax * pw, py = Y (fn (tt));
                    if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
                }
                g.setColour (col);
                if (dash) { Path d2; const float dl[2] = { 4, 3 }; PathStrokeType (2.0f).createDashedStroke (d2, path, dl, 2); g.fillPath (d2); }
                else g.strokePath (path, PathStrokeType (2.0f));
            }
        };
        panel (x1a, "attack, ms", 2, { 0, 0.5f, 1, 2 }, { { [=] (float tt) { return g0 * (1.0f - std::exp (-tt * 1000.0f / aU)); }, t.accent, false } });
        const bool all = live.all;
        const float relMs = live.relMs;
        panel (x1b, "release, ms", 1200, { 0, 400, 800, 1200 },
               { { [=] (float tt) { return g0 * std::exp (-tt / fp::releaseEffMs (relMs, 0.0f, all)); }, t.accent, false },
                 { [=] (float tt) { return g0 * std::exp (-tt / fp::releaseEffMs (relMs, 1.0f, all)); }, theme::pink, true } });
        g.setFont (theme::font (theme::FontKind::mono, 10.0f));
        g.setColour (t.dim);
        g.drawSingleLineText ("dB", 3, 12);
    }
};

// Input or output level strip, 26 x 300. Click to clear the clip marker.
class MeterView : public Component, public TipClient
{
public:
    MeterView (const Ctx& c) : ctx (c) {}
    float lin = 0.0f;
    bool latch = false;
    void update (float l)
    {
        lin = l;
        if (l2d (l) >= 0.0f) latch = true;
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const float w = (float) getWidth(), h = (float) getHeight(), mn = -60, mx = 6;
        auto y = [=] (float db) { return h - 6.0f - (jlimit (mn, mx, db) - mn) / (mx - mn) * (h - 12.0f); };
        g.fillAll (t.disp);
        const float db = l2d (lin), top = y (db);
        auto f = [=] (float d) { return (d - mn) / (mx - mn); };
        ColourGradient grad (theme::blue, 0.0f, y (mn), theme::red, 0.0f, y (mx), false);
        grad.addColour (f (-12.0f), theme::blue);
        grad.addColour (f (-12.0f) + 0.001, theme::purple);
        grad.addColour (f (0.0f), theme::purple);
        grad.addColour (f (0.0f) + 0.001, theme::red);
        g.setGradientFill (grad);
        g.fillRect (2.0f, top, 12.0f, y (mn) - top);
        g.setFont (theme::font (theme::FontKind::mono, 9.0f));
        for (float tk : { 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f })
        {
            const float yy = y (tk);
            g.setColour (t.dim);
            g.fillRect (14.0f, yy, 3.0f, 1.0f);
            g.drawSingleLineText (String ((int) tk), roundToInt (w - 1.0f), roundToInt (yy + 3.0f), Justification::right);
        }
        g.setColour (latch ? theme::red : Colour (0xff2a2c31));
        g.fillRect (2.0f, 0.0f, 12.0f, 4.0f);
    }
    void mouseDown (const MouseEvent&) override { latch = false; repaint(); }
private:
    const Ctx& ctx;
};

// The needle meter, drawn from the mockup's canvas code (760 x 460 design units).
class VuView : public Component, public TipClient
{
public:
    VuView (const Ctx&, const Live& l) : live (l) {}
    void tick (float dt)
    {
        const int m = live.meter;
        if (m == 3) { mode = 3; repaint(); return; }
        const bool gm = m == 0;
        const float mn = gm ? 0.0f : -20.0f, mx = gm ? 24.0f : 3.0f;
        const float val = gm ? live.grNow : live.outRmsDb - (m == 1 ? -10.0f : -14.0f);
        if (mode != m) { needle = mn; mode = m; }
        const float tgt = jlimit (mn - 1.0f, mx + 1.0f, val);
        needle += (tgt - needle) * (1.0f - std::exp (-dt / (gm ? 0.05f : 0.15f)));
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // the 4 px bezel with its 14 px corners, then the face inside
        g.setColour (Colour (0xff08080a));
        g.fillRoundedRectangle (b, 14.0f);
        auto in = b.reduced (4.0f);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::scale (in.getWidth() / 760.0f, in.getHeight() / 460.0f).translated (in.getX(), in.getY()));
        drawFace (g);
    }
private:
    static Font f (theme::FontKind k, float px) { return theme::font (k, px); }
    // Text with the baseline at y, like canvas fillText. Spacing is extra px between letters.
    static void text (Graphics& g, const String& s, float x, float y, bool centred, Font fo, Colour c, float spacing = 0.0f)
    {
        const float w = textWidth (fo, s) + spacing * (float) s.length() + 8.0f;
        if (std::abs (spacing) > 1.0e-6f) fo = fo.withExtraKerningFactor (spacing / std::max (1.0f, fo.getHeight()));
        g.setFont (fo); g.setColour (c);
        const float top = y - fo.getAscent();
        const float x0 = centred ? x - (w + 20.0f) * 0.5f : x;
        g.drawText (s, Rectangle<float> (x0, top, w + 20.0f, fo.getHeight() + 4.0f), centred ? Justification::centred : Justification::centredLeft, false);
    }
    void drawFace (Graphics& g)
    {
        const float W = 760, H = 460;
        Path clip; clip.addRoundedRectangle (0.0f, 0.0f, W, H, 30.0f);
        g.reduceClipRegion (clip, AffineTransform());
        const int m = live.meter;
        if (m == 3)
        {
            g.setColour (Colour (0xff17181b)); g.fillRect (0.0f, 0.0f, W, H);
            text (g, "METER OFF", W / 2, H / 2 + 10, true, f (theme::FontKind::semi, 34.0f), Colour (0xff5b5e66), 6.0f);
            return;
        }
        g.setGradientFill (ColourGradient (Colour (0xfff3ecd6), 0, 0, Colour (0xffd8cdaa), 0, H, false));
        g.fillRect (0.0f, 0.0f, W, H);
        {
            ColourGradient rg (Colour::fromFloatRGBA (1.0f, 230 / 255.0f, 160 / 255.0f, 0.45f), W / 2, 120, Colour::fromFloatRGBA (1.0f, 230 / 255.0f, 160 / 255.0f, 0.0f), W / 2 + 420, 120, true);
            g.setGradientFill (rg);
            g.fillRect (0.0f, 0.0f, W, H);
        }
        const bool gm = m == 0;
        const float mn = gm ? 0.0f : -20.0f, mx = gm ? 24.0f : 3.0f;
        const float val = gm ? live.grNow : live.outRmsDb - (m == 1 ? -10.0f : -14.0f);
        const float PX = W / 2, PY = 590, R = 470, A0 = -36, A1 = 36;
        auto ang = [=] (float v) { return degreesToRadians (A0 + (A1 - A0) * (v - mn) / (mx - mn)); };
        auto pt = [=] (float v, float r) { return Point<float> (PX + std::sin (ang (v)) * r, PY - std::cos (ang (v)) * r); };
        const float hot = gm ? 20.0f : 0.0f;
        const Colour ink (0xff2a2420), red (0xffd6135f);
        auto arc = [&] (float v0, float v1, Colour c)
        {
            Path a; a.addCentredArc (PX, PY, R + 4, R + 4, 0.0f, ang (v0), ang (v1), true);
            g.setColour (c); g.strokePath (a, PathStrokeType (10.0f, PathStrokeType::mitered, PathStrokeType::butt));
        };
        arc (mn, hot, ink); arc (hot, mx, red);
        for (float v = mn; v <= mx + 1.0e-6f; v += 1.0f)
        {
            const int iv = (int) std::round (v);
            const bool major = gm ? iv % 4 == 0 : (iv % 5 == 0 || iv == 3), mid = ! major && gm && iv % 2 == 0;
            const float len = major ? 38.0f : mid ? 26.0f : 18.0f;
            const auto p0 = pt (v, R - len), p1 = pt (v, R - 2);
            const bool hotv = gm ? v >= 20.0f : v >= 0.0f;
            g.setColour (hotv ? red : ink);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, major ? 5.0f : 2.5f);
            if (major)
            {
                const auto lp = pt (v, R - 62);
                text (g, String ((! gm && iv > 0) ? "+" : "") + String (iv), lp.x, lp.y + 10, true, f (theme::FontKind::bold, 30.0f), hotv ? red : ink);
            }
        }
        text (g, gm ? "GAIN REDUCTION  dB" : (m == 1 ? "OUTPUT  0 = +8" : "OUTPUT  0 = +4"), W / 2, 318, true, f (theme::FontKind::semi, 30.0f), ink, 5.0f);
        text (g, gm ? String (live.grNow, 1) + " dB" : String (val >= 0 ? "+" : "") + String (val, 1), W / 2, 382, true, f (theme::FontKind::mono, 40.0f), Colour (0xff4a3f36));
        // little cat head and paw
        {
            Path cat;
            cat.startNewSubPath (8, 6); cat.lineTo (24, 22); cat.lineTo (40, 22); cat.lineTo (56, 6); cat.lineTo (60, 36);
            cat.quadraticTo (60, 58, 32, 60); cat.quadraticTo (4, 58, 4, 36); cat.closeSubPath();
            g.setColour (Colour (0xffe0568f));
            g.fillPath (cat, AffineTransform::scale (1.1f).translated (618, 350));
        }
        g.setColour (Colour (0xff4a3f36));
        g.fillEllipse (112 - 17, 392 - 14, 34, 28);
        for (auto p : { Point<float> (86, 368), { 104, 356 }, { 124, 356 }, { 142, 368 } }) g.fillEllipse (p.x - 6, p.y - 8, 12, 16);
        text (g, "FETLINEPOUNCE", 60, 436, false, f (theme::FontKind::semi, 26.0f), Colour (0xff4a3f36), 4.0f);
        // needle
        const float nv = jlimit (mn - 0.8f, mx + 0.8f, needle), a = ang (nv);
        const float nx = PX + std::sin (a) * (R + 2), ny = PY - std::cos (a) * (R + 2);
        {
            Path np; np.startNewSubPath (PX, PY); np.lineTo (nx, ny);
            Path sh; PathStrokeType (5.0f, PathStrokeType::mitered, PathStrokeType::rounded).createStrokedPath (sh, np);
            DropShadow (Colours::black.withAlpha (0.35f), 8, { 5, 0 }).drawForPath (g, sh);
            g.setColour (Colour (0xffc4143c));
            g.strokePath (np, PathStrokeType (5.0f, PathStrokeType::mitered, PathStrokeType::rounded));
        }
    }
    const Live& live;
    float needle = 0; int mode = -1;
};

} // namespace ui
