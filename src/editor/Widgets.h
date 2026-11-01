#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "Theme.h"
#include "Layout.h"

namespace ui
{
using namespace juce;

// Settings every widget needs to paint (look and info bubbles).
struct Ctx
{
    char look = 'C';
    bool tips = true;
    bool power = false;
    const theme::Look& t() const { return theme::look (look); }
};

// Anything that can show an info bubble.
struct TipClient
{
    String tip;
    virtual ~TipClient() = default;
};

inline Rectangle<float> rc (const layout::Rc& r) { return { r.x, r.y, r.w, r.h }; }

inline float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Text centred in a box, never shortened with dots. Letter spacing is in px.
inline void drawText (Graphics& g, const String& s, Rectangle<float> r, Font f, Colour c,
                      Justification j = Justification::centred, float spacingPx = 0.0f)
{
    if (std::abs (spacingPx) > 1.0e-6f) f = f.withExtraKerningFactor (spacingPx / jmax (1.0f, f.getHeight())); // JUCE kerning is a fraction of the font height
    g.setFont (f);
    g.setColour (c);
    Rectangle<float> box = r.expanded (30.0f, 0.0f); // room for font differences, text is never shortened
    if (j.testFlags (Justification::left))       box = r.withWidth (r.getWidth() + 60.0f);
    else if (j.testFlags (Justification::right)) box = r.withLeft (r.getX() - 60.0f);
    g.drawText (s, box, j, false);
}

// ---- Button in the mockup's .btn style -----------------------------------------------------------
class PButton : public Component, public TipClient
{
public:
    PButton (const Ctx& c, String t, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
        : text (std::move (t)), ctx (c), size (px), kind (k) { setMouseCursor (MouseCursor::PointingHandCursor); }

    std::function<void (const ModifierKeys&)> onClick;
    std::function<void()> onDouble;
    bool on = false;
    bool hot = false;                       // red border and pink text (Auto Trim has trimmed)
    float spacing = 0.5f;
    float radius = 4.0f;
    String text;

    void setText (const String& t) { if (t != text) { text = t; repaint(); } }
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (on ? theme::sel : t.plate);
        g.fillRoundedRectangle (r, radius);
        Colour border = hot ? theme::red : on ? theme::selb : (over ? t.dim : t.line);
        g.setColour (border);
        g.drawRoundedRectangle (r, radius, 1.0f);
        const Colour tc = hot ? theme::pink : on ? theme::lilac : t.ink;
        auto f = theme::font (kind, size);
        if (text.contains ("\n"))
        {
            auto lines = StringArray::fromLines (text);
            const float lh = size * 1.15f;
            float y = r.getCentreY() - lh * (float) lines.size() * 0.5f;
            for (auto& l : lines) { drawText (g, l, { r.getX(), y, r.getWidth(), lh }, f, tc, Justification::centred, spacing); y += lh; }
        }
        else drawText (g, text, r, f, tc, Justification::centred, spacing);
    }
    void mouseEnter (const MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const MouseEvent&) override { over = false; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDouble) onDouble(); }

private:
    const Ctx& ctx;
    float size;
    theme::FontKind kind;
    bool over = false;
};

// ---- Faceplate knob, drawn like the mockup's SVG (150 x 150 design units, scaled to the component) -----
class KnobView : public Component, public TipClient
{
public:
    KnobView (RangedAudioParameter& prm, bool isMini)
        : p (prm), mini (isMini), att (prm, [this] (float) { repaint(); })
    {
        setMouseCursor (MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        att.sendInitialUpdate();
    }

    double norm() const { return p.getValue(); }

    void paint (Graphics& g) override
    {
        g.addTransform (AffineTransform::scale ((float) getWidth() / 150.0f));
        const float cx = 75.0f, cy = 75.0f;
        for (int i = 0; i <= 20; ++i)
        {
            const float a = degreesToRadians (-135.0f + (float) i * 13.5f);
            const bool major = i % 2 == 0;
            const float r1 = major ? 66.0f : 68.0f, r2 = 72.0f;
            g.setColour (major ? Colour (0xfff4f5f7) : Colour (0xff8c8f99));
            g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 2.4f : 1.4f);
        }
        if (! mini)
        {
            g.setFont (theme::font (theme::FontKind::semi, 11.0f));
            g.setColour (Colour (0xffc9cbd3));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = degreesToRadians (-135.0f + (float) i * 27.0f);
                g.drawText (String (i), Rectangle<float> (cx + std::sin (a) * 57.0f - 10.0f, cy - std::cos (a) * 57.0f + 3.5f - 9.0f, 20.0f, 12.0f), Justification::centred, false);
            }
        }
        const float rc0 = mini ? 58.0f : 46.0f, rk = mini ? 50.0f : 37.0f, ri = mini ? 45.0f : 33.0f, len = mini ? 39.0f : 29.0f, lw = mini ? 7.0f : 4.5f;
        g.setColour (Colour (0xff0b0b0d));
        g.fillEllipse (cx - rc0, cy - rc0, rc0 * 2, rc0 * 2);
        g.setColour (Colours::black);
        g.drawEllipse (cx - rc0, cy - rc0, rc0 * 2, rc0 * 2, 1.0f);
        const float ang = degreesToRadians (-135.0f + 270.0f * (float) norm());
        ColourGradient grad (Colour (0xfff1f1f4), cx - rk + 0.4f * rk * 2, cy - rk + 0.35f * rk * 2, Colour (0xff6e6f78), cx - rk + 0.4f * rk * 2 + 0.7f * rk * 2, cy - rk + 0.35f * rk * 2, true);
        grad.addColour (0.55, Colour (0xffb9bac2));
        g.setGradientFill (grad);
        g.fillEllipse (cx - rk, cy - rk, rk * 2, rk * 2);
        g.setColour (Colour (0xff4a4b53));
        g.drawEllipse (cx - rk, cy - rk, rk * 2, rk * 2, 1.0f);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawEllipse (cx - ri, cy - ri, ri * 2, ri * 2, 1.0f);
        g.setColour (Colour (0xff16171a));
        g.drawLine (cx, cy, cx + std::sin (ang) * len, cy - std::cos (ang) * len, lw);
        g.fillEllipse (cx + std::sin (ang) * len - lw * 0.5f, cy - std::cos (ang) * len - lw * 0.5f, lw, lw);   // round cap
        g.fillEllipse (cx - lw * 0.5f, cy - lw * 0.5f, lw, lw);
        if (hasKeyboardFocus (false))
        {
            g.setColour (theme::selb);
            g.drawEllipse (1.0f, 1.0f, 148.0f, 148.0f, 3.0f);
        }
    }

    void mouseDown (const MouseEvent&) override { startNorm = norm(); att.beginGesture(); grabKeyboardFocus(); }
    void mouseDrag (const MouseEvent& e) override
    {
        // the knob is 120 or 56 px on screen but the window may be scaled: distance is read in design pixels
        const double d = -(double) e.getDistanceFromDragStartY() / 250.0 * (e.mods.isShiftDown() ? 0.1 : 1.0);
        setNorm (startNorm + d);
    }
    void mouseUp (const MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { att.beginGesture(); setNorm (p.getDefaultValue()); att.endGesture(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const float dy = w.isReversed ? -w.deltaY : w.deltaY;
        if (std::abs (dy) < 1.0e-6f) return;
        att.beginGesture();
        setNorm (norm() + (dy > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 0.002 : 0.02));
        att.endGesture();
    }
    bool keyPressed (const KeyPress& k) override
    {
        const double s = k.getModifiers().isShiftDown() ? 0.002 : 0.01;
        double n = norm();
        if (k.isKeyCode (KeyPress::upKey) || k.isKeyCode (KeyPress::rightKey)) n += s;
        else if (k.isKeyCode (KeyPress::downKey) || k.isKeyCode (KeyPress::leftKey)) n -= s;
        else if (k.isKeyCode (KeyPress::homeKey)) n = 0.0;
        else if (k.isKeyCode (KeyPress::endKey)) n = 1.0;
        else return false;
        att.beginGesture(); setNorm (n); att.endGesture();
        return true;
    }
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    void setNorm (double n)
    {
        n = jlimit (0.0, 1.0, n);
        att.setValueAsPartOfGesture (p.convertFrom0to1 ((float) n));   // the attachment takes the real value, not 0..1
    }

private:
    RangedAudioParameter& p;
    bool mini;
    ParameterAttachment att;
    double startNorm = 0.0;
};

// Value text under a knob: "1.5 · +4.5 dB". Bold part in ink, the rest dim.
class KnobValue : public Component, public TipClient
{
public:
    KnobValue (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (true, false); }
    String bold, rest;
    bool left = false;                       // Mix: text starts at the left edge
    void setParts (const String& b, const String& r) { if (b != bold || r != rest) { bold = b; rest = r; repaint(); } }

    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        auto f = theme::font (theme::FontKind::mono, 12.0f);
        auto r = getLocalBounds().toFloat();
        const float wb = textWidth (f, bold), wr = textWidth (f, " " + rest);
        float x = left ? r.getX() : r.getCentreX() - (wb + wr) * 0.5f;
        const float y = r.getCentreY() + f.getAscent() * 0.5f - 1.0f;
        g.setFont (f);
        g.setColour (t.ink);
        g.drawSingleLineText (bold, roundToInt (x), roundToInt (y));
        g.setColour (t.dim);
        g.drawSingleLineText (" " + rest, roundToInt (x + wb), roundToInt (y));
    }
private:
    const Ctx& ctx;
};

// Hardware style key of the control row: grey push key with a small LED, lit in the accent colour when on.
class KeyButton : public Component, public TipClient
{
public:
    KeyButton (const Ctx& c, String t) : text (std::move (t)), ctx (c) { setMouseCursor (MouseCursor::PointingHandCursor); setWantsKeyboardFocus (true); }
    std::function<void (const ModifierKeys&)> onClick;
    bool on = false;
    String text;
    void setOn (bool v) { if (v != on) { on = v; repaint(); } }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        r.removeFromBottom (2.0f);
        const Colour acc = ctx.t().accent;
        if (on) r.translate (0.0f, 1.0f);
        else { g.setColour (Colours::black); g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 6.0f); }
        g.setGradientFill (on ? ColourGradient (Colour (0xff24262b), 0.0f, r.getY(), Colour (0xff2e3036), 0.0f, r.getBottom(), false)
                              : ColourGradient (Colour (0xff5a5d66), 0.0f, r.getY(), Colour (0xff3a3c43), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        if (on)
        {
            g.setColour (Colours::black.withAlpha (0.65f));                       // inset shadow
            g.drawLine (r.getX() + 4.0f, r.getY() + 1.5f, r.getRight() - 4.0f, r.getY() + 1.5f, 3.0f);
        }
        else
        {
            g.setColour (Colours::white.withAlpha (0.25f));
            g.drawLine (r.getX() + 4.0f, r.getY() + 1.5f, r.getRight() - 4.0f, r.getY() + 1.5f, 1.0f);
        }
        g.setColour (Colours::black);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::bold, 19.0f), on ? acc : Colour (0xfff4f5f7), Justification::centred, 0.0f);
        const float lx = r.getRight() - 5.0f - 3.0f, ly = r.getY() + 5.0f + 3.0f;
        if (on)
        {
            ColourGradient glow (acc.withAlpha (0.6f), lx, ly, acc.withAlpha (0.0f), lx + 8.0f, ly, true);
            g.setGradientFill (glow);
            g.fillEllipse (lx - 8.0f, ly - 8.0f, 16.0f, 16.0f);
            g.setColour (acc);
        }
        else g.setColour (Colour (0xff26282c));
        g.fillEllipse (lx - 3.0f, ly - 3.0f, 6.0f, 6.0f);
        if (hasKeyboardFocus (false)) { g.setColour (theme::selb); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 2.0f); }
    }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods); }
    bool keyPressed (const KeyPress& k) override { if (k == KeyPress::spaceKey || k == KeyPress::returnKey) { if (onClick) onClick (k.getModifiers()); return true; } return false; }
private:
    const Ctx& ctx;
};

// Invisible click area over an LED and its label.
class HitArea : public Component, public TipClient
{
public:
    std::function<void (const ModifierKeys&)> onClick;
    HitArea() { setMouseCursor (MouseCursor::PointingHandCursor); }
    void mouseUp (const MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick (e.mods); }
};

} // namespace ui
