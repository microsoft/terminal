// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/colorTable.hpp"
#include "til/static_map.h"

using namespace Microsoft::Console;
using namespace std::string_view_literals;

static constexpr std::array<til::color, 16> campbellColorTable{
    til::color{ 0x0C, 0x0C, 0x0C },
    til::color{ 0xC5, 0x0F, 0x1F },
    til::color{ 0x13, 0xA1, 0x0E },
    til::color{ 0xC1, 0x9C, 0x00 },
    til::color{ 0x00, 0x37, 0xDA },
    til::color{ 0x88, 0x17, 0x98 },
    til::color{ 0x3A, 0x96, 0xDD },
    til::color{ 0xCC, 0xCC, 0xCC },
    til::color{ 0x76, 0x76, 0x76 },
    til::color{ 0xE7, 0x48, 0x56 },
    til::color{ 0x16, 0xC6, 0x0C },
    til::color{ 0xF9, 0xF1, 0xA5 },
    til::color{ 0x3B, 0x78, 0xFF },
    til::color{ 0xB4, 0x00, 0x9E },
    til::color{ 0x61, 0xD6, 0xD6 },
    til::color{ 0xF2, 0xF2, 0xF2 },
};

static constexpr std::array<til::color, 256> standardXterm256ColorTable{
    til::color{ 0x00, 0x00, 0x00 },
    til::color{ 0x80, 0x00, 0x00 },
    til::color{ 0x00, 0x80, 0x00 },
    til::color{ 0x80, 0x80, 0x00 },
    til::color{ 0x00, 0x00, 0x80 },
    til::color{ 0x80, 0x00, 0x80 },
    til::color{ 0x00, 0x80, 0x80 },
    til::color{ 0xC0, 0xC0, 0xC0 },
    til::color{ 0x80, 0x80, 0x80 },
    til::color{ 0xFF, 0x00, 0x00 },
    til::color{ 0x00, 0xFF, 0x00 },
    til::color{ 0xFF, 0xFF, 0x00 },
    til::color{ 0x00, 0x00, 0xFF },
    til::color{ 0xFF, 0x00, 0xFF },
    til::color{ 0x00, 0xFF, 0xFF },
    til::color{ 0xFF, 0xFF, 0xFF },
    til::color{ 0x00, 0x00, 0x00 },
    til::color{ 0x00, 0x00, 0x5F },
    til::color{ 0x00, 0x00, 0x87 },
    til::color{ 0x00, 0x00, 0xAF },
    til::color{ 0x00, 0x00, 0xD7 },
    til::color{ 0x00, 0x00, 0xFF },
    til::color{ 0x00, 0x5F, 0x00 },
    til::color{ 0x00, 0x5F, 0x5F },
    til::color{ 0x00, 0x5F, 0x87 },
    til::color{ 0x00, 0x5F, 0xAF },
    til::color{ 0x00, 0x5F, 0xD7 },
    til::color{ 0x00, 0x5F, 0xFF },
    til::color{ 0x00, 0x87, 0x00 },
    til::color{ 0x00, 0x87, 0x5F },
    til::color{ 0x00, 0x87, 0x87 },
    til::color{ 0x00, 0x87, 0xAF },
    til::color{ 0x00, 0x87, 0xD7 },
    til::color{ 0x00, 0x87, 0xFF },
    til::color{ 0x00, 0xAF, 0x00 },
    til::color{ 0x00, 0xAF, 0x5F },
    til::color{ 0x00, 0xAF, 0x87 },
    til::color{ 0x00, 0xAF, 0xAF },
    til::color{ 0x00, 0xAF, 0xD7 },
    til::color{ 0x00, 0xAF, 0xFF },
    til::color{ 0x00, 0xD7, 0x00 },
    til::color{ 0x00, 0xD7, 0x5F },
    til::color{ 0x00, 0xD7, 0x87 },
    til::color{ 0x00, 0xD7, 0xAF },
    til::color{ 0x00, 0xD7, 0xD7 },
    til::color{ 0x00, 0xD7, 0xFF },
    til::color{ 0x00, 0xFF, 0x00 },
    til::color{ 0x00, 0xFF, 0x5F },
    til::color{ 0x00, 0xFF, 0x87 },
    til::color{ 0x00, 0xFF, 0xAF },
    til::color{ 0x00, 0xFF, 0xD7 },
    til::color{ 0x00, 0xFF, 0xFF },
    til::color{ 0x5F, 0x00, 0x00 },
    til::color{ 0x5F, 0x00, 0x5F },
    til::color{ 0x5F, 0x00, 0x87 },
    til::color{ 0x5F, 0x00, 0xAF },
    til::color{ 0x5F, 0x00, 0xD7 },
    til::color{ 0x5F, 0x00, 0xFF },
    til::color{ 0x5F, 0x5F, 0x00 },
    til::color{ 0x5F, 0x5F, 0x5F },
    til::color{ 0x5F, 0x5F, 0x87 },
    til::color{ 0x5F, 0x5F, 0xAF },
    til::color{ 0x5F, 0x5F, 0xD7 },
    til::color{ 0x5F, 0x5F, 0xFF },
    til::color{ 0x5F, 0x87, 0x00 },
    til::color{ 0x5F, 0x87, 0x5F },
    til::color{ 0x5F, 0x87, 0x87 },
    til::color{ 0x5F, 0x87, 0xAF },
    til::color{ 0x5F, 0x87, 0xD7 },
    til::color{ 0x5F, 0x87, 0xFF },
    til::color{ 0x5F, 0xAF, 0x00 },
    til::color{ 0x5F, 0xAF, 0x5F },
    til::color{ 0x5F, 0xAF, 0x87 },
    til::color{ 0x5F, 0xAF, 0xAF },
    til::color{ 0x5F, 0xAF, 0xD7 },
    til::color{ 0x5F, 0xAF, 0xFF },
    til::color{ 0x5F, 0xD7, 0x00 },
    til::color{ 0x5F, 0xD7, 0x5F },
    til::color{ 0x5F, 0xD7, 0x87 },
    til::color{ 0x5F, 0xD7, 0xAF },
    til::color{ 0x5F, 0xD7, 0xD7 },
    til::color{ 0x5F, 0xD7, 0xFF },
    til::color{ 0x5F, 0xFF, 0x00 },
    til::color{ 0x5F, 0xFF, 0x5F },
    til::color{ 0x5F, 0xFF, 0x87 },
    til::color{ 0x5F, 0xFF, 0xAF },
    til::color{ 0x5F, 0xFF, 0xD7 },
    til::color{ 0x5F, 0xFF, 0xFF },
    til::color{ 0x87, 0x00, 0x00 },
    til::color{ 0x87, 0x00, 0x5F },
    til::color{ 0x87, 0x00, 0x87 },
    til::color{ 0x87, 0x00, 0xAF },
    til::color{ 0x87, 0x00, 0xD7 },
    til::color{ 0x87, 0x00, 0xFF },
    til::color{ 0x87, 0x5F, 0x00 },
    til::color{ 0x87, 0x5F, 0x5F },
    til::color{ 0x87, 0x5F, 0x87 },
    til::color{ 0x87, 0x5F, 0xAF },
    til::color{ 0x87, 0x5F, 0xD7 },
    til::color{ 0x87, 0x5F, 0xFF },
    til::color{ 0x87, 0x87, 0x00 },
    til::color{ 0x87, 0x87, 0x5F },
    til::color{ 0x87, 0x87, 0x87 },
    til::color{ 0x87, 0x87, 0xAF },
    til::color{ 0x87, 0x87, 0xD7 },
    til::color{ 0x87, 0x87, 0xFF },
    til::color{ 0x87, 0xAF, 0x00 },
    til::color{ 0x87, 0xAF, 0x5F },
    til::color{ 0x87, 0xAF, 0x87 },
    til::color{ 0x87, 0xAF, 0xAF },
    til::color{ 0x87, 0xAF, 0xD7 },
    til::color{ 0x87, 0xAF, 0xFF },
    til::color{ 0x87, 0xD7, 0x00 },
    til::color{ 0x87, 0xD7, 0x5F },
    til::color{ 0x87, 0xD7, 0x87 },
    til::color{ 0x87, 0xD7, 0xAF },
    til::color{ 0x87, 0xD7, 0xD7 },
    til::color{ 0x87, 0xD7, 0xFF },
    til::color{ 0x87, 0xFF, 0x00 },
    til::color{ 0x87, 0xFF, 0x5F },
    til::color{ 0x87, 0xFF, 0x87 },
    til::color{ 0x87, 0xFF, 0xAF },
    til::color{ 0x87, 0xFF, 0xD7 },
    til::color{ 0x87, 0xFF, 0xFF },
    til::color{ 0xAF, 0x00, 0x00 },
    til::color{ 0xAF, 0x00, 0x5F },
    til::color{ 0xAF, 0x00, 0x87 },
    til::color{ 0xAF, 0x00, 0xAF },
    til::color{ 0xAF, 0x00, 0xD7 },
    til::color{ 0xAF, 0x00, 0xFF },
    til::color{ 0xAF, 0x5F, 0x00 },
    til::color{ 0xAF, 0x5F, 0x5F },
    til::color{ 0xAF, 0x5F, 0x87 },
    til::color{ 0xAF, 0x5F, 0xAF },
    til::color{ 0xAF, 0x5F, 0xD7 },
    til::color{ 0xAF, 0x5F, 0xFF },
    til::color{ 0xAF, 0x87, 0x00 },
    til::color{ 0xAF, 0x87, 0x5F },
    til::color{ 0xAF, 0x87, 0x87 },
    til::color{ 0xAF, 0x87, 0xAF },
    til::color{ 0xAF, 0x87, 0xD7 },
    til::color{ 0xAF, 0x87, 0xFF },
    til::color{ 0xAF, 0xAF, 0x00 },
    til::color{ 0xAF, 0xAF, 0x5F },
    til::color{ 0xAF, 0xAF, 0x87 },
    til::color{ 0xAF, 0xAF, 0xAF },
    til::color{ 0xAF, 0xAF, 0xD7 },
    til::color{ 0xAF, 0xAF, 0xFF },
    til::color{ 0xAF, 0xD7, 0x00 },
    til::color{ 0xAF, 0xD7, 0x5F },
    til::color{ 0xAF, 0xD7, 0x87 },
    til::color{ 0xAF, 0xD7, 0xAF },
    til::color{ 0xAF, 0xD7, 0xD7 },
    til::color{ 0xAF, 0xD7, 0xFF },
    til::color{ 0xAF, 0xFF, 0x00 },
    til::color{ 0xAF, 0xFF, 0x5F },
    til::color{ 0xAF, 0xFF, 0x87 },
    til::color{ 0xAF, 0xFF, 0xAF },
    til::color{ 0xAF, 0xFF, 0xD7 },
    til::color{ 0xAF, 0xFF, 0xFF },
    til::color{ 0xD7, 0x00, 0x00 },
    til::color{ 0xD7, 0x00, 0x5F },
    til::color{ 0xD7, 0x00, 0x87 },
    til::color{ 0xD7, 0x00, 0xAF },
    til::color{ 0xD7, 0x00, 0xD7 },
    til::color{ 0xD7, 0x00, 0xFF },
    til::color{ 0xD7, 0x5F, 0x00 },
    til::color{ 0xD7, 0x5F, 0x5F },
    til::color{ 0xD7, 0x5F, 0x87 },
    til::color{ 0xD7, 0x5F, 0xAF },
    til::color{ 0xD7, 0x5F, 0xD7 },
    til::color{ 0xD7, 0x5F, 0xFF },
    til::color{ 0xD7, 0x87, 0x00 },
    til::color{ 0xD7, 0x87, 0x5F },
    til::color{ 0xD7, 0x87, 0x87 },
    til::color{ 0xD7, 0x87, 0xAF },
    til::color{ 0xD7, 0x87, 0xD7 },
    til::color{ 0xD7, 0x87, 0xFF },
    til::color{ 0xD7, 0xAF, 0x00 },
    til::color{ 0xD7, 0xAF, 0x5F },
    til::color{ 0xD7, 0xAF, 0x87 },
    til::color{ 0xD7, 0xAF, 0xAF },
    til::color{ 0xD7, 0xAF, 0xD7 },
    til::color{ 0xD7, 0xAF, 0xFF },
    til::color{ 0xD7, 0xD7, 0x00 },
    til::color{ 0xD7, 0xD7, 0x5F },
    til::color{ 0xD7, 0xD7, 0x87 },
    til::color{ 0xD7, 0xD7, 0xAF },
    til::color{ 0xD7, 0xD7, 0xD7 },
    til::color{ 0xD7, 0xD7, 0xFF },
    til::color{ 0xD7, 0xFF, 0x00 },
    til::color{ 0xD7, 0xFF, 0x5F },
    til::color{ 0xD7, 0xFF, 0x87 },
    til::color{ 0xD7, 0xFF, 0xAF },
    til::color{ 0xD7, 0xFF, 0xD7 },
    til::color{ 0xD7, 0xFF, 0xFF },
    til::color{ 0xFF, 0x00, 0x00 },
    til::color{ 0xFF, 0x00, 0x5F },
    til::color{ 0xFF, 0x00, 0x87 },
    til::color{ 0xFF, 0x00, 0xAF },
    til::color{ 0xFF, 0x00, 0xD7 },
    til::color{ 0xFF, 0x00, 0xFF },
    til::color{ 0xFF, 0x5F, 0x00 },
    til::color{ 0xFF, 0x5F, 0x5F },
    til::color{ 0xFF, 0x5F, 0x87 },
    til::color{ 0xFF, 0x5F, 0xAF },
    til::color{ 0xFF, 0x5F, 0xD7 },
    til::color{ 0xFF, 0x5F, 0xFF },
    til::color{ 0xFF, 0x87, 0x00 },
    til::color{ 0xFF, 0x87, 0x5F },
    til::color{ 0xFF, 0x87, 0x87 },
    til::color{ 0xFF, 0x87, 0xAF },
    til::color{ 0xFF, 0x87, 0xD7 },
    til::color{ 0xFF, 0x87, 0xFF },
    til::color{ 0xFF, 0xAF, 0x00 },
    til::color{ 0xFF, 0xAF, 0x5F },
    til::color{ 0xFF, 0xAF, 0x87 },
    til::color{ 0xFF, 0xAF, 0xAF },
    til::color{ 0xFF, 0xAF, 0xD7 },
    til::color{ 0xFF, 0xAF, 0xFF },
    til::color{ 0xFF, 0xD7, 0x00 },
    til::color{ 0xFF, 0xD7, 0x5F },
    til::color{ 0xFF, 0xD7, 0x87 },
    til::color{ 0xFF, 0xD7, 0xAF },
    til::color{ 0xFF, 0xD7, 0xD7 },
    til::color{ 0xFF, 0xD7, 0xFF },
    til::color{ 0xFF, 0xFF, 0x00 },
    til::color{ 0xFF, 0xFF, 0x5F },
    til::color{ 0xFF, 0xFF, 0x87 },
    til::color{ 0xFF, 0xFF, 0xAF },
    til::color{ 0xFF, 0xFF, 0xD7 },
    til::color{ 0xFF, 0xFF, 0xFF },
    til::color{ 0x08, 0x08, 0x08 },
    til::color{ 0x12, 0x12, 0x12 },
    til::color{ 0x1C, 0x1C, 0x1C },
    til::color{ 0x26, 0x26, 0x26 },
    til::color{ 0x30, 0x30, 0x30 },
    til::color{ 0x3A, 0x3A, 0x3A },
    til::color{ 0x44, 0x44, 0x44 },
    til::color{ 0x4E, 0x4E, 0x4E },
    til::color{ 0x58, 0x58, 0x58 },
    til::color{ 0x62, 0x62, 0x62 },
    til::color{ 0x6C, 0x6C, 0x6C },
    til::color{ 0x76, 0x76, 0x76 },
    til::color{ 0x80, 0x80, 0x80 },
    til::color{ 0x8A, 0x8A, 0x8A },
    til::color{ 0x94, 0x94, 0x94 },
    til::color{ 0x9E, 0x9E, 0x9E },
    til::color{ 0xA8, 0xA8, 0xA8 },
    til::color{ 0xB2, 0xB2, 0xB2 },
    til::color{ 0xBC, 0xBC, 0xBC },
    til::color{ 0xC6, 0xC6, 0xC6 },
    til::color{ 0xD0, 0xD0, 0xD0 },
    til::color{ 0xDA, 0xDA, 0xDA },
    til::color{ 0xE4, 0xE4, 0xE4 },
    til::color{ 0xEE, 0xEE, 0xEE },
};

static constexpr til::color _calculateXorgAppColorVariant(til::color baseColor, size_t variant)
{
    float ratio = 1.0f;
    switch (variant)
    {
    case 1:
        ratio = 1.0f;
        break;
    case 2:
        ratio = 0.932f;
        break;
    case 3:
        ratio = 0.804f;
        break;
    case 4:
        ratio = 0.548f;
        break;
    default:
        break;
    }

    return til::color(
        ::base::saturated_cast<uint8_t>(baseColor.r * ratio),
        ::base::saturated_cast<uint8_t>(baseColor.g * ratio),
        ::base::saturated_cast<uint8_t>(baseColor.b * ratio));
}

static constexpr til::presorted_static_map xorgAppStandardVariantColorTable
{
    std::pair{ "antiquewhite"sv, std::array<til::color, 2>{ til::color{ 250, 235, 215 }, til::color{ 255, 239, 219 }}},
    std::pair{ "brown"sv, std::array<til::color, 2>{ til::color{ 165, 42, 42 }, til::color{ 255, 64, 64 } } },
    std::pair{ "burlywood"sv, std::array<til::color, 2>{ til::color{ 222, 184, 135 }, til::color{ 255, 211, 155 } } },
    std::pair{ "cadetblue"sv, std::array<til::color, 2>{ til::color{ 95, 158, 160 }, til::color{ 152, 245, 255 } } },
    std::pair{ "chocolate"sv, std::array<til::color, 2>{ til::color{ 210, 105, 30 }, til::color{ 255, 127, 36 } } },
    std::pair{ "coral"sv, std::array<til::color, 2>{ til::color{ 255, 127, 80 }, til::color{ 255, 114, 86 }}},
    std::pair{ "darkgoldenrod"sv, std::array<til::color, 2>{ til::color{ 184, 134, 11 }, til::color{ 255, 185, 15 } } },
    std::pair{ "darkolivegreen"sv, std::array<til::color, 2>{ til::color{ 85, 107, 47 }} },
    std::pair{ "darkorange"sv, std::array<til::color, 2>{ til::color{ 255, 140, 0 }, til::color{ 255, 127, 0 } } },
    std::pair{ "darkorchid"sv, std::array<til::color, 2>{ til::color{ 153, 50, 204 }, til::color{ 191, 62, 255 } } },
    std::pair{ "darkseagreen"sv, std::array<til::color, 2>{ til::color{ 143, 188, 143 }, til::color{ 193, 255, 193 } } },
    std::pair{ "darkslategray"sv, std::array<til::color, 2>{ til::color{ 47, 79, 79 }, til::color{ 151, 255, 255 } } },
    std::pair{ "firebrick"sv, std::array<til::color, 2>{ til::color{ 178, 34, 34 }, til::color{ 255, 48, 48 }} },
    std::pair{ "goldenrod"sv, std::array<til::color, 2>{ til::color{ 218, 165, 32 }, til::color{ 255, 193, 37 } } },
    std::pair{ "hotpink"sv, std::array<til::color, 2>{ til::color{ 255, 105, 180 }, til::color{ 255, 110, 180 }} },
    std::pair{ "indianred"sv, std::array<til::color, 2>{ til::color{ 205, 92, 92 }, til::color{ 255, 106, 106 }}},
    std::pair{ "khaki"sv, std::array<til::color, 2>{ til::color{ 240, 230, 140 }, til::color{ 255, 246, 143 } } },
    std::pair{ "lightblue"sv, std::array<til::color, 2>{ til::color{ 173, 216, 230 }, til::color{ 191, 239, 255 } } },
    std::pair{ "lightgoldenrod"sv, std::array<til::color, 2>{ til::color{ 238, 221, 130 }, til::color{ 255, 236, 139 } } },
    std::pair{ "lightpink"sv, std::array<til::color, 2>{ til::color{ 255, 182, 193 }, til::color{ 255, 174, 185 } } },
    std::pair{ "lightskyblue"sv, std::array<til::color, 2>{ til::color{ 135, 206, 250 }, til::color{ 176, 226, 255 } } },
    std::pair{ "lightsteelblue"sv, std::array<til::color, 2>{ til::color{ 176, 196, 222 }, til::color{ 202, 225, 255 } } },
    std::pair{ "maroon"sv, std::array<til::color, 2>{ til::color{ 176, 48, 96 }, til::color{ 255, 52, 179 }} },
    std::pair{ "mediumorchid"sv, std::array<til::color, 2>{ til::color{ 186, 85, 211 }, til::color{ 224, 102, 255 } } },
    std::pair{ "mediumpurple"sv, std::array<til::color, 2>{ til::color{ 147, 112, 219 }, til::color{ 171, 130, 255 }} },
    std::pair{ "olivedrab"sv, std::array<til::color, 2>{ til::color{ 107, 142, 35 }, til::color{ 192, 255, 62 } } },
    std::pair{ "orchid"sv, std::array<til::color, 2>{ til::color{ 218, 112, 214 }, til::color{ 255, 131, 250 } } },
    std::pair{ "palegreen"sv, std::array<til::color, 2>{ til::color{ 152, 251, 152 }, til::color{ 154, 255, 154 } } },
    std::pair{ "paleturquoise"sv, std::array<til::color, 2>{ til::color{ 175, 238, 238 }, til::color{ 187, 255, 255 } } },
    std::pair{ "palevioletred"sv, std::array<til::color, 2>{ til::color{ 219, 112, 147 }, til::color{ 255, 130, 171 } } },
    std::pair{ "pink"sv, std::array<til::color, 2>{ til::color{ 255, 192, 203 }, til::color{ 255, 181, 197 } } },
    std::pair{ "plum"sv, std::array<til::color, 2>{ til::color{ 221, 160, 221 }, til::color{ 255, 187, 255 } } },
    std::pair{ "purple"sv, std::array<til::color, 2>{ til::color{ 160, 32, 240 }, til::color{ 155, 48, 255 } } },
    std::pair{ "rosybrown"sv, std::array<til::color, 2>{ til::color{ 188, 143, 143 }, til::color{ 255, 193, 193 }} },
    std::pair{ "royalblue"sv, std::array<til::color, 2>{ til::color{ 65, 105, 225 }, til::color{ 72, 118, 255 } } },
    std::pair{ "salmon"sv, std::array<til::color, 2>{ til::color{ 250, 128, 114 }, til::color{ 255, 140, 105 } } },
    std::pair{ "seagreen"sv, std::array<til::color, 2>{ til::color{ 46, 139, 87 }, til::color{ 84, 255, 159 } } },
    std::pair{ "sienna"sv, std::array<til::color, 2>{ til::color{ 160, 82, 45 }, til::color{ 255, 130, 71 } } },
    std::pair{ "skyblue"sv, std::array<til::color, 2>{ til::color{ 135, 206, 235 }, til::color{ 135, 206, 255 } } },
    std::pair{ "slateblue"sv, std::array<til::color, 2>{ til::color{ 106, 90, 205 }, til::color{ 131, 111, 255 } } },
    std::pair{ "slategray"sv, std::array<til::color, 2>{ til::color{ 112, 128, 144 }, til::color{ 198, 226, 255 } } },
    std::pair{ "steelblue"sv, std::array<til::color, 2>{ til::color{ 70, 130, 180 }, til::color{ 99, 184, 255 } } },
    std::pair{ "tan"sv, std::array<til::color, 2>{ til::color{ 210, 180, 140 }, til::color{ 255, 165, 79 } } },
    std::pair{ "thistle"sv, std::array<til::color, 2>{ til::color{ 216, 191, 216 }, til::color{ 255, 225, 255 } } },
    std::pair{ "turquoise"sv, std::array<til::color, 2>{ til::color{ 64, 224, 208 } } },
    std::pair{ "violetred"sv, std::array<til::color, 2>{ til::color{ 208, 32, 144 }, til::color{ 255, 62, 150 } } },
    std::pair{ "wheat"sv, std::array<til::color, 2>{ til::color{ 245, 222, 179 }, til::color{ 255, 231, 186 } } },
};

static constexpr til::presorted_static_map xorgAppVariantColorTable{
    std::pair{ "aquamarine"sv, til::color{ 127, 255, 212 } },
    std::pair{ "azure"sv, til::color{ 240, 255, 255 } },
    std::pair{ "bisque"sv, til::color{ 255, 228, 196 } },
    std::pair{ "blue"sv,til::color{ 0, 0, 255 } },
    std::pair{ "chartreuse"sv,til::color{ 127, 255, 0 } },
    std::pair{ "cornsilk"sv, til::color{ 255, 248, 220 } },
    std::pair{ "cyan"sv, til::color{ 0, 255, 255 }},
    std::pair{ "deeppink"sv, til::color{ 255, 20, 147 } },
    std::pair{ "deepskyblue"sv, til::color{ 0, 191, 255 } },
    std::pair{ "dodgerblue"sv,  til::color{ 30, 144, 255 } },
    std::pair{ "gold"sv,  til::color{ 255, 215, 0 }},
    std::pair{ "green"sv, til::color{ 0, 255, 0 } },
    std::pair{ "honeydew"sv,  til::color{ 240, 255, 240 } },
    std::pair{ "ivory"sv,  til::color{ 255, 255, 240 } },
    std::pair{ "lavenderblush"sv, til::color{ 255, 240, 245 } },
    std::pair{ "lemonchiffon"sv, til::color{ 255, 250, 205 } },
    std::pair{ "lightcyan"sv, til::color{ 224, 255, 255 } },
    std::pair{ "lightsalmon"sv, til::color{ 255, 160, 122 } },
    std::pair{ "lightyellow"sv, til::color{ 255, 255, 224 } },
    std::pair{ "magenta"sv, til::color{ 255, 0, 255 } },
    std::pair{ "mistyrose"sv, til::color{ 255, 228, 225 } },
    std::pair{ "navajowhite"sv,til::color{ 255, 222, 173 } },
    std::pair{ "orange"sv,til::color{ 255, 165, 0 } },
    std::pair{ "orangered"sv,  til::color{ 255, 69, 0 } },
    std::pair{ "peachpuff"sv, til::color{ 255, 218, 185 } },
    std::pair{ "red"sv, til::color{ 255, 0, 0 } },
    std::pair{ "seashell"sv, til::color{ 255, 245, 238 } },
    std::pair{ "snow"sv,  til::color{ 255, 250, 250 } },
    std::pair{ "springgreen"sv, til::color{ 0, 255, 127 }},
    std::pair{ "tomato"sv,til::color{ 255, 99, 71 } },
    std::pair{ "yellow"sv, til::color{ 255, 255, 0 } },
};

static constexpr til::presorted_static_map xorgAppColorTable{
    std::pair{ "aliceblue"sv, til::color{ 240, 248, 255 } },
    std::pair{ "aqua"sv, til::color{ 0, 255, 255 } },
    std::pair{ "beige"sv, til::color{ 245, 245, 220 } },
    std::pair{ "black"sv, til::color{ 0, 0, 0 } },
    std::pair{ "blanchedalmond"sv, til::color{ 255, 235, 205 } },
    std::pair{ "blueviolet"sv, til::color{ 138, 43, 226 } },
    std::pair{ "cornflowerblue"sv, til::color{ 100, 149, 237 } },
    std::pair{ "crimson"sv, til::color{ 220, 20, 60 } },
    std::pair{ "darkblue"sv, til::color{ 0, 0, 139 } },
    std::pair{ "darkcyan"sv, til::color{ 0, 139, 139 } },
    std::pair{ "darkgray"sv, til::color{ 169, 169, 169 } },
    std::pair{ "darkgreen"sv, til::color{ 0, 100, 0 } },
    std::pair{ "darkgrey"sv, til::color{ 169, 169, 169 } },
    std::pair{ "darkkhaki"sv, til::color{ 189, 183, 107 } },
    std::pair{ "darkmagenta"sv, til::color{ 139, 0, 139 } },
    std::pair{ "darkred"sv, til::color{ 139, 0, 0 } },
    std::pair{ "darksalmon"sv, til::color{ 233, 150, 122 } },
    std::pair{ "darkslateblue"sv, til::color{ 72, 61, 139 } },
    std::pair{ "darkslategrey"sv, til::color{ 47, 79, 79 } },
    std::pair{ "darkturquoise"sv, til::color{ 0, 206, 209 } },
    std::pair{ "darkviolet"sv, til::color{ 148, 0, 211 } },
    std::pair{ "dimgray"sv, til::color{ 105, 105, 105 } },
    std::pair{ "dimgrey"sv, til::color{ 105, 105, 105 } },
    std::pair{ "floralwhite"sv, til::color{ 255, 250, 240 } },
    std::pair{ "forestgreen"sv, til::color{ 34, 139, 34 } },
    std::pair{ "fuchsia"sv, til::color{ 255, 0, 255 } },
    std::pair{ "gainsboro"sv, til::color{ 220, 220, 220 } },
    std::pair{ "ghostwhite"sv, til::color{ 248, 248, 255 } },
    std::pair{ "gray"sv, til::color{ 190, 190, 190 } },
    std::pair{ "greenyellow"sv, til::color{ 173, 255, 47 } },
    std::pair{ "grey"sv, til::color{ 190, 190, 190 } },
    std::pair{ "indigo"sv, til::color{ 75, 0, 130 } },
    std::pair{ "lavender"sv, til::color{ 230, 230, 250 } },
    std::pair{ "lawngreen"sv, til::color{ 124, 252, 0 } },
    std::pair{ "lightcoral"sv, til::color{ 240, 128, 128 } },
    std::pair{ "lightgoldenrodyellow"sv, til::color{ 250, 250, 210 } },
    std::pair{ "lightgray"sv, til::color{ 211, 211, 211 } },
    std::pair{ "lightgreen"sv, til::color{ 144, 238, 144 } },
    std::pair{ "lightgrey"sv, til::color{ 211, 211, 211 } },
    std::pair{ "lightseagreen"sv, til::color{ 32, 178, 170 } },
    std::pair{ "lightslateblue"sv, til::color{ 132, 112, 255 } },
    std::pair{ "lightslategray"sv, til::color{ 119, 136, 153 } },
    std::pair{ "lightslategrey"sv, til::color{ 119, 136, 153 } },
    std::pair{ "lime"sv, til::color{ 0, 255, 0 } },
    std::pair{ "limegreen"sv, til::color{ 50, 205, 50 } },
    std::pair{ "linen"sv, til::color{ 250, 240, 230 } },
    std::pair{ "mediumaquamarine"sv, til::color{ 102, 205, 170 } },
    std::pair{ "mediumblue"sv, til::color{ 0, 0, 205 } },
    std::pair{ "mediumseagreen"sv, til::color{ 60, 179, 113 } },
    std::pair{ "mediumslateblue"sv, til::color{ 123, 104, 238 } },
    std::pair{ "mediumspringgreen"sv, til::color{ 0, 250, 154 } },
    std::pair{ "mediumturquoise"sv, til::color{ 72, 209, 204 } },
    std::pair{ "mediumvioletred"sv, til::color{ 199, 21, 133 } },
    std::pair{ "midnightblue"sv, til::color{ 25, 25, 112 } },
    std::pair{ "mintcream"sv, til::color{ 245, 255, 250 } },
    std::pair{ "moccasin"sv, til::color{ 255, 228, 181 } },
    std::pair{ "navy"sv, til::color{ 0, 0, 128 } },
    std::pair{ "navyblue"sv, til::color{ 0, 0, 128 } },
    std::pair{ "oldlace"sv, til::color{ 253, 245, 230 } },
    std::pair{ "olive"sv, til::color{ 128, 128, 0 } },
    std::pair{ "palegoldenrod"sv, til::color{ 238, 232, 170 } },
    std::pair{ "papayawhip"sv, til::color{ 255, 239, 213 } },
    std::pair{ "peru"sv, til::color{ 205, 133, 63 } },
    std::pair{ "powderblue"sv, til::color{ 176, 224, 230 } },
    std::pair{ "rebeccapurple"sv, til::color{ 102, 51, 153 } },
    std::pair{ "saddlebrown"sv, til::color{ 139, 69, 19 } },
    std::pair{ "sandybrown"sv, til::color{ 244, 164, 96 } },
    std::pair{ "silver"sv, til::color{ 192, 192, 192 } },
    std::pair{ "slategrey"sv, til::color{ 112, 128, 144 } },
    std::pair{ "teal"sv, til::color{ 0, 128, 128 } },
    std::pair{ "violet"sv, til::color{ 238, 130, 238 } },
    std::pair{ "webgray"sv, til::color{ 128, 128, 128 } },
    std::pair{ "webgreen"sv, til::color{ 0, 128, 0 } },
    std::pair{ "webgrey"sv, til::color{ 128, 128, 128 } },
    std::pair{ "webmaroon"sv, til::color{ 128, 0, 0 } },
    std::pair{ "webpurple"sv, til::color{ 128, 0, 128 } },
    std::pair{ "white"sv, til::color{ 255, 255, 255 } },
    std::pair{ "whitesmoke"sv, til::color{ 245, 245, 245 } },
    std::pair{ "x11gray"sv, til::color{ 190, 190, 190 } },
    std::pair{ "x11green"sv, til::color{ 0, 255, 0 } },
    std::pair{ "x11grey"sv, til::color{ 190, 190, 190 } },
    std::pair{ "x11maroon"sv, til::color{ 176, 48, 96 } },
    std::pair{ "x11purple"sv, til::color{ 160, 32, 240 } },
    std::pair{ "yellowgreen"sv, til::color{ 154, 205, 50 } }
};

// Function Description:
// - Fill the first 16 entries of a given color table with the Campbell color
//   scheme, in the ANSI/VT RGB order.
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::InitializeCampbellColorTable(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);

    std::copy(campbellColorTable.begin(), campbellColorTable.end(), table.begin());
}

// Function Description:
// - Fill the first 16 entries of a given color table with the Campbell color
//   scheme, in the Windows BGR order.
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::InitializeCampbellColorTableForConhost(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);
    InitializeCampbellColorTable(table);
    SwapANSIColorOrderForConhost(table);
}

// Function Description:
// - modifies in-place the given color table from ANSI (RGB) order to Console order (BRG).
// Arguments:
// - table: a color table with at least 16 entries
// Return Value:
// - <none>, throws if the table has less that 16 entries
void Utils::SwapANSIColorOrderForConhost(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 16);
    std::swap(til::at(table, 1), til::at(table, 4));
    std::swap(til::at(table, 3), til::at(table, 6));
    std::swap(til::at(table, 9), til::at(table, 12));
    std::swap(til::at(table, 11), til::at(table, 14));
}

// Function Description:
// - Fill the first 255 entries of a given color table with the default values
//      of a full 256-color table
// Arguments:
// - table: a color table with at least 256 entries
// Return Value:
// - <none>, throws if the table has less that 256 entries
void Utils::Initialize256ColorTable(const gsl::span<COLORREF> table)
{
    THROW_HR_IF(E_INVALIDARG, table.size() < 256);

    std::copy(standardXterm256ColorTable.begin(), standardXterm256ColorTable.end(), table.begin());
}

#pragma warning(push)
#pragma warning(disable : 26447) // This is a false positive.

// Function Description:
// - Parses a color from a string based on the XOrg app color name table.
// Arguments:
// - str: a string representation of the color name to parse
// Return Value:
// - An optional color which contains value if a color was successfully parsed
std::optional<til::color> Utils::ColorFromXOrgAppColorName(const std::wstring_view wstr) noexcept
try
{
    std::stringstream ss;
    size_t variantIndex = 0;
    bool foundVariant = false;
    for (const wchar_t c : wstr)
    {
        // X11 guarantees that characters are all Latin1.
        // Return early if an invalid character is seen.
        if (c > 127)
        {
            return std::nullopt;
        }

        if (c >= L'0' && c <= L'9')
        {
            foundVariant = true;
            variantIndex *= 10;
            variantIndex += c - L'0';
            continue;
        }

        // Ignore spaces.
        if (std::iswspace(c))
        {
            continue;
        }

        if (foundVariant)
        {
            // Variant should be at the end of the string, e.g., "yellow3".
            // This means another non-numeric character is seen, e.g., "yellow3a".
            // This is invalid so return early.
            return std::nullopt;
        }

        ss << gsl::narrow_cast<char>(std::towlower(c));
    }

    std::string name(ss.str());
    const auto standartVariantColorIter = xorgAppStandardVariantColorTable.find(name);
    if (standartVariantColorIter != xorgAppStandardVariantColorTable.end())
    {
        const auto standardAndVariantBaseColor = standartVariantColorIter->second;
        if (variantIndex > 4)
        {
            return std::nullopt;
        }

        if (foundVariant)
        {
            return _calculateXorgAppColorVariant(standardAndVariantBaseColor.at(1), variantIndex);
        }
        else
        {
            return standardAndVariantBaseColor.at(0);
        }
    }

    const auto variantColorIter = xorgAppVariantColorTable.find(name);
    if (variantColorIter != xorgAppVariantColorTable.end())
    {
        const auto baseColor = variantColorIter->second;
        if (variantIndex > 4)
        {
            return std::nullopt;
        }

        if (foundVariant)
        {
            return _calculateXorgAppColorVariant(baseColor, variantIndex);
        }
        else
        {
            return baseColor;
        }
    }

    // Calculate the color value for gray0 - gray99.
    if ((name == "gray" || name == "grey") && foundVariant)
    {
        if (variantIndex > 100) // size_t is unsigned, so >=0 is implicit
        {
            return std::nullopt;
        }
        const auto component{ ::base::saturated_cast<uint8_t>(((variantIndex * 255) + 50) / 100) };
        return til::color{ component, component, component };
    }

    const auto colorIter = xorgAppColorTable.find(name);
    if (colorIter != xorgAppColorTable.end())
    {
        return colorIter->second;
    }

    return std::nullopt;
}
catch (...)
{
    LOG_CAUGHT_EXCEPTION();
    return std::nullopt;
}

#pragma warning(pop)
