// Renders the real editor offscreen to a PNG so it can be compared with the mockup.
// usage: FetlineSnapshot out.png [scale] [look A|B|C] [dialog none|settings|guide|about|presets|save|export|import|tip] [frames]
//        FetlineSnapshot --selftest
#include "../src/plugin/PluginProcessor.h"
#include "../src/editor/PluginEditor.h"

// Checks that need the plugin classes but no window: parameter mappings and saved state round trip.
static void setFixtureValues (FetlinePounceProcessor& a)
{
    auto set = [&a] (const char* id, float real) { auto* p = a.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (real)); };
    set ("input", 21.0f); set ("output", -3.0f); set ("attack", 40.0f); set ("release", 900.0f); set ("ratio", 3); set ("all", 1);
    set ("mix", 40.0f); set ("hp", 1); set ("meter", 2); set ("bypass", 0);
    a.setSetting (setting::look, "B"); a.setSetting (setting::preset, "Vocal Pounce");
}

// Saved state of state version 1 must keep loading (tests/fixtures/state-v1.xml).
static int fixtureTest (const juce::File& f)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    FetlinePounceProcessor p;
    auto xml = juce::XmlDocument::parse (f);
    check (xml != nullptr, "fixture parses");
    if (xml == nullptr) return 1;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    auto real = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    check (std::abs (real ("input") - 21.0f) < 0.02f && std::abs (real ("output") + 3.0f) < 0.02f && std::abs (real ("attack") - 40.0f) < 0.1f
               && std::abs (real ("release") - 900.0f) < 1.0f, "knob values from the v1 fixture");
    check ((int) real ("ratio") == 3 && real ("all") > 0.5f && std::abs (real ("mix") - 40.0f) < 0.1f && real ("hp") > 0.5f && (int) real ("meter") == 2,
           "switch and choice values from the v1 fixture");
    check (p.getSetting (setting::look, "C").toString() == "B" && p.getSetting (setting::preset, "").toString() == "Vocal Pounce", "settings from the v1 fixture");
    return bad;
}

static int selfTest (const juce::File& fixture)
{
    int bad = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what); if (! ok) ++bad; };
    FetlinePounceProcessor a, b;
    auto set = [] (FetlinePounceProcessor& p, const char* id, float real)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (real));
    };
    auto real = [] (FetlinePounceProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
    auto knob10 = [] (FetlinePounceProcessor& p, const char* id) { return 10.0f * p.apvts.getParameter (id)->getValue(); };
    // defaults are the mockup's: Input 1.5, Output 5.3, Attack 6, Release 4, Mix 100, ratio 4:1
    check (std::abs (real (a, "input") - 4.5f) < 0.01f && std::abs (knob10 (a, "input") - 1.5f) < 0.01f, "Input default is knob 1.5 (+4.5 dB)");
    check (std::abs (real (a, "output") - 1.2f) < 0.01f && std::abs (knob10 (a, "output") - 5.3f) < 0.01f, "Output default is knob 5.3 (+1.2 dB)");
    check (std::abs (knob10 (a, "attack") - 6.0f) < 0.01f && std::abs (real (a, "attack") - 87.4f) < 0.3f, "Attack default is knob 6 (87 us)");
    check (std::abs (knob10 (a, "release") - 4.0f) < 0.01f && std::abs (real (a, "release") - 319.5f) < 1.0f, "Release default is knob 4 (319 ms)");
    check ((int) real (a, "ratio") == 0 && real (a, "mix") == 100.0f && real (a, "all") < 0.5f && real (a, "hp") < 0.5f && (int) real (a, "meter") == 0, "ratio 4:1, Mix 100, switches off, meter GR");
    a.apvts.getParameter ("attack")->setValueNotifyingHost (1.0f);
    check (std::abs (real (a, "attack") - 20.0f) < 0.01f, "Attack knob 10 is 20 us (clockwise is faster)");
    a.apvts.getParameter ("attack")->setValueNotifyingHost (0.0f);
    check (std::abs (real (a, "attack") - 800.0f) < 0.1f, "Attack knob 0 is 800 us");
    a.apvts.getParameter ("release")->setValueNotifyingHost (1.0f);
    check (std::abs (real (a, "release") - 50.0f) < 0.01f, "Release knob 10 is 50 ms");
    a.apvts.getParameter ("release")->setValueNotifyingHost (0.0f);
    check (std::abs (real (a, "release") - 1100.0f) < 0.1f, "Release knob 0 is 1.1 s");
    // every ratio and meter item survives a set and read (last item included)
    bool all = true;
    for (int i = 0; i < 4; ++i) { set (a, "ratio", (float) i); all = all && (int) real (a, "ratio") == i; }
    check (all, "all 4 ratio items");
    all = true;
    for (int i = 0; i < 4; ++i) { set (a, "meter", (float) i); all = all && (int) real (a, "meter") == i; }
    check (all, "all 4 meter items");
    // state round trip including plugin settings
    setFixtureValues (a);
    juce::MemoryBlock mb; a.getStateInformation (mb);
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    check (std::abs (real (b, "input") - 21.0f) < 0.02f && (int) real (b, "ratio") == 3 && std::abs (real (b, "mix") - 40.0f) < 0.1f
               && real (b, "all") > 0.5f && (int) real (b, "meter") == 2 && std::abs (real (b, "attack") - 40.0f) < 0.1f, "parameters survive a state round trip");
    check (b.getSetting (setting::look, "C").toString() == "B" && b.getSetting (setting::preset, "").toString() == "Vocal Pounce", "settings survive a state round trip");
    // reported latency follows the sample rate
    b.prepareToPlay (48000.0, 512);
    check (b.getLatencySamples() == 30, "latency 30 samples at 48 kHz");
    b.prepareToPlay (96000.0, 512);
    check (b.getLatencySamples() == 0, "latency 0 samples at 96 kHz");
    b.prepareToPlay (48000.0, 512);
    // an editor can be made, resized and deleted
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (b.createEditor());
        ed->setSize (600, 278);
        ed->setSize (1800, 834);
        check (ed->getWidth() == 1800, "editor creates and resizes");
    }
    if (fixture != juce::File()) bad += fixtureTest (fixture);
    std::printf (bad == 0 ? "ALL SELF TESTS PASSED\n" : "SELF TESTS FAILED\n");
    return bad;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--write-fixture")
    {
        FetlinePounceProcessor p; setFixtureValues (p);
        auto state = p.apvts.copyState(); state.setProperty ("stateVersion", 1, nullptr);
        if (auto xml = state.createXml()) juce::File (argv[2]).replaceWithText (xml->toString());
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--selftest")
        return selfTest (argc > 2 ? juce::File (argv[2]) : juce::File()) == 0 ? 0 : 1;
    FetlinePounceProcessor proc;
    proc.prepareToPlay (48000.0, 800);
    if (argc > 3) proc.setSetting (setting::look, juce::String (argv[3]));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    const double sc = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 1.0;
    ed->setSize (juce::roundToInt (1200 * sc), juce::roundToInt (556 * sc));
    ed->addToDesktop (0);   // the editor only redraws while it is showing (it pauses when hidden)
    ed->setVisible (true);
    const juce::String dialog = argc > 4 ? juce::String (argv[4]) : juce::String ("none");
    const int frames = argc > 5 ? juce::String (argv[5]).getIntValue() : 30;
    // Feed a drum-and-pad loop in real time so the meters, curve dot and history show something.
    juce::AudioBuffer<float> buf (2, 800);
    juce::MidiBuffer midi;
    juce::Random rnd (7);
    long pos = 0;
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < 800; ++i, ++pos)
        {
            const double t = (double) pos / 48000.0, tk = std::fmod (t, 0.5), th = std::fmod (t + 0.25, 0.5);
            float v = (float) (0.85 * std::exp (-tk * 14.0) * std::sin (6.2831853 * (52.0 + 90.0 * std::exp (-tk * 40.0)) * tk));
            v += (float) (0.22 * std::exp (-th * 45.0) * (rnd.nextFloat() * 2.0f - 1.0f));
            v += 0.12f * (float) std::sin (6.2831853 * 220.0 * t) * (float) (0.6 + 0.4 * std::sin (6.2831853 * 0.25 * t));
            buf.setSample (0, i, v); buf.setSample (1, i, v * 0.97f);
        }
        proc.processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }
    if (dialog != "none") { if (auto* pe = dynamic_cast<FetlinePounceEditor*> (ed.get())) pe->openDialogForTesting (dialog); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    juce::Image img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    juce::File out (argv[1]);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    ed.reset();
    return 0;
}
