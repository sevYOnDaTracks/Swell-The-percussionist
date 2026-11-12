#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
constexpr int heroHeight = 300, margin = 32, tabsY = 248, sectionY = 308, contentY = 330, splitX = 568, knobsY = 588;

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

// The note of a pad, named as the host's piano roll names it (C1 in Ableton or Logic, C2 in Reaper, C3 in FL Studio).
juce::String padNote (int pad) { return PercProcessor::noteName (firstPadNote + pad); }

// -1 if the name is not one of the ready-made kits (saving under that name would hide it).
int builtInKitIndex (const juce::String& name)
{
    for (int k = 0; k < builtInKitCount; ++k)
        if (PercProcessor::kitName (k) == name) return k;
    return -1;
}

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
    // A dark base so the buttons stay readable over the photo.
    g.setColour (juce::Colour (0xb80b0b0c));
    g.fillRoundedRectangle (r, 4);
    g.setColour (down ? orange.withAlpha (.3f) : over ? juce::Colour (0x18ffffff) : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (r, 4);
    g.setColour (b.isEnabled() ? juce::Colour (over ? 0xff6a6a70 : 0xff4a4a50) : juce::Colour (0xff222226));
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

Pad::Pad (PercView& v, int p) : view (v), pad (p) {}

void Pad::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1);
    const bool lit = heat > .5f;
    g.setColour (juce::Colour (0xff161618).interpolatedWith (orange, heat));
    g.fillRoundedRectangle (r, 6);
    g.setColour (selected || lit || dropHover ? orange : juce::Colour (0xff26262a));
    g.drawRoundedRectangle (r, 6, dropHover ? 3.f : selected ? 2.f : 1.f);
    const float w = r.getWidth(), h = r.getHeight();
    label (g, juce::String (pad + 1), { 10, 7, 30, 14 }, 10, lit ? bg : selected ? orange : faint, juce::Justification::centredLeft, true);
    label (g, padNote (pad), { w - 50, 7, 40, 14 }, 10, lit ? bg : faint, juce::Justification::centredRight);
    label (g, name, { 10, h / 2 - 12, w - 18, 18 }, 12.5f, lit ? bg : empty ? faint : ink, juce::Justification::centredLeft, true);
    label (g, group, { 10, h / 2 + 6, w - 18, 14 }, 10, lit ? bg : muted);
    if (showKey)
        label (g, juce::String::charToString ((juce::juce_wchar) padKeys[pad]).toUpperCase(), { w - 30, h - 20, 20, 14 }, 9.5f,
               lit ? bg : juce::Colour (0xff4a4a50), juce::Justification::centredRight);
}

void Pad::mouseDown (const juce::MouseEvent& e)
{
    view.selectPad (pad);
    view.play (pad, juce::jlimit (.25f, 1.f, .25f + .75f * (float) e.y / (float) getHeight()));
}

bool Pad::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("sound:"); }

void Pad::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int code = d.description.toString().fromFirstOccurrenceOf ("sound:", false, false).getIntValue();
    view.loadSoundInto (pad, code / PercProcessor::maxSounds, code % PercProcessor::maxSounds);
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
    const auto inPad = processor.padSound (view.selectedPad());
    for (int col = 0; col < columns; ++col)
    {
        const int index = processor.visibleSound (c, row * columns + col);
        if (index < 0) continue;
        const juce::Rectangle<float> r ((float) col * cell, 1, cell - 6, (float) height - 2);
        const bool loaded = inPad.category == c && inPad.index == index;
        g.setColour (loaded ? juce::Colour (0xff241c16) : juce::Colour (0xff131315));
        g.fillRoundedRectangle (r, 3);
        const bool fav = processor.isFavourite (c, index);
        star (g, r.withWidth (26).reduced (6), fav, fav ? orange : juce::Colour (0xff4a4a50));
        label (g, soundName (c, index), r.withTrimmedLeft (28).withTrimmedRight (6), 12, loaded ? orange : ink);
    }
}

int SoundList::columnAt (float x) const
{
    return juce::jlimit (0, columns - 1, (int) (x / ((float) box.getVisibleRowWidth() / columns)));
}

void SoundList::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    const int col = columnAt ((float) e.x);
    clickCell (row, col, (float) e.x - (float) col * ((float) box.getVisibleRowWidth() / columns) < 28);
}

void SoundList::listBoxItemDoubleClicked (int row, const juce::MouseEvent& e)
{
    const int col = columnAt ((float) e.x);
    if ((float) e.x - (float) col * ((float) box.getVisibleRowWidth() / columns) >= 28) loadCell (row, col);
}

void SoundList::clickCell (int row, int column, bool onStar)
{
    const int index = processor.visibleSound (processor.category(), row * columns + column);
    if (index < 0) return;
    if (onStar) view.toggleFavourite (index);
    else if (juce::Time::getMillisecondCounterHiRes() - lastLoad > 400) view.previewSound (index);
    box.repaint();
}

void SoundList::loadCell (int row, int column)
{
    const int index = processor.visibleSound (processor.category(), row * columns + column);
    if (index < 0) return;
    lastLoad = juce::Time::getMillisecondCounterHiRes();
    view.loadSound (index);
    box.repaint();
}

juce::var SoundList::getDragSourceDescription (const juce::SparseSet<int>& rows)
{
    if (rows.isEmpty()) return {};
    // The column where the drag started.
    const auto down = box.getLocalPoint (nullptr, juce::Desktop::getInstance().getMainMouseSource().getLastMouseDownPosition());
    const int index = processor.visibleSound (processor.category(), rows[0] * columns + columnAt (down.x));
    return index < 0 ? juce::var() : dragDescription (processor.category(), index);
}

// ---------------------------------------------------------------- Prompt, Tutorial

namespace
{
void styleButton (juce::TextButton& b, bool primary)
{
    b.setWantsKeyboardFocus (false);
    b.setClickingTogglesState (false);
    b.setToggleState (primary, juce::dontSendNotification); // the look draws a toggled button in orange
}

// Draws text on several lines, as many as it needs.
void paragraph (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> area, float size, juce::Colour colour)
{
    juce::AttributedString a;
    a.append (s, juce::FontOptions (size), colour);
    a.setLineSpacing (3);
    a.setWordWrap (juce::AttributedString::byWord);
    a.draw (g, area);
}
}

Prompt::Prompt()
{
    styleButton (ok, true);
    styleButton (cancel, false);
    cancel.setButtonText ("Annuler");
    ok.onClick = [this] { accept(); };
    cancel.onClick = [this] { setVisible (false); };
    field.setFont (juce::FontOptions (15.f));
    field.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0b0c));
    field.setColour (juce::TextEditor::textColourId, ink);
    field.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3a40));
    field.setColour (juce::TextEditor::focusedOutlineColourId, orange);
    field.setColour (juce::TextEditor::highlightColourId, orange.withAlpha (.4f));
    field.setColour (juce::CaretComponent::caretColourId, orange);
    field.setIndents (10, 7);
    field.onReturnKey = [this] { accept(); };
    field.onEscapeKey = [this] { setVisible (false); };
    addAndMakeVisible (ok);
    addAndMakeVisible (cancel);
    addChildComponent (field);
}

void Prompt::ask (const juce::String& t, const juce::String& m, const juce::String& okText, bool wantsField,
                  const juce::String& initial, std::function<void (const juce::String&)> callback)
{
    title = t;
    message = m;
    withField = wantsField;
    onOk = std::move (callback);
    ok.setButtonText (okText);
    field.setVisible (withField);
    field.setText (initial, false);
    resized();
    setVisible (true);
    toFront (false);
    if (withField) { field.selectAll(); field.grabKeyboardFocus(); }
    repaint();
}

void Prompt::accept()
{
    const auto value = field.getText().trim();
    if (withField && value.isEmpty()) return;
    setVisible (false);
    if (auto f = std::move (onOk)) f (value);
}

static juce::Rectangle<int> promptBox (juce::Rectangle<int> bounds, bool withField)
{
    return bounds.withSizeKeepingCentre (440, withField ? 232 : 196);
}

void Prompt::resized()
{
    const auto box = promptBox (getLocalBounds(), withField);
    field.setBounds (box.getX() + 24, box.getBottom() - 112, box.getWidth() - 48, 36);
    ok.setBounds (box.getRight() - 24 - 120, box.getBottom() - 24 - 32, 120, 32);
    cancel.setBounds (ok.getX() - 10 - 100, ok.getY(), 100, 32);
}

void Prompt::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (.65f));
    const auto box = promptBox (getLocalBounds(), withField).toFloat();
    g.setColour (juce::Colour (0xff141416));
    g.fillRoundedRectangle (box, 10);
    g.setColour (orange);
    g.drawRoundedRectangle (box.reduced (.5f), 10, 1.5f);
    label (g, title, { box.getX() + 24, box.getY() + 20, box.getWidth() - 48, 24 }, 19, ink, juce::Justification::centredLeft, true);
    paragraph (g, message, { box.getX() + 24, box.getY() + 54, box.getWidth() - 48, withField ? 60.f : 80.f }, 13, soft);
}

Tutorial::Tutorial()
{
    styleButton (nextButton, true);
    styleButton (skipButton, false);
    skipButton.setButtonText ("Passer le tuto");
    nextButton.onClick = [this] { next(); };
    skipButton.onClick = [this] { finish(); };
    addAndMakeVisible (nextButton);
    addAndMakeVisible (skipButton);
}

void Tutorial::start (std::vector<Step> s)
{
    steps = std::move (s);
    step = steps.empty() ? -1 : 0;
    setVisible (step >= 0);
    toFront (false);
    resized();
    repaint();
}

void Tutorial::next()
{
    if (step < 0) return;
    if (++step >= (int) steps.size()) { finish(); return; }
    resized();
    repaint();
}

void Tutorial::finish()
{
    step = -1;
    setVisible (false);
    if (onFinish) onFinish();
}

juce::Rectangle<int> Tutorial::bubble() const
{
    const int w = 420, h = 196, gap = 18;
    const auto t = steps[(size_t) step].target;
    int y = t.getBottom() + gap;                                   // below the target if it fits,
    if (y + h > getHeight() - 8) y = t.getY() - gap - h;           // else above,
    const int x = juce::jlimit (16, getWidth() - w - 16, t.getCentreX() - w / 2);
    return { x, juce::jlimit (8, getHeight() - h - 8, y), w, h };
}

void Tutorial::resized()
{
    if (step < 0 || step >= (int) steps.size()) return;
    const auto b = bubble();
    nextButton.setButtonText (step == (int) steps.size() - 1 ? juce::String ("C'est parti !") : juce::String ("Suivant"));
    nextButton.setBounds (b.getRight() - 20 - 130, b.getBottom() - 20 - 32, 130, 32);
    skipButton.setBounds (b.getX() + 20, nextButton.getY(), 130, 32);
    skipButton.setVisible (step < (int) steps.size() - 1);
}

void Tutorial::paint (juce::Graphics& g)
{
    if (step < 0 || step >= (int) steps.size()) return;
    const auto& s = steps[(size_t) step];
    const auto hole = s.target.toFloat().expanded (6);

    // Everything dimmed but the part of the window this step is about.
    juce::Path shade;
    shade.addRectangle (getLocalBounds().toFloat());
    shade.addRoundedRectangle (hole, 8);
    shade.setUsingNonZeroWinding (false);
    g.setColour (juce::Colours::black.withAlpha (.72f));
    g.fillPath (shade);
    g.setColour (orange);
    g.drawRoundedRectangle (hole, 8, 2);

    const auto b = bubble().toFloat();
    g.setColour (juce::Colour (0xff141416));
    g.fillRoundedRectangle (b, 10);
    g.setColour (orange);
    g.drawRoundedRectangle (b.reduced (.5f), 10, 1.5f);
    label (g, juce::String::fromUTF8 ("\xc3\x89TAPE ") + juce::String (step + 1) + " / " + juce::String ((int) steps.size()),
           { b.getX() + 20, b.getY() + 18, 200, 14 }, 10, orange, juce::Justification::centredLeft, true, .2f);
    label (g, s.title, { b.getX() + 20, b.getY() + 36, b.getWidth() - 40, 26 }, 20, ink, juce::Justification::centredLeft, true);
    paragraph (g, s.text, { b.getX() + 20, b.getY() + 70, b.getWidth() - 40, 80 }, 13.5f, soft);
}

// ---------------------------------------------------------------- Window

PercView::PercView (PercProcessor& p)
    : processor (p), list (*this, p), licenceBadge (faint, orange, juce::Justification::centred),
      licencePanel ({ bg, juce::Colour (0xff141416), ink, muted, orange, juce::Colour (0xffff6b5b) })
{
    setLookAndFeel (&look);

    for (int i = 0; i < categoryCount; ++i)
    {
        photos[(size_t) i] = loadPhoto (i);
        addAndMakeVisible (tabs.add (new Tab (*this, i, photos[(size_t) i])));
    }
    for (int i = 0; i < padsPerPage; ++i)
        addAndMakeVisible (pads.add (new Pad (*this, i)));

    favouritesButton.setButtonText (text ("\xe2\x98\x85 Favoris"));
    favouritesButton.setClickingTogglesState (true);
    favouritesButton.setWantsKeyboardFocus (false);
    favouritesButton.setToggleState (p.favouritesOnly(), juce::dontSendNotification);
    favouritesButton.onClick = [this] { setFavouritesOnly (favouritesButton.getToggleState()); };
    addAndMakeVisible (favouritesButton);
    addAndMakeVisible (list);

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
    knobs.add (new Knob (p, "sat", "SATUR.", percent));
    knobs.add (new Knob (p, "cho", "CHORUS", percent));
    knobs.add (new Knob (p, "fla", "FLANGUS", percent));
    knobs.add (new Knob (p, "dly", "DELAY", percent));
    knobs.add (new Knob (p, "rev", "REVERB", percent));
    knobs.add (new Knob (p, "vol", "VOLUME", signedValue (" dB", 1), true));
    for (auto* k : knobs) addAndMakeVisible (k);
    // Kits: menu, save, and the ? that replays the tutorial.
    kitBox.setWantsKeyboardFocus (false);
    kitBox.setTextWhenNothingSelected (text ("Kit du projet"));
    kitBox.onChange = [this] { if (const int id = kitBox.getSelectedId(); id > 0) chooseKit (id); };
    addAndMakeVisible (kitBox);
    saveKitButton.setButtonText ("Sauver le kit");
    saveKitButton.setWantsKeyboardFocus (false);
    saveKitButton.onClick = [this] { askToSaveKit(); };
    addAndMakeVisible (saveKitButton);
    helpButton.setButtonText ("?");
    helpButton.setWantsKeyboardFocus (false);
    helpButton.setTooltip (text ("Revoir le tuto"));
    helpButton.onClick = [this] { startTutorial(); };
    addAndMakeVisible (helpButton);
    rebuildKitMenu();
    setComputerKeys (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    addAndMakeVisible (licenceBadge);
    addChildComponent (tour);
    tour.onFinish = [this] { processor.setTutorialSeen (true); };
    addChildComponent (question);
    addChildComponent (licencePanel); // shows itself when there is no valid licence

    setSize (width, height);
    // Notes played while the window was closed are not news: no stale name, no pad lit on opening.
    shownHits = p.lastHit().count;
    for (int i = 0; i < padsPerPage; ++i) p.takeFlash (i);
    showSound (p.padSound (0), 0, -1, -1);
    refresh();
    startTimerHz (60);
    if (! p.tutorialSeen()) startTutorial(); // the first time the window opens
}

PercView::~PercView() { setLookAndFeel (nullptr); }

void PercView::resized()
{
    preset.setBounds (width - margin - 160, 22, 160, 30);
    helpButton.setBounds (preset.getX() - 40, 22, 30, 30);
    saveKitButton.setBounds (helpButton.getX() - 112, 22, 104, 30);
    kitBox.setBounds (saveKitButton.getX() - 236, 22, 228, 30);

    const float tabWidth = ((float) width - margin * 2 - 6 * (categoryCount - 1)) / categoryCount;
    for (int i = 0; i < categoryCount; ++i)
        tabs[i]->setBounds (margin + juce::roundToInt ((float) i * (tabWidth + 6)), tabsY, juce::roundToInt (tabWidth), 44);

    // Pads: 4 x 4, pad 1 at the bottom left (note 36), as on an MPC or a drum rack.
    const int gap = 8, gridWidth = splitX - margin, gridHeight = height - 34 - contentY;
    const float padWidth = (float) (gridWidth - gap * 3) / 4, padHeight = (float) (gridHeight - gap * 3) / 4;
    for (int i = 0; i < padsPerPage; ++i)
    {
        const int col = i % 4, row = 3 - i / 4;
        pads[i]->setBounds (margin + juce::roundToInt ((float) col * (padWidth + gap)), contentY + juce::roundToInt ((float) row * (padHeight + gap)),
                            juce::roundToInt (padWidth), juce::roundToInt (padHeight));
    }

    const int right = splitX + 32;
    favouritesButton.setBounds (width - margin - 86, sectionY - 4, 86, 22);
    list.setBounds (right, contentY, width - margin - right, knobsY - 14 - contentY);

    const float slot = (float) (width - margin - right) / (float) knobs.size();
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (right + juce::roundToInt (((float) i + .5f) * slot) - 35, knobsY + 12, 70, 94);
    licenceBadge.setBounds (width / 2 - 180, height - 17, 360, 14); // milieu : les coins ont déjà la photo et la version
    tour.setBounds (getLocalBounds());
    question.setBounds (getLocalBounds());
    licencePanel.setBounds (getLocalBounds());
}

// Brings the tabs, pads and list in line with the processor (category, kit, samples loaded).
void PercView::refresh()
{
    category = processor.category();
    ready = processor.samplesReady();

    for (int i = 0; i < categoryCount; ++i)
    {
        tabs[i]->selected = i == category;
        tabs[i]->repaint();
    }
    shownFavourites = processor.favouriteCount (category);
    for (int i = 0; i < padsPerPage; ++i)
    {
        const auto slot = processor.padSound (i);
        shownKit[(size_t) i] = PercProcessor::code (slot);
        auto& pad = *pads[i];
        pad.name = slot.empty() ? text ("Vide") : soundName (slot.category, slot.index);
        pad.group = slot.empty() ? text ("Glisse un son ici") : text (categories[slot.category].label);
        pad.empty = slot.empty();
        pad.selected = i == selected;
        pad.repaint();
    }
    favouritesButton.setToggleState (processor.favouritesOnly(), juce::dontSendNotification);
    kitBox.setText (kitText(), juce::dontSendNotification);
    preset.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    list.update();
    repaint();
}

juce::String PercView::kitText() const
{
    const auto name = processor.currentKit();
    return (name.isEmpty() ? text ("Kit du projet") : name) + (processor.kitModified() ? text (" (modifi\xc3\xa9)") : juce::String());
}

void PercView::rebuildKitMenu()
{
    kitBox.clear (juce::dontSendNotification);
    kitBox.addSectionHeading (text ("Kits Swell"));
    for (int k = 0; k < builtInKitCount; ++k)
        kitBox.addItem (PercProcessor::kitName (k), 1 + k);
    const auto mine = processor.userKits();
    if (! mine.isEmpty())
    {
        kitBox.addSeparator();
        kitBox.addSectionHeading (text ("Mes kits"));
        for (int i = 0; i < mine.size(); ++i)
            kitBox.addItem (mine[i], userKitId + i);
    }
    kitBox.setText (kitText(), juce::dontSendNotification);
}

void PercView::chooseKit (int menuId)
{
    if (! processor.kitModified()) { applyKitChoice (menuId); return; }
    const auto name = processor.currentKit().isEmpty() ? text ("ton kit") : text ("\xc2\xab ") + processor.currentKit() + text (" \xc2\xbb");
    question.ask (text ("Changer de kit ?"),
                  text ("Tu as chang\xc3\xa9 des pads de ") + name + text (". Le nouveau kit va remplacer les 16 pads. "
                        "Pour garder le tien, annule puis clique \xc2\xab Sauver le kit \xc2\xbb."),
                  text ("Changer"), false, {}, [this, menuId] (const juce::String&) { applyKitChoice (menuId); });
    kitBox.setText (kitText(), juce::dontSendNotification); // stays on the current kit until the answer
}

void PercView::applyKitChoice (int menuId)
{
    if (menuId >= userKitId)
    {
        const auto mine = processor.userKits();
        if (menuId - userKitId < mine.size()) processor.applyUserKit (mine[menuId - userKitId]);
    }
    else processor.applyKit (menuId - 1);
    selected = 0;
    showSound (processor.padSound (0), 0, -1, -1);
    subtitle = text ("Kit \xc2\xab ") + processor.currentKit() + text (" \xc2\xbb charg\xc3\xa9  \xc2\xb7  ") + subtitle;
    refresh();
}

void PercView::askToSaveKit()
{
    const auto current = processor.currentKit();
    question.ask (text ("Sauver le kit"), text ("Donne-lui un nom : il sera dans le menu Kit, pour tous tes projets. "
                                               "Le m\xc3\xaame nom remplace l'ancienne version."),
                  text ("Sauver"), true, current.isEmpty() || builtInKitIndex (current) >= 0 ? text ("Mon kit") : current,
                  [this] (const juce::String& name)
                  {
                      if (! processor.saveUserKit (name)) return;
                      rebuildKitMenu();
                      refresh();
                      bigName = processor.currentKit();
                      subtitle = text ("Kit sauv\xc3\xa9 : tu le retrouves dans le menu Kit, dans tous tes projets.");
                      repaint();
                  });
}

void PercView::startTutorial()
{
    const auto padsArea = pads[12]->getBounds().getUnion (pads[3]->getBounds());
    const auto listArea = list.getBounds().withTop (sectionY - 6);
    const auto knobsArea = knobs[0]->getBounds().getUnion (knobs[knobs.size() - 1]->getBounds()).expanded (4, 8);
    tour.start ({
        { kitBox.getBounds().getUnion (helpButton.getBounds()), text ("Choisis un kit"),
          text ("Un kit remplit les 16 pads d'un coup, avec les effets qui vont avec. Prends \xc2\xab Vide \xc2\xbb "
                "pour partir de z\xc3\xa9ro. Le bouton ? relance ce tuto.") },
        { listArea, text ("\xc3\x89""coute les sons"),
          text ("Choisis un onglet (Djemb\xc3\xa9, Congas\xe2\x80\xa6), puis clique un son dans la liste : tu l'entends, "
                "et ton kit ne change pas.") },
        { padsArea, text ("Mets-le dans un pad"),
          text ("Glisse le son sur un pad, ou double-clique-le : il va dans le pad s\xc3\xa9lectionn\xc3\xa9 (cadre orange). "
                "Ton kit est sauvegard\xc3\xa9 avec ton projet.") },
        { padsArea, text ("Joue"),
          text ("Clique un pad (en haut = doux, en bas = fort)") + (keysPlayPads ? text (", tape A Z E R T Y U I et Q S D F G H J K") : juce::String())
              + text (", ou joue depuis ton piano roll : ") + padNote (0) + text (" \xc3\xa0 ") + padNote (padsPerPage - 1) + text (" jouent les pads 1 \xc3\xa0 16. Plus haut ou plus bas, "
                "chaque note joue le pad de m\xc3\xaame nom : tous les F jouent le pad 6.") },
        { knobsArea.getUnion (preset.getBounds()), text ("R\xc3\xa8gle le son"),
          text ("Tune, Decay et les effets. Le menu en haut \xc3\xa0 droite change d'ambiance. Content de ton kit ? "
                "\xc2\xab Sauver le kit \xc2\xbb le garde pour tous tes projets.") },
    });
}

void PercView::selectCategory (int c)
{
    processor.selectCategory (c);
    refresh();
}

void PercView::selectPad (int pad)
{
    if (pad == selected) return;
    selected = pad;
    refresh();
}

void PercView::play (int pad, float velocity) { processor.playPad (pad, velocity); }

void PercView::previewSound (int index)
{
    processor.playSound (category, index, .85f);
    bigName = soundName (category, index);
    subtitle = text ("\xc3\x89""coute  \xc2\xb7  ") + text (categories[category].label)
             + text ("  \xc2\xb7  double-clic ou glisse-le sur un pad pour le charger");
    repaint (margin, 80, 560, 150);
}

void PercView::loadSound (int index) { loadSoundInto (selected, category, index); }

void PercView::loadSoundInto (int pad, int c, int index)
{
    if (pad < 0 || pad >= padsPerPage || c < 0 || c >= categoryCount || index < 0 || index >= PercProcessor::soundCount (c)) return;
    selected = pad;
    processor.setPadSound (pad, c, index);
    processor.markKitModified();
    processor.playSound (c, index, .85f);
    showSound ({ c, index }, pad, -1, -1);
    subtitle = text ("Charg\xc3\xa9 dans le ") + subtitle;
    refresh();
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

void PercView::showSound (PercProcessor::Slot s, int pad, int note, int velocity)
{
    if (s.empty())
    {
        bigName = text ("Pad vide");
        subtitle = "Pad " + juce::String (pad + 1) + text ("  \xc2\xb7  ") + PercProcessor::noteName (note >= 0 ? note : firstPadNote + pad)
                 + text ("  \xc2\xb7  glisse un son de la liste sur ce pad");
        repaint (margin, 80, 560, 150);
        return;
    }
    bigName = soundName (s.category, s.index);
    // The note actually played (it can be far from the pad's own note: the 16 pads repeat over the keyboard).
    subtitle = "Pad " + juce::String (pad + 1) + text ("  \xc2\xb7  ") + PercProcessor::noteName (note >= 0 ? note : firstPadNote + pad)
             + text ("  \xc2\xb7  ") + text (categories[s.category].label);
    if (velocity >= 0) subtitle << text ("  \xc2\xb7  ") << velocity;
    repaint (margin, 80, 520, 150);
}

void PercView::timerCallback()
{
    bool kitChanged = false;
    for (int i = 0; i < padsPerPage; ++i)
    {
        const auto slot = processor.padSound (i);
        kitChanged |= shownKit[(size_t) i] != PercProcessor::code (slot);
    }
    if (! kitBox.isPopupActive() && kitBox.getText() != kitText()) kitChanged = true; // e.g. the host restored a project
    if (kitChanged || processor.category() != category || processor.samplesReady() != ready
        || processor.favouriteCount (processor.category()) != shownFavourites || processor.favouritesOnly() != favouritesButton.getToggleState())
        refresh();

    if (preset.getSelectedId() != processor.getCurrentProgram() + 1)
        preset.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    // The name shows the latest note in time (several pads can be hit between two frames).
    if (const auto hit = processor.lastHit(); hit.count != shownHits)
    {
        shownHits = hit.count;
        showSound (processor.padSound (hit.pad), hit.pad, hit.note, hit.velocity);
    }
    for (int i = 0; i < padsPerPage; ++i)
    {
        const float hit = processor.takeFlash (i);
        pads[i]->setHeat (hit > 0 ? 1.f : (pads[i]->getHeat() < .02f ? 0.f : pads[i]->getHeat() * .8f));
    }
}

void PercView::setComputerKeys (bool on)
{
    keysPlayPads = on;
    setWantsKeyboardFocus (on);
    for (auto* pad : pads) { pad->showKey = on; pad->repaint(); }
}

bool PercView::keyPressed (const juce::KeyPress& key)
{
    if (tour.isVisible() && key == juce::KeyPress::escapeKey) { tour.finish(); return true; }
    if (! keysPlayPads) return false; // let the host have the key
    const int i = juce::String (padKeys).indexOfChar (juce::CharacterFunctions::toLowerCase (key.getTextCharacter()));
    if (i < 0) return false;
    selectPad (i);
    play (i, .85f);
    return true;
}

void PercView::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    const float W = width;
    if (category < 0) return;
    const auto& cat = categories[category];

    // Photo of the tab shown in the list
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
    label (g, "KIT", kitBox.getBounds().toFloat().withY (7).withHeight (12), 9, faint, juce::Justification::centredLeft, false, .2f);
    label (g, "EFFETS", preset.getBounds().toFloat().withY (7).withHeight (12), 9, faint, juce::Justification::centredLeft, false, .2f);

    // Last sound played
    label (g, ready ? subtitle : text ("Chargement des sons\xe2\x80\xa6"), { (float) margin, 92, 500, 16 }, 12, ready ? muted : orange);
    label (g, bigName, { (float) margin - 2, 112, 520, 70 }, 44, juce::Colours::white);
    label (g, text ("Le piano roll joue les pads 1 \xc3\xa0 16 de ") + padNote (0) + text (" \xc3\xa0 ") + padNote (padsPerPage - 1)
               + text (". Ailleurs, chaque note joue le pad de m\xc3\xaame nom (tous les F = pad 6)."),
           { (float) margin, 186, 560, 18 }, 11.5f, faint);
    label (g, text ("Clic sur un son : tu l'\xc3\xa9""coutes, le kit ne change pas. Double-clic ou glisser sur un pad : il y est charg\xc3\xa9."),
           { (float) margin, 204, 560, 18 }, 11.5f, faint);

    // Sections
    auto section = [&] (const juce::String& name, float x, float ruleEnd)
    {
        label (g, name, { x, (float) sectionY, ruleEnd - x, 14 }, 9, faint, juce::Justification::centredLeft, false, .2f);
        const float textEnd = x + juce::GlyphArrangement::getStringWidth (juce::FontOptions (9.f).withKerningFactor (.2f), name) + 12;
        g.setColour (rule);
        g.fillRect (textEnd, (float) sectionY + 7, ruleEnd - textEnd, 1.f);
    };
    section (text ("KIT  \xc2\xb7  PAD ") + juce::String (selected + 1) + " " + text ("S\xc3\x89LECTIONN\xc3\x89"), (float) margin, (float) splitX);
    const int right = splitX + 32;
    section (text ("SONS  \xc2\xb7  ") + upper (text (cat.label)) + text ("  \xc2\xb7  CLIC : \xc3\x89""COUTER  \xc2\xb7  DOUBLE-CLIC : DANS LE PAD ") + juce::String (selected + 1),
             (float) right, (float) favouritesButton.getX() - 12);
    if (processor.favouritesOnly() && processor.favouriteCount (category) == 0)
        label (g, text ("Aucun favori dans cet onglet : touche l'\xc3\xa9toile d'un son, ou d\xc3\xa9sactive \xe2\x98\x85 Favoris."),
               { (float) right, (float) contentY + 10, W - margin - right, 40 }, 12.5f, muted, juce::Justification::centred);
    g.setColour (rule);
    g.fillRect ((float) right, (float) knobsY, W - margin - right, 1.f);

    label (g, text ("Photo provisoire : ") + text (cat.credit), { (float) margin, (float) height - 20, 400, 14 }, 10, faint);
    label (g, "Swell-audio / " SWELL_VERSION " / by @sevyondatracks", { W - margin - 300, (float) height - 20, 300, 14 }, 10, faint, juce::Justification::centredRight);
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
