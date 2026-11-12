#pragma once
#include "Categories.h"

// Ready-made kits: what the 16 pads hold (pad 1 = note 36, bottom left) and the effects preset that goes with them.
// A pad picks a sound by tab and by its place in that tab, the sounds of a tab going from low to high (0 = the lowest,
// 1 = the highest): a kit stays right when sounds are added to a tab.
struct KitPad { int category; float where; }; // category -1: empty pad
struct Kit { const char* name; int preset; KitPad pads[padsPerPage]; }; // preset: index in the effect presets

constexpr KitPad emptyPad { -1, 0 };

// Tabs: 0 Djembé, 1 Congas, 2 Talking, 3 Percs, 4 Toms, 5 Rims, 6 Shakers, 7 Claps, 8 Métal, 9 FX.
// Presets: 0 Sec et net, 1 Soft and Clean, 2 Afro Room, 3 Tape Punch, 4 Dub Delay, 5 Flangus Trip, 6 Grande Salle.
inline const Kit builtInKits[]
{
    // The kit of a new instance (and of the projects made before 0.5): a bit of every tab.
    { "Swell Mix", 1, {
        { 0, .1f }, { 0, .5f }, { 0, .9f }, { 1, .25f },
        { 1, .55f }, { 1, .85f }, { 2, .2f }, { 2, .8f },
        { 4, .3f }, { 3, .5f }, { 5, .5f }, { 7, .8f },
        { 6, .5f }, { 6, .85f }, { 8, 0 }, { 9, .3f } } },
    { "Afro Groove", 2, {
        { 0, .05f }, { 0, .5f }, { 0, .9f }, { 2, .2f },
        { 2, .5f }, { 2, .85f }, { 1, .2f }, { 1, .5f },
        { 1, .8f }, { 3, .5f }, { 5, .5f }, { 7, .4f },
        { 6, .3f }, { 6, .7f }, { 8, .3f }, { 8, .6f } } },
    { "Congas Live", 1, {
        { 1, 0 }, { 1, .11f }, { 1, .22f }, { 1, .33f },
        { 1, .44f }, { 1, .55f }, { 1, .66f }, { 1, .77f },
        { 1, .88f }, { 1, 1 }, { 3, .3f }, { 3, .6f },
        { 3, .9f }, { 6, .5f }, { 5, .5f }, { 7, .5f } } },
    { "Talking & Djemb\xc3\xa9", 2, {
        { 0, 0 }, { 0, .15f }, { 0, .3f }, { 0, .45f },
        { 0, .6f }, { 0, .75f }, { 0, .9f }, { 0, 1 },
        { 2, 0 }, { 2, .15f }, { 2, .3f }, { 2, .45f },
        { 2, .6f }, { 2, .75f }, { 2, .9f }, { 2, 1 } } },
    { "Shakers & Claps", 0, {
        { 6, 0 }, { 6, .15f }, { 6, .3f }, { 6, .45f },
        { 6, .6f }, { 6, .75f }, { 6, .9f }, { 6, 1 },
        { 7, 0 }, { 7, .2f }, { 7, .4f }, { 7, .6f },
        { 7, .8f }, { 7, 1 }, { 5, .3f }, { 5, .7f } } },
    { "Percs & M\xc3\xa9tal", 3, {
        { 4, .1f }, { 4, .4f }, { 4, .7f }, { 4, .95f },
        { 3, .2f }, { 3, .45f }, { 3, .7f }, { 3, .95f },
        { 5, .3f }, { 5, .7f }, { 8, 0 }, { 8, .2f },
        { 8, .4f }, { 8, .6f }, { 8, .8f }, { 8, 1 } } },
    { "FX & Textures", 5, {
        { 9, 0 }, { 9, .09f }, { 9, .18f }, { 9, .27f },
        { 9, .36f }, { 9, .45f }, { 9, .55f }, { 9, .64f },
        { 9, .73f }, { 9, .82f }, { 9, .91f }, { 9, 1 },
        { 8, .15f }, { 8, .45f }, { 8, .75f }, { 8, .95f } } },
    // Start from scratch: every pad empty, drag the sounds onto them.
    { "Vide", 1, {
        emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad,
        emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad, emptyPad } },
};

constexpr int builtInKitCount = (int) (sizeof (builtInKits) / sizeof (builtInKits[0]));
