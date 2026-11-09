#include <algorithm>
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

        // Every embedded sound decodes and plays, under full scale.
        {
            PercProcessor p;
            ready (p);
            p.setCurrentProgram (0);
            int total = 0;
            for (int c = 0; c < categoryCount; ++c)
            {
                p.selectCategory (c);
                require (PercProcessor::soundCount (c) > 0, "empty category");
                for (int page = 0; page < p.pageCount (c); ++page)
                {
                    p.setPage (page);
                    for (int pad = 0; pad < padsPerPage && page * padsPerPage + pad < PercProcessor::soundCount (c); ++pad)
                    {
                        const auto r = render (p, .05, firstPadNote + pad);
                        const auto name = juce::String::fromUTF8 (sampleLists[c].entries[page * padsPerPage + pad].name);
                        require (r.peak > .001f, juce::String::fromUTF8 (categories[c].label) + " / " + name + " is silent");
                        require (r.peak <= 1.f, name + " clips");
                        render (p, .3);
                        ++total;
                    }
                }
            }
            std::cout << total << " sounds play\n";
            int listed = 0;
            for (int c = 0; c < categoryCount; ++c) listed += PercProcessor::soundCount (c);
            require (total == listed && total > 0, "sounds missing");
        }

        // Silence without input; notes outside the pads, and pads past the last sound, play nothing.
        {
            PercProcessor p;
            ready (p);
            require (render (p, .3).peak == 0, "output without any hit");
            require (render (p, .3, 80).peak == 0, "unmapped note played");
            const int c = 0, last = p.pageCount (c) - 1;
            p.selectCategory (c);
            p.setPage (last);
            const int onLastPage = PercProcessor::soundCount (c) - last * padsPerPage;
            if (onLastPage < padsPerPage)
                require (render (p, .3, firstPadNote + padsPerPage - 1).peak == 0, "empty pad played");
            p.setPage (999);
            require (p.page() == last, "page not limited");
        }

        // Each effect knob changes the sound once turned up from zero.
        // Shakers: a bright sound well under the output limiter.
        for (int category : { 6 })
        for (const char* id : { "sat", "cho", "fla", "dly", "rev" })
        {
            PercProcessor a, b;
            for (auto* p : { &a, &b }) { ready (*p); p->setCurrentProgram (0); p->selectCategory (category); }
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
                p.selectCategory (4);
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
                p.selectCategory (4);
                p.setValue ("decay", decay);
                return render (p, 2, firstPadNote).energy;
            };
            require (energy (20) < energy (100), "decay does not change the length");
        }

        // Settings survive a save and reload.
        {
            PercProcessor a, b;
            a.selectCategory (5);
            a.setPage (3);
            a.setValue ("rev", 77);
            juce::MemoryBlock saved;
            a.getStateInformation (saved);
            b.setStateInformation (saved.getData(), (int) saved.getSize());
            require (b.category() == 5 && b.page() == 3 && b.value ("rev") == 77, "state not restored");
        }

        // Clicks reach the processor: tabs, page buttons, pads and keys.
        {
            PercProcessor p;
            ready (p);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            std::vector<Tab*> tabs;
            std::vector<Pad*> pads;
            std::vector<PercView*> views;
            std::vector<juce::TextButton*> buttons;
            collect (*e, tabs);
            collect (*e, pads);
            collect (*e, views);
            collect (*e, buttons);
            // Les boutons de l'écran d'activation (licence) ne font pas partie de l'interface du plugin.
            buttons.erase (std::remove_if (buttons.begin(), buttons.end(), [] (juce::TextButton* b) { return b->findParentComponentOfClass<swell::LicencePanel>() != nullptr; }), buttons.end());
            require (tabs.size() == categoryCount && pads.size() == padsPerPage && views.size() == 1 && buttons.size() == 4, "missing controls");
            juce::TextButton* prevButton = nullptr; juce::TextButton* nextButton = nullptr;
            for (auto* b : buttons) { if (b->getButtonText() == "<") prevButton = b; if (b->getButtonText() == ">") nextButton = b; }
            for (int i = categoryCount - 1; i >= 0; --i)
            {
                click (*tabs[(size_t) i], 10, 10);
                require (p.category() == i && views[0]->shownCategory() == i && p.page() == 0, "tab does not change the category");
            }
            click (*tabs[5], 10, 10);
            nextButton->onClick();
            require (p.page() == 1 && views[0]->shownPage() == 1, "next page button");
            prevButton->onClick();
            require (p.page() == 0, "previous page button");
            render (p, 1);
            click (*pads[3], 20, 50);
            const auto loud = render (p, .4);
            require (loud.peak > .05f && p.lastSound() == 3, "pad is silent");
            render (p, 2);
            click (*pads[3], 20, 0);
            require (render (p, .4).energy < loud.energy, "top of the pad is not softer");
            render (p, 2);
            require (views[0]->keyPressed (juce::KeyPress ('q', {}, 'q')), "key ignored");
            require (render (p, .4).peak > .05f && p.lastSound() == 8, "key does not play its pad");
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

        // Favourites: the star keeps a sound, Favoris shows only those, and they are saved for the next session.
        {
            PercProcessor p;
            ready (p);
            p.selectCategory (5);
            require (p.favouriteCount (5) == 0 && p.visibleCount (5) == PercProcessor::soundCount (5), "starts without favourites");
            p.toggleFavourite (5, 40);
            p.toggleFavourite (5, 7);
            p.setFavouritesOnly (true);
            require (p.visibleCount (5) == 2 && p.pageCount (5) == 1, "favourites view count");
            require (p.visibleSound (5, 0) == 7 && p.visibleSound (5, 1) == 40 && p.visibleSound (5, 2) == -1, "favourites order");
            render (p, .05, firstPadNote + 1);
            require (p.lastSound() == 40, "pad 2 of the favourites view plays the second favourite");
            p.takeFlash (2);
            render (p, .1, firstPadNote + 2);
            require (p.takeFlash (2) == 0 && p.lastSound() == 40, "pad past the favourites plays nothing");

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
        }

        // Editor: star on a pad, Favoris button, list view (play and star from the list).
        {
            PercProcessor p;
            ready (p);
            std::unique_ptr<juce::AudioProcessorEditor> e (p.createEditor());
            std::vector<Pad*> pads;
            std::vector<PercView*> views;
            std::vector<SoundList*> lists;
            std::vector<juce::TextButton*> buttons;
            collect (*e, pads);
            collect (*e, views);
            collect (*e, lists);
            collect (*e, buttons);
            auto button = [&] (const juce::String& text)
            {
                for (auto* b : buttons) if (b->getButtonText().contains (text)) return b;
                throw std::runtime_error (("no button " + text).toStdString());
            };
            views[0]->selectCategory (2);
            click (*pads[3], (float) pads[3]->getWidth() - 14, 14);
            require (p.isFavourite (2, 3), "star on a pad");
            render (p, .3);
            require (render (p, .1).peak == 0, "the star played the pad");
            auto* fav = button ("Favoris");
            fav->setToggleState (true, juce::dontSendNotification);
            fav->onClick();
            require (p.favouritesOnly() && pads[0]->sound == 3 && pads[1]->sound < 0, "Favoris button");
            fav->setToggleState (false, juce::dontSendNotification);
            fav->onClick();
            require (! p.favouritesOnly() && pads[1]->sound == 1, "Favoris button off");

            auto* listButton = button ("Liste");
            listButton->setToggleState (true, juce::dontSendNotification);
            listButton->onClick();
            require (views[0]->listShown() && ! pads[0]->isVisible(), "list view");
            lists[0]->clickCell (2, 1, false);   // row 2, column 1 = sound 9
            require (render (p, .3).peak > .01f && p.lastSound() == 9, "list click plays");
            lists[0]->clickCell (2, 1, true);
            require (p.isFavourite (2, 9), "list star");
            require (e->createComponentSnapshot (e->getLocalBounds()).isValid(), "list view did not draw");
            listButton->setToggleState (false, juce::dontSendNotification);
            listButton->onClick();
            require (! views[0]->listShown() && pads[0]->isVisible(), "back to pads");
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
