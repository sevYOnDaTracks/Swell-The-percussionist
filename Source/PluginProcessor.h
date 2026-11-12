#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <thread>
#include "SampleEngine.h"
#include "Kits.h"
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

    float value (const juce::String& id) const { return state.getRawParameterValue (id)->load(); } // message thread
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
    // the same sounds whatever the window shows. Every MIDI note plays a pad: notes 36 to 51 (C1 to D#2 in Ableton, as a
    // drum rack) play pads 1 to 16, and the same 16 pads repeat over the whole keyboard.
    // A pad can be empty (category -1): it plays nothing.
    struct Slot { int category = 0, index = 0; bool empty() const { return category < 0; } };
    static int code (Slot s) { return s.empty() ? -1 : s.category * maxSounds + s.index; }
    Slot padSound (int pad) const { const int v = kit[(size_t) pad].load(); return v < 0 ? Slot { -1, 0 } : Slot { v / maxSounds, v % maxSounds }; }
    void setPadSound (int pad, int category, int index);
    void clearPad (int pad) { if (pad >= 0 && pad < padsPerPage) kit[(size_t) pad].store (-1); }

    // Kits (message thread): a ready-made kit fills the 16 pads and sets the effects; your own kits are saved in the
    // Kits folder next to the favourites, for every project. The name of the kit and whether pads were changed since
    // are saved with the project.
    static juce::String kitName (int k) { return juce::String::fromUTF8 (builtInKits[k].name); }
    void applyKit (int k);
    juce::StringArray userKits() const;
    bool saveUserKit (const juce::String& name);
    bool applyUserKit (const juce::String& name);
    juce::File kitsFolder() const { return favouritesFile.getParentDirectory().getChildFile ("Kits"); }
    juce::String currentKit() const { return kitLabel; } // empty: a project saved before kits existed
    bool kitModified() const { return modified.load(); }
    void markKitModified() { modified.store (true); }

    // The tutorial shows once, the first time the window opens (kept next to the favourites).
    bool tutorialSeen() const { return favouritesFile.getSiblingFile ("tuto-vu.txt").existsAsFile(); }
    void setTutorialSeen (bool seen);
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

    // The last note received (the latest in time, not the highest pad), as the editor shows it. 'count' grows with
    // every note, so the editor sees a note played twice in a row.
    struct Hit { juce::uint32 count = 0; int note = -1, pad = 0, velocity = 0; };
    Hit lastHit() const
    {
        const auto v = hits.load();
        if (v == 0) return {};
        return { (juce::uint32) (v >> 32), (int) ((v >> 16) & 0x7f), (int) ((v >> 8) & 0xff), (int) (v & 0xff) };
    }

    // A note's name as the host's piano roll writes it: C3 = 60 in Ableton, Logic, Cubase, Studio One, Bitwig and
    // Pro Tools (so pad 1 = C1), C4 = 60 in Reaper (C2), C5 = 60 in FL Studio (C3).
    static int middleCOctave();
    static juce::String noteName (int note) { return juce::MidiMessage::getMidiNoteName (note, true, true, middleCOctave()); }

private:
    FxChain::Settings fxSettings() const;
    void play (const SampleBank& sounds, int category, int index, float velocity, int offset);
    void resetKit();
    bool findSound (const juce::String& id, int& category, int& index) const;
    void loadFavourites();
    void saveFavourites() const;

    // The parameters the audio thread reads, looked up once (a lookup by name allocates).
    struct Raw { std::atomic<float> *tune, *decay, *sat, *cho, *fla, *dly, *rev, *vol; } raw {};

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
    std::atomic<juce::uint64> hits { 0 }; // count << 32 | note << 16 | pad << 8 | velocity (0-127)
    juce::uint32 hitCount = 0;            // audio thread only

    std::array<std::array<std::atomic<bool>, maxSounds>, categoryCount> favourite {};
    std::atomic<bool> onlyFavourites { false };
    juce::File favouritesFile;
    juce::String kitLabel;
    std::atomic<bool> modified { false };

    // Sounds asked for by the list view: written on the message thread, read on the audio thread.
    juce::AbstractFifo listQueue { 64 };
    std::array<std::pair<int, float>, 64> listPlays {}; // (category * maxSounds + index, velocity)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PercProcessor)
};
