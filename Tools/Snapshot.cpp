#include "../Source/PluginEditor.h"
// Renders the window to a PNG: Snapshot <out.png> [category 0-9] [page] [list]
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (! juce::SystemStats::getEnvironmentVariable ("SNAPSHOT_LOCKED", {}).isNotEmpty()) swell::Licence::unlockForTesting();
    PercProcessor p;
    p.waitForSamples (20000);
    if (argc > 2) p.selectCategory (juce::String (argv[2]).getIntValue());
    if (argc > 3) p.setPage (juce::String (argv[3]).getIntValue());
    // Optional 4th argument "list": the list view, with a few example favourites (kept in a temporary file).
    const bool list = argc > 4 && juce::String (argv[4]) == "list";
    if (list)
    {
        p.setFavouritesFile (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("swell-perc-snapshot.txt"));
        for (int i : { 2, 5, 11 }) if (! p.isFavourite (p.category(), i)) p.toggleFavourite (p.category(), i);
    }
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
    if (list)
        if (auto* view = dynamic_cast<PercView*> (e->getChildComponent (0))) view->setListShown (true);
    e->setSize (1180, 730);
    auto image = e->createComponentSnapshot (e->getLocalBounds(), true, 1.f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (image, s);
    return 0;
}
