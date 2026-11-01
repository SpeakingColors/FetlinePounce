#include "PluginEditor.h"
#include "Widgets.h"
#include "Views.h"
#include "Dialogs.h"
#include "Presets.h"

using namespace juce;
using namespace ui;

namespace
{
// ---- the wood cheeks: same three colours and vertical grain as PurrOne ---------------------------
struct Mul32
{
    uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = a ^ (a >> 15);
        t *= (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

Image makeWood (uint32_t seed, bool edgeRight)
{
    const int w = 104, h = 1280;
    Image img (Image::ARGB, w, h, true);
    Graphics g (img);
    Mul32 r { seed };
    g.fillAll (Colour (0xff3d2210));
    for (int x = 0; x < w; ++x)
    {
        const double a = r.next();
        g.setColour (Colour::fromFloatRGBA (123 / 255.0f, 74 / 255.0f, 38 / 255.0f, (float) (0.40 * (0.4 + 0.6 * a))));
        g.fillRect (x, 0, 1, h);
        if (r.next() < 0.35)
        {
            const float al = (float) (0.75 * r.next());
            const double y0 = r.next() * h;
            const double len = h * (0.2 + 0.6 * r.next());
            g.setColour (Colour::fromFloatRGBA (90 / 255.0f, 52 / 255.0f, 25 / 255.0f, al));
            g.fillRect ((float) x, (float) y0, 1.0f, (float) len);
        }
        if (r.next() < 0.12)
        {
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillRect (x, 0, 1, h);
        }
    }
    g.setColour (Colours::black);
    g.fillRect (edgeRight ? 0 : w - 3, 0, 3, h);
    return img;
}

std::unique_ptr<Drawable> makeLogo()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64"><path d="M8 6 L24 22 H40 L56 6 L60 36 Q60 58 32 60 Q4 58 4 36 Z" fill="#FF7BB1"/><path d="M12 14 L21 23 H14 Z M52 14 L43 23 H50 Z" fill="#FBBDD0"/><ellipse cx="22" cy="36" rx="4" ry="5" fill="#16171a"/><ellipse cx="42" cy="36" rx="4" ry="5" fill="#16171a"/><path d="M29 44 H35 L32 48 Z" fill="#16171a"/><path d="M32 48 Q28 53 24 50 M32 48 Q36 53 40 50" stroke="#16171a" stroke-width="2" fill="none"/><path d="M2 40 H14 M2 46 H14 M62 40 H50 M62 46 H50" stroke="#FBBDD0" stroke-width="1.6"/></svg>)";
    if (auto xml = XmlDocument::parse (svg)) return Drawable::createFromSVG (*xml);
    return {};
}

class Cheek : public Component, public TipClient
{
public:
    Cheek (Image i, std::function<void()> reset) : img (std::move (i)), onReset (std::move (reset)) {}
    void paint (Graphics& g) override
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onReset) onReset(); }
private:
    Image img;
    std::function<void()> onReset;
};

struct Delay : Timer
{
    std::function<void()> fn;
    void start (int ms) { startTimer (ms); }
    void timerCallback() override { stopTimer(); if (fn) fn(); }
};

class Bubble : public Component
{
public:
    Bubble (const Ctx& c) : ctx (c) { setInterceptsMouseClicks (false, false); setVisible (false); }
    void showAt (const String& s, Rectangle<int> target, int parentW, int parentH)
    {
        text = s;
        AttributedString as = build();
        TextLayout tl; tl.createLayout (as, 282.0f);
        const int tw = (int) std::ceil (std::min (282.0f, tl.getWidth())) + 20, th = (int) std::ceil (tl.getHeight()) + 14;
        int x = jlimit (60, std::max (60, parentW - tw - 8), target.getX());
        int y = target.getBottom() + 6;
        if (y + th > parentH - 4) y = target.getY() - th - 6;
        setBounds (x, y, tw, th);
        setVisible (true);
        toFront (false);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        build().draw (g, r.reduced (10.0f, 7.0f));
    }
private:
    AttributedString build() const
    {
        AttributedString as; as.setWordWrap (AttributedString::byWord); as.setLineSpacing (2.0f);
        as.append (text, theme::font (theme::FontKind::regular, 14.0f), ctx.t().ink);
        return as;
    }
    const Ctx& ctx;
    String text;
};

class Toast : public Component
{
public:
    Toast (const Ctx&) { setInterceptsMouseClicks (false, false); setVisible (false); done.fn = [this] { setVisible (false); }; }
    void say (const String& s, int parentW, int parentH)
    {
        text = s;
        const int w = (int) textWidth (theme::font (theme::FontKind::regular, 15.0f), s) + 30;
        setBounds ((parentW - w) / 2, parentH - 34 - 28, w, 28);
        setVisible (true); toFront (false); repaint();
        done.start (1600);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (theme::selb); g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        drawText (g, text, r, theme::font (theme::FontKind::regular, 15.0f), Colours::white);
    }
private:
    String text;
    Delay done;
};

} // namespace

// =====================================================================================================
class Face : public Component, public DialogHost, private Timer
{
public:
    Face (FetlinePounceProcessor& p, std::function<void()> resetSize)
        : proc (p), onResetSize (std::move (resetSize)), logo (makeLogo()),
          curve (ctx, live), hist (ctx, live), timeV (ctx, live), vu (ctx, live),
          meterIn (ctx), meterOut (ctx), bubble (ctx), toastView (ctx), modal (*this)
    {
        const String lk = proc.getSetting (setting::look, "C").toString();
        ctx.look = lk == "A" ? 'A' : lk == "B" ? 'B' : 'C';
        ctx.tips = (bool) proc.getSetting (setting::tips, true);
        ctx.power = (bool) proc.getSetting (setting::power, false);
        setSize (layout::W, layout::H);
        setWantsKeyboardFocus (true);

        addAndMakeVisible (cheekL = new Cheek (makeWood (11, true), onResetSize));
        addAndMakeVisible (cheekR = new Cheek (makeWood (29, false), onResetSize));
        place (*cheekL, layout::cheekL); place (*cheekR, layout::cheekR);
        cheekL->tip = cheekR->tip = layout::tip_cheek;

        buildHeader();
        buildTop();
        buildControls();
        buildDisplays();
        buildMeters();

        addAndMakeVisible (bubble);
        addAndMakeVisible (toastView);
        addChildComponent (modal);
        addMouseListener (this, true);
        tipTimer.fn = [this] { showTip(); };
        refreshLive (1.0f / 60.0f);
        syncControls();
        startTimerHz (ctx.power ? 16 : 60);
    }
    ~Face() override { stopTimer(); removeMouseListener (this); }

    void openForTesting (const String& n)
    {
        if (n == "settings") modal.openSettings();
        else if (n == "guide") modal.openGuide();
        else if (n == "about") modal.openAbout();
        else if (n == "presets") modal.openBrowser();
        else if (n == "save") modal.openSave (nullptr);
        else if (n == "export") modal.openExport (nullptr);
        else if (n == "import") modal.openImport();
        else if (n == "tip") { tipHost = &meterIn; tipText = "Info bubble check."; showTip(); }
    }

    // ---- DialogHost --------------------------------------------------------------------------
    FetlinePounceProcessor& processor() override { return proc; }
    presets::Store& store() override { return presetStore; }
    Ctx& context() override { return ctx; }
    String presetName() override { return proc.getSetting (setting::preset, presets::factory()[0].name).toString(); }
    void setPresetName (const String& n) override { proc.setSetting (setting::preset, n); syncControls(); }
    void toast (const String& s) override { toastView.say (s, getWidth(), getHeight()); }
    void resetWindow() override { if (onResetSize) onResetSize(); }
    void settingsChanged() override
    {
        startTimerHz (ctx.power ? 16 : 60);
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    Preset currentValues() override
    {
        Preset p;
        p.v[0] = real (ids::input) / 3.0;
        p.v[1] = fp::knobFromAttackUs (real (ids::attack));
        p.v[2] = fp::knobFromReleaseMs (real (ids::release));
        p.v[3] = real (ids::output) / 4.0 + 5.0;
        p.v[4] = real (ids::mix);
        p.v[5] = real (ids::ratio);
        p.v[6] = real (ids::all);
        p.v[7] = real (ids::hp);
        for (int i = 0; i < 5; ++i) p.v[i] = std::round (p.v[i] * 1000.0) / 1000.0;
        return p;
    }

    void loadPreset (const Preset& p) override
    {
        setReal (ids::input, (float) (p.v[0] * 3.0));
        setReal (ids::attack, fp::attackUsFromKnob ((float) p.v[1]));
        setReal (ids::release, fp::releaseMsFromKnob ((float) p.v[2]));
        setReal (ids::output, (float) ((p.v[3] - 5.0) * 4.0));
        setReal (ids::mix, (float) p.v[4]);
        setReal (ids::ratio, (float) p.v[5]);
        setReal (ids::all, (float) p.v[6]);
        setReal (ids::hp, (float) p.v[7]);
        setPresetName (p.name);
        toast ("Loaded " + p.name);
        refreshLive (1.0f / 60.0f);
    }

    bool dirty() override
    {
        const Preset* base = presetStore.find (presetName());
        if (base == nullptr) base = &presets::factory()[0];
        const Preset now = currentValues();
        for (int i = 0; i < presets::kCount; ++i)
            if (std::abs (now.v[i] - base->v[i]) > (i == 4 ? 0.2 : i < 4 ? 0.02 : 0.01)) return true;
        return false;
    }

    // ---- painting ----------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        auto& t = ctx.t();
        const auto F = [] (theme::FontKind k, float px) { return theme::font (k, px); };
        g.fillAll (Colours::black);
        g.setColour (t.plate);
        g.fillRect (rc (layout::body).withTop (0).withHeight ((float) layout::H));
        // header
        g.setColour (t.plate2); g.fillRect (rc (layout::hdr));
        g.setColour (t.line); g.fillRect (layout::hdr.x, layout::hdr.h - 1.0f, layout::hdr.w, 1.0f);
        if (logo) logo->drawWithin (g, rc (layout::logo), RectanglePlacement::centred, 1.0f);
        drawText (g, "Fetline", rc (layout::n1), F (theme::FontKind::brand, 27.0f), Colours::white, Justification::centredLeft, 0.5f);
        drawText (g, "Pounce", rc (layout::n2), F (theme::FontKind::brand, 27.0f), theme::pink, Justification::centredLeft, 0.5f);
        drawText (g, "STUDIO 87", rc (layout::s87), F (theme::FontKind::regular, 12.0f), t.dim, Justification::centredLeft, 2.0f);
        // top plate with the three claw scratches behind the controls
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (rc (layout::top).toNearestInt());
            g.addTransform (AffineTransform::translation (layout::top.x - 30.0f, layout::top.y));
            g.setColour (theme::pink.withAlpha (0.10f));
            const float c[3][6] = { { 40, 20, 70, 110, 56, 240 }, { 70, 14, 104, 112, 88, 250 }, { 100, 10, 136, 110, 120, 256 } };
            for (auto& q : c)
            {
                Path p; p.startNewSubPath (q[0], q[1]); p.quadraticTo (q[2], q[3], q[4], q[5]);
                g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::mitered, PathStrokeType::rounded));
            }
        }
        g.setColour (t.line); g.fillRect (layout::top.x, layout::top.y + layout::top.h - 1.0f, layout::top.w, 1.0f);
        for (auto& c : layout::cells)
        {
            drawText (g, String (c.label).toUpperCase(), rc (c.labelR), F (theme::FontKind::bold, 16.0f), t.title, Justification::centred, 3.0f);
            const auto ef = F (theme::FontKind::bold, 11.0f);
            drawText (g, c.end0, rc (c.end0R), ef, t.title, Justification::centredLeft, 0.5f);
            drawText (g, c.end1, rc (c.end1R), ef, t.accent, Justification::centredRight, 0.5f);
        }
        // control row
        g.setColour (t.line); g.fillRect (layout::ctl.x, layout::ctl.y + layout::ctl.h - 1.0f, layout::ctl.w, 1.0f);
        const auto cf = F (theme::FontKind::regular, 12.0f);
        drawText (g, String (layout::ratioCapText).toUpperCase(), rc (layout::ratioCap), cf, t.title, Justification::centredLeft, 2.0f);
        drawText (g, String (layout::meterCapText).toUpperCase(), rc (layout::meterCap), cf, t.title, Justification::centredLeft, 2.0f);
        drawText (g, String (layout::detCapText).toUpperCase(), rc (layout::detCap), cf, t.title, Justification::centredLeft, 2.0f);
        drawText (g, "MIX", rc (layout::mixCap), cf, t.title, Justification::centredLeft, 2.0f);
        // displays
        g.setColour (t.plate2); g.fillRect (rc (layout::disp));
        g.setColour (t.line); g.fillRect (layout::disp.x, layout::disp.y + layout::disp.h - 1.0f, layout::disp.w, 1.0f);
        for (int i = 0; i < 3; ++i)
            drawText (g, String (layout::dispCapText[i]).toUpperCase(), rc (layout::dispCaps[i]), cf, t.title, Justification::centredLeft, 1.5f);
        // meter column
        g.setColour (t.plate2); g.fillRect (rc (layout::meters));
        g.setColour (t.line); g.fillRect (layout::meters.x, layout::meters.y, 1.0f, layout::meters.h);
        drawText (g, "IN", rc (layout::meterCaps[0]), cf, t.title, Justification::centred, 1.5f);
        drawText (g, "OUT", rc (layout::meterCaps[1]), cf, t.title, Justification::centred, 1.5f);
        auto pk = [] (float lin) { return lin > 1.0e-4f ? String (l2d (lin), 1) : String ("-inf"); };
        {
            const auto f = F (theme::FontKind::mono, 11.0f);
            const String a = pk (live.inPk), b = pk (live.outPk);
            const float wa = textWidth (f, a), wb = textWidth (f, b);
            const float cx = (layout::pkIn.x + layout::pkOut.x + layout::pkOut.w) * 0.5f;
            const float x0 = cx - (wa + 6.0f + wb) * 0.5f;
            drawText (g, a, { x0, layout::pkIn.y, wa, layout::pkIn.h }, f, t.dim);
            drawText (g, b, { x0 + wa + 6.0f, layout::pkIn.y, wb, layout::pkIn.h }, f, t.dim);
        }
        // footer
        g.setColour (t.plate2); g.fillRect (rc (layout::foot));
        g.setColour (t.line); g.fillRect (layout::foot.x, layout::foot.y, layout::foot.w, 1.0f);
        drawText (g, String ("Studio 87 ") + String::fromUTF8 ("\xc2\xb7") + " FetlinePounce v" + JucePlugin_VersionString,
                  { layout::foot.x + 12.0f, layout::foot.y, 400.0f, layout::foot.h }, cf, t.dim, Justification::centredLeft, 1.0f);
        if (auto* ed = findParentComponentOfClass<FetlinePounceEditor>())
            drawText (g, String (ed->getWidth()) + String::fromUTF8 (" \xc3\x97 ") + String (ed->getHeight()) + " px",
                      { layout::foot.x + layout::foot.w - 12.0f - 120.0f, layout::foot.y, 120.0f, layout::foot.h }, cf, t.dim, Justification::centredRight, 1.0f);
    }

    // ---- keyboard ----------------------------------------------------------------------------
    bool keyPressed (const KeyPress& k) override
    {
        if (modal.isOpen() || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown() || k.getModifiers().isAltDown()) return false;
        const juce_wchar c = CharacterFunctions::toLowerCase (k.getTextCharacter());
        const bool shift = k.getModifiers().isShiftDown();
        if (c == 'b') toggle (ids::bypass);
        else if (c == 'r') { setReal (ids::all, 0.0f); setReal (ids::ratio, (float) (((int) real (ids::ratio) + (shift ? 3 : 1)) % 4)); }
        else if (c == 'a') toggle (ids::all);
        else if (c == 'h') toggle (ids::hp);
        else if (c == 'm') setReal (ids::meter, (float) (((int) real (ids::meter) + 1) % 4));
        else return false;
        return true;
    }

    // ---- info bubbles ------------------------------------------------------------------------
    void mouseMove (const MouseEvent& e) override { trackTip (e); }
    void mouseEnter (const MouseEvent& e) override { trackTip (e); }
    void mouseExit (const MouseEvent&) override { hideTip(); }
    void mouseDown (const MouseEvent& e) override
    {
        hideTip();
        if (dynamic_cast<TextEditor*> (e.eventComponent) == nullptr && ! modal.isOpen()) grabKeyboardFocus();
    }

private:
    // Pause: nothing is redrawn while the window is hidden or minimised (audio and automation keep running).
    void timerCallback() override
    {
        if (! isShowing()) { lastTick = Time::getMillisecondCounterHiRes(); return; }
        const double now = Time::getMillisecondCounterHiRes();
        const float dt = (float) jlimit (0.001, 0.1, (now - lastTick) / 1000.0);
        lastTick = now;
        refreshLive (dt);
        vu.tick (dt);
        hist.repaint(); curve.repaint(); timeV.repaint();
        meterIn.update (live.inPk); meterOut.update (live.outPk);
        repaint (Rectangle<int> (1056, 370, 92, 24));
        syncControls();
    }

    void place (Component& c, const layout::Rc& r) { c.setBounds (roundToInt (r.x), roundToInt (r.y), roundToInt (r.w), roundToInt (r.h)); }
    float real (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setReal (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    void toggle (const char* id) { setReal (id, real (id) > 0.5f ? 0.0f : 1.0f); syncControls(); }

    void refreshLive (float dt)
    {
        const float dec = std::exp (-dt / 0.1f);
        live.inPk = std::max (proc.inPeak.exchange (0.0f), live.inPk * dec);
        live.outPk = std::max (proc.outPeak.exchange (0.0f), live.outPk * dec);
        live.grNow = proc.grMeterDb.load();
        live.outRmsDb = proc.outRmsDb.load();
        // the history runs at a fixed 60 values per second, whatever the redraw rate
        histAcc += dt * 60.0f;
        const float peak = proc.grPeakDb.exchange (0.0f);
        histPeak = std::max (histPeak, peak);
        while (histAcc >= 1.0f) { live.hist[live.hi] = histPeak; live.hi = (live.hi + 1) % 360; histAcc -= 1.0f; histPeak = live.grNow; }
        live.inputDb = real (ids::input); live.outputDb = real (ids::output);
        live.atkUs = real (ids::attack); live.relMs = real (ids::release);
        live.ratio = (int) real (ids::ratio); live.meter = (int) real (ids::meter);
        live.all = real (ids::all) > 0.5f; live.hp = real (ids::hp) > 0.5f; live.bypass = real (ids::bypass) > 0.5f;
    }

    String dot() const { return String::fromUTF8 (" \xc2\xb7 "); }

    void syncControls()
    {
        bBypass->setOn (live.bypass);
        for (int i = 0; i < 4; ++i) ratioKeys[i]->setOn (! live.all && live.ratio == i);
        ratioKeys[4]->setOn (live.all);
        for (int i = 0; i < 4; ++i) meterKeys[i]->setOn (live.meter == i);
        hpKey->setOn (live.hp);
        const double kIn = real (ids::input) / 3.0, kAt = fp::knobFromAttackUs (real (ids::attack)), kRe = fp::knobFromReleaseMs (real (ids::release)),
                     kOut = real (ids::output) / 4.0 + 5.0;
        const double au = real (ids::attack), rm = real (ids::release), od = real (ids::output);
        values[0]->setParts (String (kIn, 1), dot() + "+" + String (kIn * 3.0, 1) + " dB");
        values[1]->setParts (String (kOut, 1), dot() + (od >= 0 ? "+" : "") + String (od, 1) + " dB");
        values[2]->setParts (String (kAt, 1), dot() + (au < 1000.0 ? String (roundToInt (au)) + String::fromUTF8 (" \xc2\xb5s") : String (au / 1000.0, 2) + " ms"));
        values[3]->setParts (String (kRe, 1), dot() + (rm < 1000.0 ? String (roundToInt (rm)) + " ms" : String (rm / 1000.0, 2) + " s"));
        mixValue->setParts (String (roundToInt (real (ids::mix))) + "%", "limited");
        pName->setText (presetName() + (dirty() ? " *" : ""));
    }

    // ---- building ----------------------------------------------------------------------------
    PButton* addButton (const String& text, const layout::Rc& r, const String& tip, float px = 14.0f, theme::FontKind k = theme::FontKind::regular)
    {
        auto* b = new PButton (ctx, text, px, k);
        owned.add (b);
        addAndMakeVisible (b);
        place (*b, r);
        b->tip = tip;
        return b;
    }

    void buildHeader()
    {
        auto* brand = new HitArea(); owned.add (brand); addAndMakeVisible (brand);
        brand->setBounds (roundToInt (layout::logo.x), 0, roundToInt (layout::s87.x + layout::s87.w - layout::logo.x), 46);
        brand->tip = layout::tip_brand; brand->setInterceptsMouseClicks (false, false); brand->setMouseCursor (MouseCursor::NormalCursor);
        pPrev = addButton (String::fromUTF8 ("\xe2\x97\x80"), layout::pPrev, layout::tip_pPrev);
        pName = addButton (presetName(), layout::pName, layout::tip_pName, 16.0f, theme::FontKind::semi);
        pNext = addButton (String::fromUTF8 ("\xe2\x96\xb6"), layout::pNext, layout::tip_pNext);
        pSave = addButton ("Save", layout::pSave, layout::tip_pSave);
        bBypass = addButton ("Bypass", layout::bBypass, layout::tip_bBypass);
        bGuide = addButton ("Guide", layout::bGuide, layout::tip_bGuide);
        bSettings = addButton ("Settings", layout::bSettings, layout::tip_bSettings);
        bAbout = addButton ("About & Support", layout::bAbout, layout::tip_bAbout);
        pPrev->onClick = [this] (const ModifierKeys&) { step (-1); };
        pNext->onClick = [this] (const ModifierKeys&) { step (1); };
        pSave->onClick = [this] (const ModifierKeys&) { modal.openSave (nullptr); };
        bBypass->onClick = [this] (const ModifierKeys&) { toggle (ids::bypass); };
        bGuide->onClick = [this] (const ModifierKeys&) { modal.openGuide(); };
        bSettings->onClick = [this] (const ModifierKeys&) { modal.openSettings(); };
        bAbout->onClick = [this] (const ModifierKeys&) { modal.openAbout(); };
        pName->onClick = [this] (const ModifierKeys&) { nameDelay.start (260); };
        nameDelay.fn = [this] { modal.openBrowser(); };
        pName->onDouble = [this]
        {
            nameDelay.stopTimer();
            const Preset* p = presetStore.find (presetName());
            if (p == nullptr || p->factory) { toast ("Save a copy first, factory presets cannot be renamed"); return; }
            Preset q = *p; modal.openSave (&q);
        };
    }

    void step (int d)
    {
        auto all = presetStore.all();
        int i = 0;
        for (size_t k = 0; k < all.size(); ++k) if (all[k].name == presetName()) i = (int) k;
        i = (i + d + (int) all.size()) % (int) all.size();
        loadPreset (all[(size_t) i]);
    }

    void buildTop()
    {
        addAndMakeVisible (vu);
        place (vu, layout::vu);
        vu.tip = layout::tip_vu;
        // cells: input, output, attack, release (the order of the mockup's top plate)
        for (int i = 0; i < 4; ++i)
        {
            const auto& c = layout::cells[i];
            auto* k = new KnobView (*proc.apvts.getParameter (c.name), false);
            knobs.add (k); addAndMakeVisible (k);
            place (*k, c.knob);
            k->tip = c.tip;
            auto* v = new KnobValue (ctx);
            values.add (v); addAndMakeVisible (v);
            place (*v, c.val);
            v->tip = c.tip;
            auto* lab = new HitArea(); owned.add (lab); addAndMakeVisible (lab);
            place (*lab, c.labelR); lab->tip = c.tip; lab->setInterceptsMouseClicks (false, false);
        }
    }

    void buildControls()
    {
        for (int i = 0; i < 5; ++i)
        {
            const auto& k = layout::ratioKeys[i];
            auto* b = new KeyButton (ctx, k.text);
            ratioKeys.add (b); addAndMakeVisible (b);
            place (*b, k.r); b->tip = k.tip;
            if (i < 4) b->onClick = [this, i] (const ModifierKeys&) { setReal (ids::all, 0.0f); setReal (ids::ratio, (float) i); syncControls(); };
            else b->onClick = [this] (const ModifierKeys&) { toggle (ids::all); };
        }
        for (int i = 0; i < 4; ++i)
        {
            const auto& k = layout::meterKeys[i];
            auto* b = new KeyButton (ctx, k.text);
            meterKeys.add (b); addAndMakeVisible (b);
            place (*b, k.r); b->tip = k.tip;
            b->onClick = [this, i] (const ModifierKeys&) { setReal (ids::meter, (float) i); syncControls(); };
        }
        hpKey = new KeyButton (ctx, "Listen HP");
        addAndMakeVisible (hpKey); place (*hpKey, layout::kHP); hpKey->tip = layout::hpTip;
        hpKey->onClick = [this] (const ModifierKeys&) { toggle (ids::hp); };
        auto* mk = new KnobView (*proc.apvts.getParameter (ids::mix), true);
        knobs.add (mk); addAndMakeVisible (mk); place (*mk, layout::mixKnob); mk->tip = layout::tip_mixKnob;
        mixValue = new KnobValue (ctx);
        values.add (mixValue); addAndMakeVisible (mixValue); place (*mixValue, layout::mixVal);
        mixValue->left = true; mixValue->tip = layout::tip_mixKnob;
    }

    void buildDisplays()
    {
        CanvasView* views[3] = { &curve, &hist, &timeV };
        const layout::Rc* rects[3] = { &layout::cvCurve, &layout::cvHist, &layout::cvTime };
        const char* tips[3] = { layout::tip_cvCurve, layout::tip_cvHist, layout::tip_cvTime };
        for (int i = 0; i < 3; ++i)
        {
            addAndMakeVisible (*views[i]);
            place (*views[i], *rects[i]);
            views[i]->tip = tips[i];
        }
    }

    void buildMeters()
    {
        addAndMakeVisible (meterIn); addAndMakeVisible (meterOut);
        place (meterIn, layout::cvIn); place (meterOut, layout::cvOut);
        meterIn.tip = layout::tip_cvIn; meterOut.tip = layout::tip_cvOut;
        for (auto [r, t] : { std::pair<const layout::Rc*, const char*> { &layout::pkIn, layout::tip_pkIn }, { &layout::pkOut, layout::tip_pkOut } })
        {
            auto* a = new HitArea(); owned.add (a); addAndMakeVisible (a);
            place (*a, *r); a->tip = t; a->setInterceptsMouseClicks (false, false);
        }
    }

    // ---- tips --------------------------------------------------------------------------------
    void trackTip (const MouseEvent& e)
    {
        if (modal.isOpen() || ! ctx.tips) { hideTip(); return; }
        Component* c = e.eventComponent;
        String text; Component* host = nullptr;
        while (c != nullptr && c != this)
        {
            if (auto* tc = dynamic_cast<TipClient*> (c); tc != nullptr && tc->tip.isNotEmpty()) { text = tc->tip; host = c; break; }
            c = c->getParentComponent();
        }
        if (host == nullptr || text.isEmpty()) { hideTip(); return; }
        if (host == tipHost && text == tipText) return;
        hideTip();
        tipHost = host; tipText = text;
        tipTimer.start (1000);
    }
    void showTip()
    {
        if (tipHost == nullptr || modal.isOpen()) return;
        bubble.showAt (tipText, getLocalArea (tipHost, tipHost->getLocalBounds()), getWidth(), getHeight());
    }
    void hideTip() { tipTimer.stopTimer(); bubble.setVisible (false); tipHost = nullptr; tipText = {}; }

    // ---- members -----------------------------------------------------------------------------
    FetlinePounceProcessor& proc;
    std::function<void()> onResetSize;
    Ctx ctx;
    Live live;
    presets::Store presetStore;
    std::unique_ptr<Drawable> logo;
    Cheek* cheekL = nullptr; Cheek* cheekR = nullptr;
    OwnedArray<Component> owned;
    PButton *pPrev = nullptr, *pName = nullptr, *pNext = nullptr, *pSave = nullptr, *bBypass = nullptr, *bGuide = nullptr, *bSettings = nullptr, *bAbout = nullptr;
    OwnedArray<KeyButton> ratioKeys, meterKeys;
    KeyButton* hpKey = nullptr;
    KnobValue* mixValue = nullptr;
    CurveView curve; HistView hist; TimeView timeV;
    VuView vu;
    OwnedArray<KnobView> knobs;
    OwnedArray<KnobValue> values;
    MeterView meterIn, meterOut;
    Bubble bubble; Toast toastView;
    Modal modal;
    Delay nameDelay, tipTimer;
    Component* tipHost = nullptr;
    String tipText;
    double lastTick = 0;
    float histAcc = 0, histPeak = 0;
};

// =====================================================================================================
FetlinePounceEditor::FetlinePounceEditor (FetlinePounceProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    face = std::make_unique<Face> (p, [this] { resetSize(); });
    addAndMakeVisible (*face);
    constrainer.setFixedAspectRatio ((double) kW / (double) kH);
    constrainer.setMinimumSize (480, 222);
    constrainer.setMaximumSize (1800, 834);
    setConstrainer (&constrainer);
    setResizable (true, false);
    border = std::make_unique<ResizableBorderComponent> (this, &constrainer);
    border->setBorderThickness (BorderSize<int> (5));
    addAndMakeVisible (*border);
    const double sc = jlimit (0.4, 1.5, (double) proc.getSetting (setting::scale, 1.0));
    setSize (roundToInt (kW * sc), roundToInt (kH * sc));
}

FetlinePounceEditor::~FetlinePounceEditor() = default;

void FetlinePounceEditor::resized()
{
    const double s = (double) getWidth() / (double) kW;
    face->setTransform (AffineTransform::scale ((float) s));
    face->setBounds (0, 0, kW, kH);
    border->setBounds (getLocalBounds());
    proc.setSetting (setting::scale, s);
}

void FetlinePounceEditor::resetSize() { setSize (kW, kH); }
void FetlinePounceEditor::openDialogForTesting (const juce::String& n) { face->openForTesting (n); }
