#include "../Source/PluginEditor.h"
// Renders the window to a PNG: Snapshot <out.png> [category 0-9] [MIDI note played before the capture]
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (! juce::SystemStats::getEnvironmentVariable ("SNAPSHOT_LOCKED", {}).isNotEmpty()) swell::Licence::unlockForTesting();
    PercProcessor p;
    p.waitForSamples (20000);
    if (argc > 2) p.selectCategory (juce::String (argv[2]).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
    e->setSize (1180, 730);
    if (argc > 3)
    {
        p.prepareToPlay (48000, 256);
        juce::AudioBuffer<float> b (2, 256);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, juce::String (argv[3]).getIntValue(), (juce::uint8) 100), 0);
        p.processBlock (b, m);
        // No message loop here: runs the editor's timer by hand for a moment, so it shows the note.
        for (int i = 0; i < 20; ++i) { juce::Thread::sleep (10); juce::Timer::callPendingTimersSynchronously(); }
    }
    auto image = e->createComponentSnapshot (e->getLocalBounds(), true, 1.f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream s (out);
    juce::PNGImageFormat().writeImageToStream (image, s);
    return 0;
}
