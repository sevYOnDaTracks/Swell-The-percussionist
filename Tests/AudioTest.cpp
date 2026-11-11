#include <algorithm>
#include <set>
#include "../Source/PluginEditor.h"
#include <iostream>
#include <stdexcept>

static void require (bool ok, const juce::String& message)
{
    if (! ok) throw std::runtime_error (message.toStdString());
}

// Renders 'seconds' with an optional note at the start; returns the peak and the energy of both channels.
struct Result { float peak = 0; double energy = 0; };
static Result render (PercProcessor& p, double seconds, int note = -1, float velocity = 1.f)
{
    const int block = 256;
    juce::AudioBuffer<float> b (2, block);
    Result r;
    for (int start = 0; start < (int) (48000 * seconds); start += block)
    {
        juce::MidiBuffer m;
        if (start == 0 && note >= 0) m.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
        p.processBlock (b, m);
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < block; ++n)
            {
                const float x = b.getSample (c, n);
                require (std::isfinite (x), "non-finite output");
                r.peak = juce::jmax (r.peak, std::abs (x));
                r.energy += (double) x * x;
            }
    }
    return r;
}

static juce::File testFavourites()
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("swell-perc-test-favoris.txt");
}

static void ready (PercProcessor& p)
{
    testFavourites().deleteFile();
    p.setFavouritesFile (testFavourites()); // never touch the user's own favourites
    require (p.waitForSamples (30000), "samples did not load");
    p.prepareToPlay (48000, 256);
}

template <typename T>
static void collect (juce::Component& parent, std::vector<T*>& found)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* match = dynamic_cast<T*> (child)) found.push_back (match);
        collect (*child, found);
    }
}

static void click (juce::Component& c, float x, float y)
{
    const auto now = juce::Time::getCurrentTime();
    const juce::Point<float> at (x, y);
    c.mouseDown (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(), 1.f, 0, 0, 0, 0, &c, &c, now, at, now, 1, false));
}

int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI gui;
        swell::Licence::unlockForTesting(); // rendu hors ligne, sans fichier de licence

        // Every embedded sound decodes and plays from a pad, under full scale.
        {
            PercProcessor p;
            ready (p);
            p.setCurrentProgram (0);
            int total = 0;
            for (int c = 0; c < categoryCount; ++c)
            {
                require (PercProcessor::soundCount (c) > 0, "empty category");
                for (int i = 0; i < PercProcessor::soundCount (c); ++i)
                {
                    p.setPadSound (5, c, i);
                    const auto r = render (p, .05, firstPadNote + 5);
                    const auto name = juce::String::fromUTF8 (sampleLists[c].entries[i].name);
                    require (r.peak > .001f, juce::String::fromUTF8 (categories[c].label) + " / " + name + " is silent");
                    require (r.peak <= 1.f, name + " clips");
                    render (p, .3);
                    ++total;
                }
            }
            std::cout << total << " sounds play\n";
        }

        // Silence without input. Every MIDI note plays a pad (C1 = pad 1, the 16 pads repeat over the keyboard),
        // whatever tab the window shows.
        {
            PercProcessor p;
            ready (p);
            require (render (p, .3).peak == 0, "output without any hit");
            require (PercProcessor::padForNote (36) == 0 && PercProcessor::padForNote (51) == 15 && PercProcessor::padForNote (52) == 0
                     && PercProcessor::padForNote (35) == 15 && PercProcessor::padForNote (60) == 8 && PercProcessor::padForNote (0) == 12, "note to pad");
            for (int note : { 0, 24, 36, 48, 60, 72, 80, 127 })
            {
                render (p, 2);
                p.takeFlash (PercProcessor::padForNote (note));
                require (render (p, .3, note).peak > .001f, "note " + juce::String (note) + " is silent");
                require (p.takeFlash (PercProcessor::padForNote (note)) > 0, "note " + juce::String (note) + " did not light its pad");
            }
            // What the window shows is the note really played, the latest in time, named as the piano roll names it.
            require (PercProcessor::noteName (36) == "C1" && PercProcessor::noteName (60) == "C3", "note names (C3 = 60 outside FL Studio)");
            {
                juce::AudioBuffer<float> b (2, 256);
                juce::MidiBuffer m;
                m.addEvent (juce::MidiMessage::noteOn (10, 60, (juce::uint8) 100), 200); // later in the block, higher note
                m.addEvent (juce::MidiMessage::noteOn (10, 51, (juce::uint8) 90), 10);   // earlier, higher pad (16)
                const auto before = p.lastHit().count;
                p.processBlock (b, m);
                const auto hit = p.lastHit();
                require (hit.count == before + 2, "every note counts");
                require (hit.note == 60 && hit.pad == PercProcessor::padForNote (60) && hit.velocity == 100, "last hit is not the latest note");
            }
            const auto before = p.padSound (3);
            for (int c = 0; c < categoryCount; ++c) p.selectCategory (c);
            const auto after = p.padSound (3);
            require (before.category == after.category && before.index == after.index, "changing tab changed the kit");
            render (p, 2);
            render (p, .05, firstPadNote + 3);
            require (p.lastSound().category == before.category && p.lastSound().index == before.index, "pad plays another sound");
        }

        // The default kit mixes the tabs.
        {
            PercProcessor p;
            std::set<int> used;
            for (int pad = 0; pad < padsPerPage; ++pad) used.insert (p.padSound (pad).category);
            require (used.size() == (size_t) categoryCount, "default kit does not use every tab");
        }

        // Each effect knob changes the sound once turned up from zero.
        // Shakers: a bright sound well under the output limiter.
        for (int category : { 6 })
        for (const char* id : { "sat", "cho", "fla", "dly", "rev" })
        {
            PercProcessor a, b;
            for (auto* p : { &a, &b }) { ready (*p); p->setCurrentProgram (0); p->setPadSound (0, category, 0); }
            b.setValue (id, 80);
            render (a, .05);
            render (b, .05);
            const auto dry = render (a, 1.5, firstPadNote), wet = render (b, 1.5, firstPadNote);
            require (std::abs (wet.energy - dry.energy) > dry.energy * .01, juce::String (id) + " has no effect");
        }

        // Tune up plays faster (more zero crossings); a shorter decay ends sooner.
        {
            auto crossings = [] (float tune)
            {
                PercProcessor p;
                ready (p);
                p.setCurrentProgram (0);
                p.setPadSound (0, 4, 0);
                p.setValue ("tune", tune);
                juce::AudioBuffer<float> b (2, 8192);
                juce::MidiBuffer m;
                m.addEvent (juce::MidiMessage::noteOn (1, firstPadNote, 1.f), 0);
                p.processBlock (b, m);
                int count = 0;
                for (int n = 1; n < 8192; ++n)
                    if (b.getSample (0, n - 1) < 0 && b.getSample (0, n) >= 0) ++count;
                return count;
            };
            require (crossings (12) > crossings (0), "tune does not change the pitch");

            auto energy = [] (float decay)
            {
                PercProcessor p;
                ready (p);
                p.setCurrentProgram (0);
                p.setPadSound (0, 4, 0);
                p.setValue ("decay", decay);
                return render (p, 2, firstPadNote).energy;
            };
            require (energy (20) < energy (100), "decay does not change the length");
        }

        // Settings and the kit survive a save and reload.
        {
            PercProcessor a, b;
            a.selectCategory (5);
            a.setPadSound (7, 6, 42);
            a.setValue ("rev", 77);
            juce::MemoryBlock saved;
            a.getStateInformation (saved);
            b.setStateInformation (saved.getData(), (int) saved.getSize());
            require (b.category() == 5 && b.value ("rev") == 77, "state not restored");
            require (b.padSound (7).category == 6 && b.padSound (7).index == 42, "kit not restored");
            require (b.padSound (0).category == a.padSound (0).category && b.padSound (0).index == a.padSound (0).index, "default pads not restored");
        }

        // Clicks reach the processor: tabs, pads, keys; the list plays, and loads only on purpose.
        {
            PercProcessor p;
            ready (p);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            std::vector<Tab*> tabs;
            std::vector<Pad*> pads;
            std::vector<PercView*> views;
            std::vector<SoundList*> lists;
            collect (*e, tabs);
            collect (*e, pads);
            collect (*e, views);
            collect (*e, lists);
            require (tabs.size() == categoryCount && pads.size() == padsPerPage && views.size() == 1 && lists.size() == 1, "missing controls");
            for (int i = categoryCount - 1; i >= 0; --i)
            {
                click (*tabs[(size_t) i], 10, 10);
                require (p.category() == i && views[0]->shownCategory() == i, "tab does not change the category");
            }
            render (p, 1);
            click (*pads[3], 20, (float) pads[3]->getHeight() - 2);
            const auto loud = render (p, .4);
            require (loud.peak > .05f && views[0]->selectedPad() == 3, "pad is silent or not selected");
            require (p.lastSound().category == p.padSound (3).category && p.lastSound().index == p.padSound (3).index, "pad plays its sound");
            render (p, 2);
            click (*pads[3], 20, 0);
            require (render (p, .4).energy < loud.energy, "top of the pad is not softer");
            render (p, 2);

            // Browsing the list plays the sounds but never changes the kit (so never what the piano roll plays).
            click (*tabs[2], 10, 10);
            const auto kitBefore = p.padSound (3);
            for (int col = 0; col < 2; ++col) lists[0]->clickCell (4, col, false);
            require (render (p, .3).peak > .01f, "list click does not play");
            require (p.padSound (3).category == kitBefore.category && p.padSound (3).index == kitBefore.index, "list click changed the kit");
            render (p, 2);
            lists[0]->loadCell (2, 1); // row 2, column 1 = sound 5
            require (p.padSound (3).category == 2 && p.padSound (3).index == 5, "double-click does not load into the selected pad");
            require (render (p, .3).peak > .01f, "loading does not play");
            render (p, 2);
            // Dropping a sound onto a pad loads it there, and selects that pad.
            {
                juce::DragAndDropTarget::SourceDetails drop (SoundList::dragDescription (7, 2), lists[0], {});
                require (pads[10]->isInterestedInDragSource (drop), "pad refuses a sound");
                pads[10]->itemDropped (drop);
                require (p.padSound (10).category == 7 && p.padSound (10).index == 2 && views[0]->selectedPad() == 10, "drop does not load into the pad");
                require (p.padSound (3).category == 2 && p.padSound (3).index == 5, "drop changed another pad");
                views[0]->selectPad (3);
                render (p, 2);
            }
            render (p, .05, firstPadNote + 3);
            require (p.lastSound().category == 2 && p.lastSound().index == 5, "MIDI does not play the loaded sound");
            render (p, 2);
            require (views[0]->keyPressed (juce::KeyPress ('q', {}, 'q')), "key ignored");
            require (render (p, .4).peak > .05f && views[0]->selectedPad() == 8, "key does not play its pad");
            require (e->createComponentSnapshot (e->getLocalBounds()).isValid(), "editor did not draw");
        }

        // Soft and Clean is the second preset and the default settings.
        {
            PercProcessor p;
            require (p.getProgramName (1) == "Soft and Clean" && p.getCurrentProgram() == 1, "Soft and Clean is not the default preset");
            require (p.value ("sat") == 17 && p.value ("rev") == 2 && p.value ("cho") == 0 && p.value ("fla") == 0 && p.value ("dly") == 0,
                     "default settings differ from Soft and Clean");
            p.setCurrentProgram (2);
            p.setCurrentProgram (1);
            require (p.value ("sat") == 17 && p.value ("rev") == 2, "Soft and Clean preset values");
        }

        // Favourites: the star keeps a sound, Favoris shows only those in the list, and they are saved for the next session.
        {
            PercProcessor p;
            ready (p);
            p.selectCategory (5);
            require (p.favouriteCount (5) == 0 && p.visibleCount (5) == PercProcessor::soundCount (5), "starts without favourites");
            p.toggleFavourite (5, 40);
            p.toggleFavourite (5, 7);
            p.setFavouritesOnly (true);
            require (p.visibleCount (5) == 2, "favourites view count");
            require (p.visibleSound (5, 0) == 7 && p.visibleSound (5, 1) == 40 && p.visibleSound (5, 2) == -1, "favourites order");

            PercProcessor q;
            q.setFavouritesFile (testFavourites());
            require (q.isFavourite (5, 40) && q.isFavourite (5, 7) && q.favouriteCount (5) == 2, "favourites not saved");
            q.toggleFavourite (5, 40);
            PercProcessor r;
            r.setFavouritesFile (testFavourites());
            require (! r.isFavourite (5, 40) && r.favouriteCount (5) == 1, "removing a favourite not saved");

            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            PercProcessor s;
            s.setStateInformation (saved.getData(), (int) saved.getSize());
            require (s.favouritesOnly(), "favourites view not restored");
            testFavourites().deleteFile();
        }

        // The editor draws every category.
        {
            PercProcessor p;
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            for (int i = 0; i < categoryCount; ++i)
            {
                p.selectCategory (i);
                require (e->createComponentSnapshot (e->getLocalBounds()).isValid(), "editor did not draw");
            }
        }

        std::cout << "OK\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
