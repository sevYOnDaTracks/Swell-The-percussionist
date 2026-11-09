#pragma once
#include "SampleIndex.h"

// The tabs: their photo and where it sits in the 1180 px wide window. The sounds of each tab are in SampleIndex.h.
constexpr int categoryCount = 10, padsPerPage = 16, firstPadNote = 36; // C2

struct Category
{
    const char* label;     // UTF-8
    const char* photo;     // BinaryData resource
    float photoX, photoY, photoWidth;
    const char* credit;
};

inline const Category categories[categoryCount]
{
    { "Djemb\xc3\xa9",   "djembe_jpg",    400,  -20, 780, "Niranjan Arminius, CC BY-SA 4.0" },
    { "Congas",          "congas_jpg",    380,  -40, 800, "Daniele Febei, CC BY-SA 2.0" },
    { "Talking",         "talking_jpg",   380, -100, 800, "EmmanuelOEmmanuel, CC BY-SA 4.0" },
    { "Percs",           "bongos_jpg",    380,  -30, 800, "Vladimir Morozov, CC BY 2.0" },
    { "Toms",            "toms_jpg",      380, -120, 800, "Modjou, CC BY-SA 4.0" },
    { "Rims",            "rims_jpg",      380, -110, 800, "Vladimir Morozov, CC BY 2.0" },
    { "Shakers",         "shaker_jpg",    380,  -60, 800, "Paju, CC BY-SA 3.0" },
    { "Claps",           "claps_jpg",     380, -120, 800, "Mona Hassan Abo-Abda, CC BY-SA 4.0" },
    { "M\xc3\xa9tal",    "tambourin_jpg", 380, -140, 800, "Serg Childed, CC BY-SA 4.0" },
    { "FX",              "fx_jpg",        380, -150, 800, "Ky, CC BY 2.0" },
};

inline const char* const padKeys = "azertyuiqsdfghjk"; // AZERTY keyboard, two rows
