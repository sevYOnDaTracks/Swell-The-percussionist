#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
#include "SampleEngine.h"
#include "FxChain.h"
#include "Licence/SwellLicence.h"

class PercProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxSounds = 1024; // per category

    PercProcessor();
    ~PercProcessor() override;

    juce::AudioProcessorValueTreeState state;

    const juce::String getName() const override { return "Swell The Percussionist"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5; }
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override;
    int getCurrentProgram() override { return program.load(); }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    float value (const juce::String& id) const { return state.getRawParameterValue (id)->load(); }
    void setValue (const juce::String& id, float v);
    int category() const { return juce::jlimit (0, categoryCount - 1, (int) value ("instrument")); }
    static int soundCount (int category) { return sampleLists[category].count; }
    void selectCategory (int c); // the category shown in the sound list (never changes what the pads play)

    // Favourites: kept for every project, in the user's Application Support folder (by sound id, so renaming is safe).
    bool isFavourite (int category, int index) const { return favourite[(size_t) category][(size_t) index].load(); }
    void toggleFavourite (int category, int index);
    int favouriteCount (int category) const;
    bool favouritesOnly() const { return onlyFavourites.load(); }
    void setFavouritesOnly (bool);
    void setFavouritesFile (const juce::File&); // loads it, and saves there from now on (tests)

    // What the sound list shows: every sound of the category, or only its favourites.
    int visibleCount (int category) const { return favouritesOnly() ? favouriteCount (category) : soundCount (category); }
    int visibleSound (int category, int n) const; // index of the n-th visible sound, -1 if none

    // The kit: 16 pads, each holding any sound of any category. Saved with the project, so the piano roll always plays
    // the same sounds whatever the window shows. Every MIDI note plays a pad: C1 to D#2 (36 to 51, as an Ableton or
    // FL drum rack), and the same 16 pads repeat over the whole keyboard.
    struct Slot { int category = 0, index = 0; };
    Slot padSound (int pad) const { const int v = kit[(size_t) pad].load(); return { v / maxSounds, v % maxSounds }; }
    void setPadSound (int pad, int category, int index);
    static int padForNote (int note) { return ((note - firstPadNote) % padsPerPage + padsPerPage) % padsPerPage; }

    // Called from the editor, played on the next audio block: a pad of the kit, or any sound (preview from the list).
    void playPad (int pad, float velocity);
    void playSound (int category, int index, float velocity);

    // True once the embedded sounds are decoded (on a background thread at start-up).
    bool samplesReady() const { return bank.load() != nullptr; }
    bool waitForSamples (int timeoutMs) const;

    // Read by the editor: the velocity of the last hit on a pad (then reset), and the last sound played.
    float takeFlash (int pad) { return flash[pad].exchange (0.f); }
    Slot lastSound() const { const int v = lastPlayed.load(); return { v / maxSounds, v % maxSounds }; }

private:
    FxChain::Settings fxSettings() const;
    void play (const SampleBank& sounds, int category, int index, float velocity, int offset);
    void resetKit();
    void loadFavourites();
    void saveFavourites() const;

    SampleEngine engine;
    FxChain fx;
    // Partagée par toutes les instances : activer une fenêtre débloque les autres.
    juce::SharedResourcePointer<swell::Licence> licence;
    juce::MidiMessageCollector uiMidi;
    std::unique_ptr<SampleBank> ownedBank;
    std::atomic<const SampleBank*> bank { nullptr };
    std::thread loader;
    double tempo = 100;
    std::atomic<int> program { 1 }, lastPlayed { 0 };
    std::array<std::atomic<int>, padsPerPage> kit {};
    std::atomic<float> flash[padsPerPage] {};

    std::array<std::array<std::atomic<bool>, maxSounds>, categoryCount> favourite {};
    std::atomic<bool> onlyFavourites { false };
    juce::File favouritesFile;

    // Sounds asked for by the list view: written on the message thread, read on the audio thread.
    juce::AbstractFifo listQueue { 64 };
    std::array<std::pair<int, float>, 64> listPlays {}; // (category * maxSounds + index, velocity)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PercProcessor)
};
