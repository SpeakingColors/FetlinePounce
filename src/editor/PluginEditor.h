#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"

class Face;
// Set only by tools/snapshot.cpp so the offscreen editor keeps drawing without a real window.
inline bool fpForceShowing = false;

// Plugin window. The faceplate is a fixed 1200 x 556 drawing that is scaled to the window size;
// the window keeps its proportions and can be dragged from any edge or corner.
class FetlinePounceEditor : public juce::AudioProcessorEditor
{
public:
    explicit FetlinePounceEditor (FetlinePounceProcessor&);
    ~FetlinePounceEditor() override;
    void resized() override;
    void resetSize();
    void openDialogForTesting (const juce::String& name); // used by tools/snapshot.cpp

    static constexpr int kW = 1200, kH = 556;

private:
    FetlinePounceProcessor& proc;
    std::unique_ptr<Face> face;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FetlinePounceEditor)
};
