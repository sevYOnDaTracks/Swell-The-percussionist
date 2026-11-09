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

class Pad : public juce::Component
{
public:
    Pad (PercView&, int slot);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setHeat (float h) { if (h != heat) { heat = h; repaint(); } }
    float getHeat() const { return heat; }
    juce::Rectangle<float> starArea() const { return { (float) getWidth() - 23, 4, 20, 20 }; }

    int sound = -1;          // index in the category, -1: empty pad
    juce::String name;
    bool favourite = false;

private:
    PercView& view;
    int slot;
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

// Every sound of the category (or its favourites) in four columns: click a name to play it, the star to keep it.
class SoundList : public juce::Component, private juce::ListBoxModel
{
public:
    static constexpr int columns = 4;
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
    void turnPage (int delta);
    void play (int pad, float velocity);
    void playSound (int index);
    void toggleFavourite (int index);
    void setFavouritesOnly (bool);
    void setListShown (bool);
    int shownCategory() const { return category; }
    int shownPage() const { return page; }
    bool listShown() const { return list.isVisible(); }

private:
    void timerCallback() override;
    void refresh();
    void showSound (int index, int velocity);

    PercProcessor& processor;
    PercLook look;
    std::array<juce::Image, categoryCount> photos;
    int category = -1, page = -1, lastShown = -1, shownFavourites = -1;
    bool ready = false;
    juce::String bigName, subtitle;

    juce::OwnedArray<Pad> pads;
    juce::OwnedArray<Tab> tabs;
    juce::OwnedArray<Knob> knobs;
    juce::ComboBox preset;
    juce::TextButton previous { "<" }, next { ">" }, favouritesButton, listButton;
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
