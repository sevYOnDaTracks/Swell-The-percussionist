#include "../Source/PluginEditor.h"
// Renders the window to a PNG: Snapshot <out.png> [category 0-9]
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (! juce::SystemStats::getEnvironmentVariable ("SNAPSHOT_LOCKED", {}).isNotEmpty()) swell::Licence::unlockForTesting();
    PercProcessor p;
    p.waitForSamples (20000);
    if (argc > 2) p.selectCategory (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
    e->setSize (1180, 730);
    auto image = e->createComponentSnapshot (e->getLocalBounds(), true, 1.f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (image, s);
    return 0;
}
