#include "Presets.h"
using namespace juce;

namespace presets
{
static Preset mk (const char* name, const char* type, StringArray tags, std::initializer_list<std::pair<const char*, double>> over)
{
    Preset p; p.name = name; p.type = type; p.tags = tags; p.factory = true;
    for (auto& o : over)
        for (int i = 0; i < kCount; ++i)
            if (String (o.first) == keys[i]) p.v[i] = o.second;
    return p;
}

const std::vector<Preset>& factory()
{
    static const std::vector<Preset> list {
        mk ("Start Here 4 to 1", "Bus", { "start", "general" }, {}),
        mk ("Vocal Pounce", "Vocal", { "catch", "bright" }, { { "input", 3 }, { "attack", 4 }, { "release", 3 }, { "ratio", 1 }, { "output", 4.7 } }),
        mk ("Vocal Smooth", "Vocal", { "smooth", "tracking" }, { { "input", 2 }, { "attack", 3 }, { "release", 5 }, { "ratio", 0 }, { "hp", 1 }, { "output", 4.3 } }),
        mk ("Vocal Air Limit", "Vocal", { "limit", "air" }, { { "input", 4 }, { "attack", 6 }, { "release", 2 }, { "ratio", 3 }, { "output", 4.1 } }),
        mk ("Snare Snap", "Drum", { "snare", "snap" }, { { "input", 3 }, { "attack", 2 }, { "release", 6 }, { "ratio", 1 }, { "output", 4.6 } }),
        mk ("Kick Slam", "Drum", { "kick", "punch" }, { { "input", 4 }, { "attack", 3 }, { "release", 6 }, { "ratio", 2 }, { "hp", 1 }, { "output", 3.5 } }),
        mk ("Room All Buttons", "Drum", { "room", "all-buttons" }, { { "input", 5 }, { "all", 1 }, { "attack", 7 }, { "release", 3 }, { "output", 4.8 } }),
        mk ("Bass Tight", "Bass", { "tight" }, { { "input", 3 }, { "attack", 3 }, { "release", 5 }, { "ratio", 1 }, { "output", 4.9 } }),
        mk ("Bass Fat Limit", "Bass", { "fat", "limit" }, { { "input", 4 }, { "attack", 5 }, { "release", 4 }, { "ratio", 3 }, { "output", 4 } }),
        mk ("Bus Glue", "Bus", { "glue", "mix bus" }, { { "input", 1.5 }, { "attack", 2 }, { "release", 3 }, { "ratio", 0 }, { "hp", 1 }, { "output", 4.1 } }),
        mk ("Hard Kick Bite", "Drum", { "kick", "hard dance" }, { { "input", 4.5 }, { "attack", 1 }, { "release", 7 }, { "ratio", 2 }, { "output", 3.5 } }),
        mk ("Lead Hold", "Keys", { "lead", "sustain" }, { { "input", 3 }, { "attack", 5 }, { "release", 2 }, { "ratio", 1 }, { "output", 5 } }),
        mk ("Parallel Smash", "Bus", { "parallel", "heavy" }, { { "input", 6 }, { "all", 1 }, { "attack", 8 }, { "release", 6 }, { "mix", 35 }, { "output", 4.8 } }),
        mk ("Guitar Crunch", "Guitar", { "crunch" }, { { "input", 4 }, { "attack", 5 }, { "release", 5 }, { "ratio", 2 }, { "output", 4.4 } }),
        mk ("Gentle Peak Catch", "Bus", { "gentle", "peaks" }, { { "input", 1.5 }, { "attack", 8 }, { "release", 2 }, { "ratio", 3 }, { "output", 4.4 } }),
        mk ("Pounce FX Burst", "FX", { "fx", "extreme" }, { { "input", 7 }, { "all", 1 }, { "attack", 9 }, { "release", 0 }, { "ratio", 3 }, { "output", 4.9 } }),
    };
    return list;
}

File userFolder()
{
   #if JUCE_LINUX || JUCE_BSD
    const String xdg = SystemStats::getEnvironmentVariable ("XDG_DATA_HOME", {});
    const File base = (xdg.isNotEmpty() && File::isAbsolutePath (xdg)) ? File (xdg)
                                                                       : File::getSpecialLocation (File::userHomeDirectory).getChildFile (".local/share");
   #else
    const File base = File::getSpecialLocation (File::userApplicationDataDirectory); // %APPDATA% or ~/Library/Application Support
   #endif
    return base.getChildFile ("Studio 87").getChildFile ("FetlinePounce").getChildFile ("presets");
}

bool valid (const var& o)
{
    auto* obj = o.getDynamicObject();
    if (obj == nullptr) return false;
    const var name = obj->getProperty ("name"), pv = obj->getProperty ("p");
    if (! name.isString() || name.toString().isEmpty() || name.toString().length() > 60) return false;
    auto* po = pv.getDynamicObject();
    if (po == nullptr) return false;
    for (auto& nv : po->getProperties())
        for (int i = 0; i < kCount; ++i)
            if (nv.name.toString() == keys[i])
            {
                if (! (nv.value.isDouble() || nv.value.isInt() || nv.value.isInt64())) return false;
                const double x = (double) nv.value;
                if (! std::isfinite (x) || x < lo[i] || x > hi[i]) return false;
            }
    return true;
}

Preset fromVar (const var& o)
{
    Preset p;
    p.name = o["name"].toString().substring (0, 60);
    const String t = o["type"].toString();
    p.type = "Bus";
    for (auto* ty : types) if (t == ty) p.type = t;
    if (auto* arr = o["tags"].getArray())
        for (auto& tg : *arr)
            if (tg.isString() && p.tags.size() < 8) p.tags.add (tg.toString().substring (0, 20));
    if (auto* po = o["p"].getDynamicObject())
        for (int i = 0; i < kCount; ++i)
            if (po->hasProperty (keys[i])) p.v[i] = (double) po->getProperty (keys[i]);
    return p;
}

var toVar (const Preset& p)
{
    auto* o = new DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("type", p.type);
    Array<var> tg; for (auto& t : p.tags) tg.add (t);
    o->setProperty ("tags", tg);
    auto* pv = new DynamicObject();
    for (int i = 0; i < kCount; ++i) pv->setProperty (keys[i], p.v[i]);
    o->setProperty ("p", var (pv));
    return var (o);
}

static String fileNameFor (const String& name)
{
    String s = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-+.");
    s = s.trim().replaceCharacters (" ", "_");
    if (s.isEmpty()) s = "preset";
    return s.substring (0, 60) + "-" + String::toHexString (name.hashCode()) + ".json";
}

Store::Store() { reload(); }

void Store::reload()
{
    mine.clear();
    auto dir = userFolder();
    if (! dir.isDirectory()) return;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        if (f.getSize() > 20000) continue;
        const var o = JSON::parse (f);
        if (! valid (o)) continue;
        Preset p = fromVar (o);
        bool clash = false;
        for (auto& x : factory()) if (x.name == p.name) clash = true;
        for (auto& x : mine) if (x.name == p.name) clash = true;
        if (! clash) mine.push_back (p);
    }
    std::sort (mine.begin(), mine.end(), [] (const Preset& a, const Preset& b) { return a.name.compareIgnoreCase (b.name) < 0; });
}

std::vector<Preset> Store::all() const
{
    std::vector<Preset> r = factory();
    r.insert (r.end(), mine.begin(), mine.end());
    return r;
}

const Preset* Store::find (const String& name) const
{
    for (auto& p : factory()) if (p.name == name) return &p;
    for (auto& p : mine) if (p.name == name) return &p;
    return nullptr;
}

bool Store::save (const Preset& p, const String& replaceName)
{
    auto dir = userFolder();
    if (! dir.createDirectory().wasOk()) return false;
    const String old = replaceName.isEmpty() ? p.name : replaceName;
    for (auto& f : dir.findChildFiles (File::findFiles, false, "*.json"))
    {
        const var o = JSON::parse (f);
        if (o["name"].toString() == old || o["name"].toString() == p.name) f.deleteFile();
    }
    Preset q = p; q.factory = false;
    if (! dir.getChildFile (fileNameFor (q.name)).replaceWithText (JSON::toString (toVar (q), false))) return false;
    reload();
    return true;
}

bool Store::remove (const String& name)
{
    bool ok = false;
    for (auto& f : userFolder().findChildFiles (File::findFiles, false, "*.json"))
        if (JSON::parse (f)["name"].toString() == name) ok = f.deleteFile() || ok;
    reload();
    return ok;
}

String Store::exportJson (const std::vector<Preset>& list) const
{
    auto* root = new DynamicObject();
    root->setProperty ("fetlinepounce", 1);
    Array<var> arr; for (auto& p : list) arr.add (toVar (p));
    root->setProperty ("presets", arr);
    return JSON::toString (var (root), false);
}

int Store::importJson (const String& text)
{
    const var o = JSON::parse (text);
    if (! o.isObject() || (int) o["fetlinepounce"] != 1) return -1;
    auto* arr = o["presets"].getArray();
    if (arr == nullptr || arr->isEmpty() || arr->size() > 200) return -1;
    for (auto& x : *arr) if (! valid (x)) return -1;
    for (auto& x : *arr)
    {
        Preset p = fromVar (x);
        for (auto& f : factory()) if (f.name == p.name) p.name += " (imported)";
        save (p);
    }
    return arr->size();
}
} // namespace presets
