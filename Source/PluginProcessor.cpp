#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>
#include <map>

namespace
{
struct Preset { const char* name; float saturation, chorus, flangus, delay, reverb; };

const std::vector<Preset> presets
{
    { "Sec et net",    0,  0,  0,  0,  0 },
    { "Soft and Clean", 17, 0, 0,  0,  2 }, // the default
    { "Afro Room",    20,  0,  0,  0, 25 },
    { "Tape Punch",   65,  0,  0,  0, 10 },
    { "Dub Delay",    15,  0,  0, 40, 20 },
    { "Flangus Trip",  0, 30, 60, 20, 20 },
    { "Grande Salle",  0, 15,  0,  0, 55 },
};

// The kit of a new instance: a mix of every tab, low to high, so the pads play something useful right away.
struct DefaultPad { int category; float where; }; // where: 0 = lowest sound of the tab, 1 = highest
const DefaultPad defaultKit[padsPerPage]
{
    { 0, .1f }, { 0, .5f }, { 0, .9f },  // Djembé grave, médium, aigu
    { 1, .25f }, { 1, .55f }, { 1, .85f }, // Congas
    { 2, .2f }, { 2, .8f },               // Talking
    { 4, .3f }, { 3, .5f }, { 5, .5f }, { 7, .8f }, // Tom, Perc, Rim, Clap
    { 6, .5f }, { 6, .85f }, { 8, 0 }, { 9, .3f }, // Shakers, Cowbell, FX
};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto number = [&] (const char* id, const char* name, float low, float high, float step, float initial)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (low, high, step), initial));
    };

    juce::StringArray names;
    for (auto& c : categories) names.add (juce::String::fromUTF8 (c.label));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "instrument", 1 }, "Category", names, 0));

    number ("tune", "Tune", -12, 12, 1, 0);
    number ("decay", "Decay", 10, 100, 1, 100);
    // Each effect is a single amount: 0 switches it off.
    number ("sat", "Saturation", 0, 100, 1, 17);  // defaults: the Soft and Clean preset
    number ("cho", "Chorus", 0, 100, 1, 0);
    number ("fla", "Flangus", 0, 100, 1, 0);
    number ("dly", "Delay", 0, 100, 1, 0);
    number ("rev", "Reverb", 0, 100, 1, 2);
    number ("vol", "Volume", -24, 6, .5f, -3);
    return layout;
}
}

PercProcessor::PercProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "PERCUSSIONIST", createLayout())
{
    favouritesFile = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("Swell").getChildFile ("The Percussionist").getChildFile ("favoris.txt");
    auto find = [this] (const char* id) { return state.getRawParameterValue (id); };
    raw = { find ("tune"), find ("decay"), find ("sat"), find ("cho"), find ("fla"), find ("dly"), find ("rev"), find ("vol") };
    loadFavourites();
    resetKit();
    loader = std::thread ([this]
    {
        ownedBank = SampleBank::load (&BinaryData::getNamedResource);
        bank.store (ownedBank.get());
    });
}

PercProcessor::~PercProcessor()
{
    if (loader.joinable()) loader.join();
}

bool PercProcessor::waitForSamples (int timeoutMs) const
{
    const auto until = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (! samplesReady() && juce::Time::getMillisecondCounter() < until)
        juce::Thread::sleep (5);
    return samplesReady();
}

bool PercProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PercProcessor::prepareToPlay (double rate, int block)
{
    engine.prepare (rate);
    fx.prepare (rate, block);
    uiMidi.reset (rate);
}

int PercProcessor::middleCOctave()
{
    static const int octave = []
    {
        const juce::PluginHostType host;
        if (host.isFruityLoops()) return 5;
        if (host.isReaper()) return 4;
        return 3;
    }();
    return octave;
}

void PercProcessor::setValue (const juce::String& id, float v)
{
    auto* p = state.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void PercProcessor::selectCategory (int c)
{
    setValue ("instrument", (float) c);
}

void PercProcessor::resetKit()
{
    for (int pad = 0; pad < padsPerPage; ++pad)
    {
        const auto& d = defaultKit[pad];
        setPadSound (pad, d.category, juce::roundToInt (d.where * (float) (soundCount (d.category) - 1)));
    }
}

void PercProcessor::setPadSound (int pad, int c, int index)
{
    if (pad < 0 || pad >= padsPerPage || c < 0 || c >= categoryCount || index < 0 || index >= soundCount (c)) return;
    kit[(size_t) pad].store (c * maxSounds + index);
}

void PercProcessor::loadFavourites()
{
    std::map<juce::String, std::pair<int, int>> where;
    for (int c = 0; c < categoryCount; ++c)
        for (int i = 0; i < soundCount (c); ++i)
            where[sampleLists[c].entries[i].id] = { c, i };
    for (auto& list : favourite)
        for (auto& f : list) f.store (false);
    juce::StringArray ids;
    ids.addLines (favouritesFile.loadFileAsString());
    for (auto& id : ids)
        if (auto it = where.find (id.trim()); it != where.end())
            favourite[(size_t) it->second.first][(size_t) it->second.second].store (true);
}

void PercProcessor::saveFavourites() const
{
    juce::StringArray ids;
    for (int c = 0; c < categoryCount; ++c)
        for (int i = 0; i < soundCount (c); ++i)
            if (isFavourite (c, i)) ids.add (sampleLists[c].entries[i].id);
    favouritesFile.getParentDirectory().createDirectory();
    favouritesFile.replaceWithText (ids.joinIntoString ("\n"));
}

void PercProcessor::setFavouritesFile (const juce::File& f)
{
    favouritesFile = f;
    loadFavourites();
}

void PercProcessor::toggleFavourite (int c, int index)
{
    auto& f = favourite[(size_t) c][(size_t) index];
    f.store (! f.load());
    saveFavourites();
}

int PercProcessor::favouriteCount (int c) const
{
    int n = 0;
    for (int i = 0; i < soundCount (c); ++i) n += isFavourite (c, i) ? 1 : 0;
    return n;
}

void PercProcessor::setFavouritesOnly (bool on)
{
    onlyFavourites.store (on);
}

int PercProcessor::visibleSound (int c, int n) const
{
    if (n < 0) return -1;
    if (! favouritesOnly()) return n < soundCount (c) ? n : -1;
    for (int i = 0; i < soundCount (c); ++i)
        if (isFavourite (c, i) && n-- == 0) return i;
    return -1;
}

void PercProcessor::playSound (int c, int index, float velocity)
{
    const auto scope = listQueue.write (1);
    if (scope.blockSize1 > 0) listPlays[(size_t) scope.startIndex1] = { c * maxSounds + index, velocity };
}

void PercProcessor::play (const SampleBank& sounds, int c, int index, float velocity, int offset)
{
    const auto& list = sounds.sounds[(size_t) c];
    if (index < 0 || index >= (int) list.size()) return;
    engine.trigger (list[(size_t) index], { raw.tune->load(), raw.decay->load() }, velocity, offset);
    lastPlayed.store (c * maxSounds + index);
}

void PercProcessor::playPad (int pad, float velocity)
{
    auto m = juce::MidiMessage::noteOn (1, firstPadNote + pad, velocity);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * .001);
    uiMidi.addMessageToQueue (m);
}

FxChain::Settings PercProcessor::fxSettings() const
{
    FxChain::Settings s;
    const float sat = raw.sat->load(), rev = raw.rev->load();
    s.drive = sat;
    s.saturation = juce::jmin (1.f, sat / 100 * 1.5f);
    s.chorus = raw.cho->load() / 100;
    s.flangus = raw.fla->load() / 100;
    s.delay = raw.dly->load() / 100;
    s.delaySeconds = (float) (.75 * 60 / tempo); // dotted eighth, on the host tempo
    s.reverb = rev / 100;
    s.reverbSize = .25f + rev / 100 * .6f;
    s.gainDb = raw.vol->load();
    return s;
}

void PercProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (! licence->isUnlocked()) // pas encore activé (ou essai fini) : silence, notes en attente jetées
    {
        uiMidi.removeNextBlockOfMessages (midi, buffer.getNumSamples());
        midi.clear();
        return;
    }

    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
            if (auto bpm = position->getBpm())
                tempo = juce::jlimit (40.0, 240.0, *bpm);

    uiMidi.removeNextBlockOfMessages (midi, buffer.getNumSamples());
    if (const auto* sounds = bank.load())
    {
        for (const auto m : midi)
        {
            const auto message = m.getMessage();
            if (! message.isNoteOn()) continue;
            const int pad = padForNote (message.getNoteNumber());
            const auto slot = padSound (pad);
            play (*sounds, slot.category, slot.index, message.getFloatVelocity(), m.samplePosition);
            flash[pad].store (message.getFloatVelocity());
            // The buffer is in time order: the last note stored is the latest one played.
            hits.store ((juce::uint64) ++hitCount << 32 | (juce::uint64) message.getNoteNumber() << 16
                        | (juce::uint64) pad << 8 | (juce::uint64) message.getVelocity());
        }
        auto previews = [&] (int start, int count)
        {
            for (int k = 0; k < count; ++k)
            {
                const auto [code, velocity] = listPlays[(size_t) (start + k)];
                play (*sounds, code / maxSounds, code % maxSounds, velocity, 0);
            }
        };
        const auto scope = listQueue.read (listQueue.getNumReady());
        previews (scope.startIndex1, scope.blockSize1);
        previews (scope.startIndex2, scope.blockSize2);
    }

    engine.render (buffer);
    fx.process (buffer, fxSettings());
    midi.clear();
}

int PercProcessor::getNumPrograms() { return (int) presets.size(); }

const juce::String PercProcessor::getProgramName (int i)
{
    return presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, i)].name;
}

void PercProcessor::setCurrentProgram (int i)
{
    i = juce::jlimit (0, (int) presets.size() - 1, i);
    program.store (i);
    const auto& p = presets[(size_t) i];
    setValue ("sat", p.saturation);
    setValue ("cho", p.chorus);
    setValue ("fla", p.flangus);
    setValue ("dly", p.delay);
    setValue ("rev", p.reverb);
}

void PercProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    tree.setProperty ("program", program.load(), nullptr);
    tree.setProperty ("favouritesOnly", favouritesOnly(), nullptr);
    // The kit, by sound id (stays right if the sounds are renumbered in a later version).
    for (int pad = 0; pad < padsPerPage; ++pad)
    {
        const auto slot = padSound (pad);
        tree.setProperty ("pad" + juce::String (pad), juce::String (sampleLists[slot.category].entries[slot.index].id), nullptr);
    }
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void PercProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    program.store (tree.getProperty ("program", 1));
    state.replaceState (tree);
    onlyFavourites.store (tree.getProperty ("favouritesOnly", false));
    resetKit(); // projects saved before the kit existed keep the default kit
    for (int pad = 0; pad < padsPerPage; ++pad)
    {
        const auto id = tree.getProperty ("pad" + juce::String (pad)).toString();
        if (id.isEmpty()) continue;
        for (int c = 0; c < categoryCount; ++c)
            for (int i = 0; i < soundCount (c); ++i)
                if (id == sampleLists[c].entries[i].id) { setPadSound (pad, c, i); c = categoryCount; break; }
    }
}

juce::AudioProcessorEditor* PercProcessor::createEditor() { return new PercEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PercProcessor(); }
