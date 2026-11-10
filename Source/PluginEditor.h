#pragma once
#include "PluginProcessor.h"

class PercLook : public juce::LookAndFeel_V4
{
public:
    PercLook();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.f); }
};

// Rotary knob with its caption and value underneath.
class Knob : public juce::Component
{
public:
    Knob (PercProcessor&, const char* paramId, juce::String caption, std::function<juce::String (float)> format, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String caption;
    std::function<juce::String (float)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class PercView;

// One pad of the kit: click to play it (top = soft, bottom = loud) and select it; a click in the sound list loads into it.
class Pad : public juce::Component
{
public:
    Pad (PercView&, int pad);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setHeat (float h) { if (h != heat) { heat = h; repaint(); } }
    float getHeat() const { return heat; }

    juce::String name, group;
    bool selected = false;

private:
    PercView& view;
    int pad;
    float heat = 0;
};

class Tab : public juce::Component
{
public:
    Tab (PercView&, int index, juce::Image photo);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool selected = false;

private:
    PercView& view;
    int index;
    juce::Image photo;
};

// Every sound of the category (or its favourites) in two columns: click a name to load it into the selected pad,
// the star to keep it in the favourites.
class SoundList : public juce::Component, private juce::ListBoxModel
{
public:
    static constexpr int columns = 2;
    SoundList (PercView&, PercProcessor&);
    void resized() override { box.setBounds (getLocalBounds()); }
    void update() { box.updateContent(); box.repaint(); }
    void clickCell (int row, int column, bool onStar); // also used by the tests

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    PercView& view;
    PercProcessor& processor;
    juce::ListBox box;
};

// The whole window, drawn at a fixed size and scaled by the editor.
class PercView : public juce::Component, private juce::Timer
{
public:
    static constexpr int width = 1180, height = 730;

    explicit PercView (PercProcessor&);
    ~PercView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void selectCategory (int);
    void selectPad (int);
    void play (int pad, float velocity);
    void loadSound (int index);       // into the selected pad, and plays it
    void toggleFavourite (int index);
    void setFavouritesOnly (bool);
    int shownCategory() const { return category; }
    int selectedPad() const { return selected; }

private:
    void timerCallback() override;
    void refresh();
    void showSound (PercProcessor::Slot, int pad, int velocity);

    PercProcessor& processor;
    PercLook look;
    std::array<juce::Image, categoryCount> photos;
    int category = -1, selected = 0, shownFavourites = -1;
    std::array<int, padsPerPage> shownKit {};
    bool ready = false;
    juce::String bigName, subtitle;

    juce::OwnedArray<Pad> pads;
    juce::OwnedArray<Tab> tabs;
    juce::OwnedArray<Knob> knobs;
    juce::ComboBox preset;
    juce::TextButton favouritesButton;
    SoundList list;
    swell::LicenceBadge licenceBadge;
    swell::LicencePanel licencePanel; // last child: covers everything until the plugin is activated
};

class PercEditor : public juce::AudioProcessorEditor
{
public:
    explicit PercEditor (PercProcessor&);
    void resized() override;

private:
    PercView view;
};
