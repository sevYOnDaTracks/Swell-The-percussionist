#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
constexpr int heroHeight = 410, margin = 32, padsY = 416, sonY = 568;

const juce::Colour bg { 0xff0b0b0c }, ink { 0xffededee }, soft { 0xffd9d9dc }, muted { 0xff8e8e94 }, faint { 0xff6e6e74 },
                   rule { 0xff1f1f22 }, track { 0xff26262a }, orange { 0xffff8a3d };

juce::String text (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// toUpperCase leaves accented letters as they are.
juce::String upper (const juce::String& s)
{
    return s.toUpperCase().replace (text ("\xc3\xa9"), text ("\xc3\x89")).replace (text ("\xc3\xb3"), text ("\xc3\x93"));
}

void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> area, float size, juce::Colour colour,
            juce::Justification j = juce::Justification::centredLeft, bool bold = false, float tracking = 0)
{
    g.setColour (colour);
    g.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (tracking));
    g.drawText (s, area, j, true);
}

juce::Image loadPhoto (int i)
{
    int size = 0;
    const char* data = BinaryData::getNamedResource (categories[i].photo, size);
    auto image = juce::ImageCache::getFromMemory (data, size).createCopy();
    image.desaturate();
    return image;
}

juce::String soundName (int category, int index) { return text (sampleLists[category].entries[index].name); }

void star (juce::Graphics& g, juce::Rectangle<float> area, bool filled, juce::Colour colour)
{
    juce::Path s;
    s.addStar (area.getCentre(), 5, area.getWidth() * .2f, area.getWidth() * .45f, 0);
    g.setColour (colour);
    if (filled) g.fillPath (s);
    else g.strokePath (s, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::String percent (float v) { return v < .5f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " %"; }
}

// ---------------------------------------------------------------- Look

PercLook::PercLook()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xb80b0b0c));
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, orange);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141416));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
    setColour (juce::PopupMenu::highlightedTextColourId, bg);
    setColour (juce::TextButton::textColourOffId, soft);
    setColour (juce::TextButton::textColourOnId, bg);
}

void PercLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size / 2 - 4;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (start + end) / 2 : start;
    const bool off = ! bipolar && pos < .005f;

    juce::Path back, arc;
    back.addCentredArc (c.x, c.y, radius, radius, 0, start, end, true);
    arc.addCentredArc (c.x, c.y, radius, radius, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (track);
    g.strokePath (back, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (off ? juce::Colour (0xff5a5a60) : orange);
    g.strokePath (arc, juce::PathStrokeType (3, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto dot = c.getPointOnCircumference (radius, angle);
    g.fillEllipse (dot.x - 4, dot.y - 4, 8, 8);
}

void PercLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRect (0, 0, w, h);
    g.setColour (orange);
    g.fillRect (0, h - 1, w, 1);
    juce::Path arrow;
    arrow.addTriangle ((float) w - 16, (float) h / 2 - 2, (float) w - 8, (float) h / 2 - 2, (float) w - 12, (float) h / 2 + 3);
    g.fillPath (arrow);
}

void PercLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (.5f);
    if (b.getToggleState())
    {
        g.setColour (orange);
        g.fillRoundedRectangle (r, 4);
        return;
    }
    g.setColour (down ? orange.withAlpha (.3f) : over ? juce::Colour (0x18ffffff) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, 4);
    g.setColour (b.isEnabled() ? juce::Colour (0xff3a3a40) : juce::Colour (0xff222226));
    g.drawRoundedRectangle (r, 4, 1);
}

// ---------------------------------------------------------------- Knob

Knob::Knob (PercProcessor& p, const char* id, juce::String c, std::function<juce::String (float)> f, bool bipolar)
    : caption (std::move (c)), format (std::move (f))
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (150);
    slider.getProperties().set ("bipolar", bipolar);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.state, id, slider);
    auto* param = p.state.getParameter (id);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::resized() { slider.setBounds (getLocalBounds().withHeight (56).withSizeKeepingCentre (56, 56)); }

void Knob::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    label (g, caption, { 0, 60, w, 12 }, 9.5f, faint, juce::Justification::centred, false, .14f);
    label (g, format ((float) slider.getValue()), { 0, 76, w, 16 }, 12.5f, soft, juce::Justification::centred);
}

// ---------------------------------------------------------------- Pad, Tab

Pad::Pad (PercView& v, int s) : view (v), slot (s) {}

void Pad::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (.5f);
    if (name.isEmpty())
    {
        g.setColour (juce::Colour (0xff111113));
        g.fillRoundedRectangle (r, 5);
        return;
    }
    const bool lit = heat > .5f;
    g.setColour (juce::Colour (0xff161618).interpolatedWith (orange, heat));
    g.fillRoundedRectangle (r, 5);
    g.setColour (lit ? orange : juce::Colour (0xff242428));
    g.drawRoundedRectangle (r, 5, 1);
    label (g, name, { 9, 8, r.getWidth() - 31, 18 }, 11.5f, lit ? bg : ink, juce::Justification::centredLeft, true);
    star (g, starArea().reduced (3), favourite, lit ? bg : favourite ? orange : juce::Colour (0xff4a4a50));
    label (g, juce::String::charToString ((juce::juce_wchar) padKeys[slot]).toUpperCase(), { 10, r.getHeight() - 20, 20, 12 }, 9.5f, lit ? bg : faint);
}

void Pad::mouseDown (const juce::MouseEvent& e)
{
    if (sound < 0) return;
    if (starArea().expanded (4).contains (e.position))
        view.toggleFavourite (sound);
    else
        view.play (slot, juce::jlimit (.25f, 1.f, .25f + .75f * (float) e.y / (float) getHeight()));
}

Tab::Tab (PercView& v, int i, juce::Image p) : view (v), index (i), photo (std::move (p)) {}

void Tab::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (selected ? juce::Colour (0xe6241c16) : juce::Colour (0xcc141416));
    g.fillRect (r);
    {
        juce::Graphics::ScopedSaveState save (g);
        const auto thumb = r.withWidth (34).withTrimmedBottom (2);
        g.reduceClipRegion (thumb.toNearestInt());
        g.setOpacity (selected ? 1.f : .7f);
        g.drawImage (photo, thumb, juce::RectanglePlacement::fillDestination);
    }
    g.setColour (selected ? orange : juce::Colour (0xff2a2a2e));
    g.fillRect (r.withTop (r.getBottom() - 2));
    const auto colour = selected ? orange : isMouseOver() ? ink : muted;
    label (g, upper (text (categories[index].label)), r.withTrimmedLeft (42).withTrimmedRight (4), 10.5f, colour,
           juce::Justification::centredLeft, true, .08f);
}

void Tab::mouseDown (const juce::MouseEvent&) { view.selectCategory (index); }

// ---------------------------------------------------------------- Sound list

SoundList::SoundList (PercView& v, PercProcessor& p) : view (v), processor (p)
{
    box.setModel (this);
    box.setRowHeight (30);
    box.setColour (juce::ListBox::backgroundColourId, bg);
    box.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    box.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff3a3a40));
    box.setWantsKeyboardFocus (false);
    addAndMakeVisible (box);
}

int SoundList::getNumRows()
{
    return (processor.visibleCount (processor.category()) + columns - 1) / columns;
}

void SoundList::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool)
{
    const int c = processor.category();
    const float cell = (float) width / columns;
    for (int col = 0; col < columns; ++col)
    {
        const int index = processor.visibleSound (c, row * columns + col);
        if (index < 0) continue;
        const juce::Rectangle<float> r ((float) col * cell, 1, cell - 6, (float) height - 2);
        const bool last = index == processor.lastSound();
        g.setColour (last ? juce::Colour (0xff241c16) : juce::Colour (0xff131315));
        g.fillRoundedRectangle (r, 3);
        star (g, r.withWidth (26).reduced (6), processor.isFavourite (c, index), processor.isFavourite (c, index) ? orange : juce::Colour (0xff4a4a50));
        label (g, soundName (c, index), r.withTrimmedLeft (28).withTrimmedRight (6), 12, last ? orange : ink);
    }
}

void SoundList::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const float cell = (float) box.getVisibleRowWidth() / columns;
    const int col = juce::jlimit (0, columns - 1, (int) ((float) e.x / cell));
    clickCell (row, col, (float) e.x - (float) col * cell < 28);
}

void SoundList::clickCell (int row, int column, bool onStar)
{
    const int index = processor.visibleSound (processor.category(), row * columns + column);
    if (index < 0) return;
    if (onStar) view.toggleFavourite (index);
    else view.playSound (index);
    box.repaint();
}

// ---------------------------------------------------------------- Window

PercView::PercView (PercProcessor& p)
    : processor (p), list (*this, p), licenceBadge (faint, orange, juce::Justification::centred),
      licencePanel ({ bg, juce::Colour (0xff141416), ink, muted, orange, juce::Colour (0xffff6b5b) })
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (true);

    for (int i = 0; i < categoryCount; ++i)
    {
        photos[(size_t) i] = loadPhoto (i);
        addAndMakeVisible (tabs.add (new Tab (*this, i, photos[(size_t) i])));
    }
    for (int i = 0; i < padsPerPage; ++i)
        addAndMakeVisible (pads.add (new Pad (*this, i)));

    for (auto* b : { &previous, &next })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    previous.onClick = [this] { turnPage (-1); };
    next.onClick = [this] { turnPage (1); };

    favouritesButton.setButtonText (text ("\xe2\x98\x85 Favoris"));
    listButton.setButtonText ("Liste");
    for (auto* b : { &favouritesButton, &listButton })
    {
        b->setClickingTogglesState (true);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }
    favouritesButton.setToggleState (p.favouritesOnly(), juce::dontSendNotification);
    favouritesButton.onClick = [this] { setFavouritesOnly (favouritesButton.getToggleState()); };
    listButton.onClick = [this] { setListShown (listButton.getToggleState()); };
    addChildComponent (list);

    for (int i = 0; i < p.getNumPrograms(); ++i)
        preset.addItem (p.getProgramName (i), i + 1);
    preset.setSelectedId (p.getCurrentProgram() + 1, juce::dontSendNotification);
    preset.setWantsKeyboardFocus (false);
    preset.onChange = [this] { processor.setCurrentProgram (preset.getSelectedId() - 1); };
    addAndMakeVisible (preset);

    auto signedValue = [] (const char* unit, int decimals)
    {
        return [=] (float v) { return (v > 0 ? "+" : "") + juce::String (v, decimals) + unit; };
    };
    knobs.add (new Knob (p, "tune", "TUNE", signedValue (" dt", 0), true));
    knobs.add (new Knob (p, "decay", "DECAY", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }));
    knobs.add (new Knob (p, "sat", "SATURATION", percent));
    knobs.add (new Knob (p, "cho", "CHORUS", percent));
    knobs.add (new Knob (p, "fla", "FLANGUS", percent));
    knobs.add (new Knob (p, "dly", "DELAY", percent));
    knobs.add (new Knob (p, "rev", "REVERB", percent));
    knobs.add (new Knob (p, "vol", "VOLUME", signedValue (" dB", 1), true));
    for (auto* k : knobs) addAndMakeVisible (k);
    addAndMakeVisible (licenceBadge);
    addChildComponent (licencePanel); // shows itself when there is no valid licence

    setSize (width, height);
    refresh();
    startTimerHz (60);
}

PercView::~PercView() { setLookAndFeel (nullptr); }

void PercView::resized()
{
    preset.setBounds (width - margin - 160, 22, 160, 30);

    const float tabWidth = ((float) width - margin * 2 - 6 * (categoryCount - 1)) / categoryCount;
    for (int i = 0; i < categoryCount; ++i)
        tabs[i]->setBounds (margin + juce::roundToInt ((float) i * (tabWidth + 6)), 352, juce::roundToInt (tabWidth), 44);

    next.setBounds (width - margin - 28, padsY - 4, 28, 22);
    previous.setBounds (width - margin - 28 - 110, padsY - 4, 28, 22);
    listButton.setBounds (previous.getX() - 16 - 64, padsY - 4, 64, 22);
    favouritesButton.setBounds (listButton.getX() - 8 - 86, padsY - 4, 86, 22);
    list.setBounds (margin, padsY + 26, width - margin * 2, height - 34 - (padsY + 26));

    const int columns = padsPerPage / 2;
    const float padWidth = ((float) width - margin * 2 - 8 * (columns - 1)) / columns;
    for (int i = 0; i < padsPerPage; ++i)
        pads[i]->setBounds (margin + juce::roundToInt ((float) (i % columns) * (padWidth + 8)), padsY + 26 + (i / columns) * 60,
                            juce::roundToInt (padWidth), 54);

    const float slot = (float) (width - margin * 2) / (float) knobs.size();
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (margin + juce::roundToInt (((float) i + .5f) * slot) - 50, sonY + 28, 100, 94);
    licenceBadge.setBounds (width / 2 - 180, height - 17, 360, 14); // milieu : les coins ont déjà la photo et la version
    licencePanel.setBounds (getLocalBounds());
}

// Brings the tabs, pads and texts in line with the processor (category, page, samples loaded).
void PercView::refresh()
{
    const bool categoryChanged = processor.category() != category;
    category = processor.category();
    page = processor.page();
    ready = processor.samplesReady();

    for (int i = 0; i < categoryCount; ++i)
    {
        tabs[i]->selected = i == category;
        tabs[i]->repaint();
    }
    shownFavourites = processor.favouriteCount (category);
    const int first = page * padsPerPage;
    for (int i = 0; i < padsPerPage; ++i)
    {
        auto& pad = *pads[i];
        pad.sound = processor.visibleSound (category, first + i);
        pad.name = pad.sound >= 0 ? soundName (category, pad.sound) : juce::String();
        pad.favourite = pad.sound >= 0 && processor.isFavourite (category, pad.sound);
        pad.repaint();
    }
    previous.setEnabled (page > 0);
    next.setEnabled (page < processor.pageCount (category) - 1);
    favouritesButton.setToggleState (processor.favouritesOnly(), juce::dontSendNotification);
    list.update();
    if (categoryChanged || lastShown < 0) showSound (juce::jmax (0, pads[0]->sound), -1);
    repaint();
}

void PercView::selectCategory (int c)
{
    processor.selectCategory (c);
    refresh();
}

void PercView::turnPage (int delta)
{
    processor.setPage (page + delta);
    refresh();
}

void PercView::play (int pad, float velocity) { processor.playPad (pad, velocity); }

void PercView::playSound (int index)
{
    processor.playSound (index, .85f);
    showSound (index, -1);
}

void PercView::toggleFavourite (int index)
{
    processor.toggleFavourite (category, index);
    refresh();
}

void PercView::setFavouritesOnly (bool on)
{
    processor.setFavouritesOnly (on);
    refresh();
}

void PercView::setListShown (bool on)
{
    listButton.setToggleState (on, juce::dontSendNotification);
    list.setVisible (on);
    for (auto* p : pads) p->setVisible (! on);
    for (auto* k : knobs) k->setVisible (! on);
    previous.setVisible (! on);
    next.setVisible (! on);
    refresh();
}

void PercView::showSound (int index, int velocity)
{
    lastShown = index;
    bigName = soundName (category, index);
    subtitle = text (categories[category].label) + "  " + juce::String (index + 1) + " / " + juce::String (PercProcessor::soundCount (category));
    if (velocity >= 0) subtitle << text ("  \xc2\xb7  ") << velocity;
    repaint (margin, 90, 460, 140);
}

void PercView::timerCallback()
{
    if (processor.category() != category || processor.page() != page || processor.samplesReady() != ready
        || processor.favouriteCount (processor.category()) != shownFavourites || processor.favouritesOnly() != favouritesButton.getToggleState())
        refresh();

    if (preset.getSelectedId() != processor.getCurrentProgram() + 1)
        preset.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    for (int i = 0; i < padsPerPage; ++i)
    {
        const float hit = processor.takeFlash (i);
        if (hit > 0) { showSound (processor.lastSound(), juce::roundToInt (hit * 127)); list.update(); }
        pads[i]->setHeat (hit > 0 ? 1.f : (pads[i]->getHeat() < .02f ? 0.f : pads[i]->getHeat() * .8f));
    }
}

bool PercView::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey) { turnPage (-1); return true; }
    if (key == juce::KeyPress::rightKey) { turnPage (1); return true; }
    const int i = juce::String (padKeys).indexOfChar (juce::CharacterFunctions::toLowerCase (key.getTextCharacter()));
    if (i < 0 || pads[i]->sound < 0) return false;
    play (i, .85f);
    return true;
}

void PercView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    const float W = width;
    if (category < 0) return;
    const auto& cat = categories[category];

    // Photo
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (0, 0, width, heroHeight);
        const auto& image = photos[(size_t) category];
        const float h = cat.photoWidth * (float) image.getHeight() / (float) image.getWidth();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image, { cat.photoX, cat.photoY, cat.photoWidth, h }, juce::RectanglePlacement::stretchToFit);
        g.setColour (juce::Colours::black.withAlpha (.2f));
        g.fillRect (0, 0, width, heroHeight);

        // Fully opaque up to the left edge of the photos, so that edge never shows.
        juce::ColourGradient side (bg, 0, 0, bg.withAlpha (0.f), W * .62f, 0, false);
        side.addColour (.325 / .62, bg);
        side.addColour (.46 / .62, bg.withAlpha (.5f));
        g.setGradientFill (side);
        g.fillRect (0, 0, width, heroHeight);

        juce::ColourGradient bottom (bg, 0, heroHeight, bg.withAlpha (0.f), 0, heroHeight - 150, false);
        bottom.addColour (.35, bg);
        g.setGradientFill (bottom);
        g.fillRect (0, 0, width, heroHeight);

        juce::ColourGradient top (bg.withAlpha (.8f), 0, 0, bg.withAlpha (0.f), 0, 90, false);
        g.setGradientFill (top);
        g.fillRect (0, 0, width, heroHeight);
    }

    // Header
    auto mark = juce::Drawable::parseSVGPath ("M7 23c3.5-7 7-7 12 0s8.5 7 12 0M7 15c3.5-7 7-7 12 0s8.5 7 12 0");
    g.setColour (orange);
    g.strokePath (mark, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::scale (30.f / 38).translated ((float) margin, 22));
    label (g, "swell", { (float) margin + 38, 22, 80, 30 }, 21, ink);
    label (g, "THE PERCUSSIONIST", { (float) margin + 118, 22, 240, 30 }, 11, ink, juce::Justification::centredLeft, true, .3f);

    // Last sound played
    label (g, upper (text (cat.label)), { (float) margin, 104, 300, 12 }, 9, faint, juce::Justification::centredLeft, false, .2f);
    label (g, bigName, { (float) margin - 2, 124, 460, 76 }, 46, juce::Colours::white);
    label (g, ready ? subtitle : text ("Chargement des sons\xe2\x80\xa6"), { (float) margin, 204, 380, 20 }, 14, ready ? soft : orange);

    // Sections
    auto section = [&] (const juce::String& name, int y, float ruleEnd)
    {
        label (g, name, { (float) margin, (float) y, 90, 14 }, 9, faint, juce::Justification::centredLeft, false, .2f);
        g.setColour (rule);
        g.fillRect ((float) margin + 60, (float) y + 7, ruleEnd - margin - 60, 1.f);
    };
    section ("SONS", padsY, (float) favouritesButton.getX() - 14);
    if (! listShown())
    {
        label (g, "Page " + juce::String (page + 1) + " / " + juce::String (processor.pageCount (category)),
               { (float) previous.getRight(), (float) padsY - 4, (float) (next.getX() - previous.getRight()), 22 }, 12, soft, juce::Justification::centred);
        section ("SON", sonY, W - margin);
    }
    else
    {
        label (g, juce::String (processor.visibleCount (category)) + " sons", { (float) previous.getX(), (float) padsY - 4, 138, 22 }, 12, soft, juce::Justification::centred);
    }
    if (processor.favouritesOnly() && processor.favouriteCount (category) == 0)
        label (g, text ("Aucun favori dans cet onglet : touche l'\xc3\xa9toile d'un son pour l'ajouter, ou d\xc3\xa9sactive \xe2\x98\x85 Favoris."),
               { (float) margin, (float) padsY + 40, W - margin * 2, 40 }, 13, muted, juce::Justification::centred);

    label (g, text ("Photo provisoire : ") + text (cat.credit), { (float) margin, (float) height - 26, 500, 14 }, 10, faint);
    label (g, "Swell-audio / 0.3 / by @sevyondatracks", { W - margin - 300, (float) height - 26, 300, 14 }, 10, faint, juce::Justification::centredRight);
}

// ---------------------------------------------------------------- Editor

PercEditor::PercEditor (PercProcessor& p) : AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (PercView::width * 6 / 10, PercView::height * 6 / 10, PercView::width * 5 / 4, PercView::height * 5 / 4);
    getConstrainer()->setFixedAspectRatio ((double) PercView::width / PercView::height);
    setSize (PercView::width * 85 / 100, PercView::height * 85 / 100);
}

void PercEditor::resized()
{
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / PercView::width));
    view.setBounds (0, 0, PercView::width, PercView::height);
}
