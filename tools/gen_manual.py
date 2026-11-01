#!/usr/bin/env python3
"""Builds docs/manual.html (printed to PDF by headless Chrome in CI).
Input: docs/mockup/layout.json (control rects and info-bubble texts, read from the approved mockup),
docs/screenshot.png (the REAL compiled editor, rendered offscreen by FetlineSnapshot) and the tables below.
Output: docs/manual.html and docs/tour/tour.png (screenshot with numbered pink and lettered teal badges).
Usage: python3 tools/gen_manual.py [version]"""
import json, os, sys, html, base64, io
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VER = sys.argv[1] if len(sys.argv) > 1 else "0.1.0"
L = json.load(open(os.path.join(ROOT, "docs/mockup/layout.json")))
RIG = 84  # the mockup's preview rig is not part of the plugin window

def shift(r):
    x, y, w, h = r
    return [x, y - RIG if y >= 530 else y, w, h]

# (key, label, rect, tip, range/default text)
T = L["tips"]
ctrl = []
def add(label, r, tip, rng):
    ctrl.append(dict(label=label, r=shift(r), tip=tip, rng=rng))

add("Preset previous, name, next", [L["pPrev"][0], L["pPrev"][1], L["pNext"][0] + L["pNext"][2] - L["pPrev"][0], L["pPrev"][3]],
    T["pName"] + " Arrows: previous and next preset.", "16 factory presets plus your own")
add("Save", L["pSave"], T["pSave"], "Your presets can be saved, renamed, deleted, exported and imported")
add("Bypass", L["bBypass"], T["bBypass"], "Off / On; exactly the input, delayed by the plugin latency")
add("Guide", L["bGuide"], T["bGuide"], "Opens the guide")
add("Settings", L["bSettings"], T["bSettings"], "Info bubbles on/off, power saving, look A/B/C, window size")
add("About and Support", L["bAbout"], T["bAbout"], "Version, privacy, Ko-fi support, file locations")
rngs = {"input": "0 to +30 dB; default +4.5 dB", "output": "-20 to +20 dB; default +1.2 dB",
        "attack": "20 to 800 microseconds (clockwise is faster); default 87 us",
        "release": "50 to 1100 ms (clockwise is faster); default 319 ms"}
for c in L["kcells"]:
    add(c["lab"], c["knob"], c["knobTip"], rngs.get(c["id"], ""))
for g in L["grps"]:
    for k in g["keys"]:
        add(g["cap"] + " " + k["t"], k["r"], k["tip"], "Ratio: 4:1, 8:1, 12:1, 20:1, or all four (a special mode)" if g["cap"] == "Ratio" else "Meter: GR, +8, +4, Off")
add("Listen HP", L["kHP"], T["kHP"], "Off / On; detector high-pass about 100 Hz")
add("Mix", L["mixKnob"], T["mixKnob"], "0 to 100 %; default 100 %")

disp = [("Needle meter", L["vu"], T["vu"]), ("Transfer curve", L["cvCurve"], T["cvCurve"]),
        ("Gain reduction history", L["cvHist"], T["cvHist"]), ("Attack and release shape", L["cvTime"], T["cvTime"]),
        ("Input meter", L["cvIn"], T["cvIn"]), ("Output meter", L["cvOut"], T["cvOut"])]

shot = Image.open(os.path.join(ROOT, "docs/screenshot.png")).convert("RGB")
sc = shot.width / 1200.0
d = ImageDraw.Draw(shot)
try:
    font = ImageFont.truetype(os.path.join(ROOT, "assets/fonts/JetBrainsMono-Bold.ttf"), int(13 * sc))
except Exception:
    font = ImageFont.load_default()

def badge(cx, cy, text, fill):
    r = 10 * sc
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=fill, outline=(255, 255, 255), width=max(1, int(sc)))
    tw = d.textlength(text, font=font)
    d.text((cx - tw / 2, cy - 8 * sc), text, fill=(10, 10, 20), font=font)

for i, c in enumerate(ctrl, 1):
    x, y, w, h = c["r"]
    badge((x + 4) * sc, (y + 4) * sc, str(i), (255, 19, 131))
for i, (n, r, t) in enumerate(disp):
    x, y, w, h = shift(r)
    badge((x + w - 6) * sc, (y + 8) * sc, chr(65 + i), (60, 220, 200))
os.makedirs(os.path.join(ROOT, "docs/tour"), exist_ok=True)
shot.save(os.path.join(ROOT, "docs/tour/tour.png"))

def b64(img):
    b = io.BytesIO(); img.save(b, "PNG"); return "data:image/png;base64," + base64.b64encode(b.getvalue()).decode()

tour = shot
plain = Image.open(os.path.join(ROOT, "docs/screenshot.png")).convert("RGB")
E = html.escape
def rows(items):
    return "".join(f"<tr><td class=n>{n}</td><td><b>{E(l)}</b></td><td>{E(rg)}</td><td>{E(tp)}</td></tr>" for n, l, rg, tp in items)

txt = open(os.path.join(ROOT, "docs/manual.txt"), encoding="utf-8").read().replace("{VERSION}", VER)
def section(title):
    out, on = [], False
    for ln in txt.splitlines():
        if ln.startswith("## "):
            on = ln[3:].strip() == title; continue
        if on: out.append(ln)
    paras = "\n".join(out).strip().split("\n\n")
    res = []
    for p in paras:
        lines = [l for l in p.splitlines() if l.strip()]
        if lines and all(l.lstrip().startswith("- ") for l in lines):
            res.append("<ul>" + "".join(f"<li>{E(l.lstrip()[2:])}</li>" for l in lines) + "</ul>")
        elif lines:
            res.append("<p>" + E(" ".join(l.strip() for l in lines)) + "</p>")
    return "".join(res)

css = """
@page{size:A4;margin:16mm}
body{font-family:'Helvetica Neue',Arial,sans-serif;color:#1c1c28;font-size:10.5pt;line-height:1.45}
h1{color:#c00a62;font-size:26pt;margin:0 0 4px}h2{color:#0b8f84;border-bottom:2px solid #FF1383;padding-bottom:3px;margin-top:22px;page-break-after:avoid}
.cover{text-align:center;padding-top:40mm}.cover img{width:100%;border-radius:8px;margin-top:10mm}
table{border-collapse:collapse;width:100%;font-size:9pt;margin:8px 0}td,th{border-bottom:1px solid #ddd;padding:4px 6px;vertical-align:top;text-align:left}
td.n{width:22px;text-align:center;color:#fff;background:#FF1383;font-weight:bold;border-radius:50%}
.pb{page-break-before:always}img.t{width:100%}
code{background:#eee;padding:0 3px}
"""
meas = ("<ul>"
 "<li>Ratios 4:1, 8:1, 12:1, 20:1 with fixed thresholds of -10, -8, -6 and -4 dBFS at the Input knob's 0 dB. These are our own design choices from the mockup, not measurements of any hardware.</li>"
 "<li>All four ratio buttons: a special mode with ratio 16:1, threshold -5 dB, faster attack and slower release, plus more saturation. Our own design.</li>"
 "<li>Static gain computer with a 4 dB soft knee. Level calibration: with the signal below the threshold, the output matches the input within 0.002 dB at every ratio (core test).</li>"
 "<li>Detector: linked peak (the louder of left and right) with a 1 ms decay. Listen HP is a 2nd order Butterworth at 100 Hz on the detector only; the audio is not filtered.</li>"
 "<li>Attack 20 to 800 microseconds and release 50 to 1100 ms. Release is program dependent: a short burst releases faster (116 ms in our test) than a long loud passage (245 ms, same setting).</li>"
 "<li>Saturation: tanh with a drive that rises with gain reduction, inside a 2x oversampled path (61 tap linear phase half-band filters). Measured alias level on a 9.1 kHz sine at the hardest setting: about -77 dB (20:1) and -68 dB (all buttons, +30 dB input). Measured in the C++ core test, not yet in a host.</li>"
 "<li>Latency: 30 samples at 44.1 and 48 kHz, 0 at 88.2 kHz and above (no oversampling there). The plugin reports it to the host exactly. Bypass is bit-exact: the input delayed by that latency, with a 10 ms crossfade when you switch.</li>"
 "<li>Input, Output and Mix are smoothed with a 5 ms one-pole; a knob change is audible within one sample and reaches 63 % in about 5 ms.</li>"
 "<li>Stability: silence, DC, full-scale square at +30 dB, NaN, infinity and denormals are handled (core tests with sanitizers). CPU in the core test: about 4 to 6.5 % of one core for stereo at 48 kHz with sanitizers on, so lighter in a normal build.</li>"
 "<li>Not verified: how it sounds against a real 1176 style unit, any DAW other than pluginval, and the installers on real Windows, macOS and Linux machines.</li></ul>")

pages = f"""<!doctype html><html><head><meta charset=utf-8><title>FetlinePounce manual</title><style>{css}</style></head><body>
<div class=cover><h1>FetlinePounce</h1><div style="font-size:14pt">A free, open source FET style limiting amplifier</div>
<div>Studio 87 &middot; version {E(VER)} &middot; VST3</div><img src="{b64(plain)}"></div>
<h2 class=pb>Introduction</h2>{section("Introduction")}
<h2>Quick start</h2>{section("Quick start")}
<h2>Signal flow</h2><p>Input &rarr; (optional Listen HP, for the detector only) &rarr; level detector &rarr; gain computer (ratio, threshold, knee) &rarr; attack and release smoothing &rarr; gain stage with saturation (2x oversampled) &rarr; Output &rarr; Mix with the dry signal.</p>
<h2 class=pb>Interface tour</h2><p>Pink numbers mark controls, teal letters mark displays. Every control also shows an info bubble after hovering about one second; this can be switched off in Settings.</p>
<img class=t src="{b64(tour)}">
<h2 class=pb>Controls</h2><table><tr><th></th><th>Control</th><th>Range</th><th>Info bubble</th></tr>{rows([(i, c['label'], c['rng'], c['tip']) for i, c in enumerate(ctrl, 1)])}</table>
<h2>Displays</h2><table><tr><th></th><th>Display</th><th></th><th>What it shows</th></tr>{rows([(chr(65+i), n, "", t) for i, (n, r, t) in enumerate(disp)])}</table>
<p>Meters use a dB scale. Colours: blue up to -12 dB, purple from -12 to 0 dB, pink at clipping.</p>
<h2 class=pb>Under the hood</h2>{meas}
<h2>Recommendations</h2>{section("Recommendations")}
<h2>Install, remove, files</h2>{section("Install")}
<h2>Privacy and licence</h2><p>FetlinePounce collects nothing and never uses the network. The only link, in About and Support, opens your browser when you click it. Licence: AGPL-3.0-or-later, see LICENSE. Built with JUCE.</p>
</body></html>"""
open(os.path.join(ROOT, "docs/manual.html"), "w", encoding="utf-8").write(pages)
print("ok", len(ctrl), "controls", len(disp), "displays")
