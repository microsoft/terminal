// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/colorTable.hpp"
#include "til/static_map.h"

using namespace Microsoft::Console;
using namespace std::string_view_literals;

static constexpr std::array<til::color, 256> standard256ColorTable{
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

static constinit til::presorted_static_map xorgAppVariantColorTable{
    std::pair{ "antiquewhite"sv, std::array<til::color, 5>{ til::color{ 250, 235, 215 }, til::color{ 255, 239, 219 }, til::color{ 238, 223, 204 }, til::color{ 205, 192, 176 }, til::color{ 139, 131, 120 } } },
    std::pair{ "aquamarine"sv, std::array<til::color, 5>{ til::color{ 127, 255, 212 }, til::color{ 127, 255, 212 }, til::color{ 118, 238, 198 }, til::color{ 102, 205, 170 }, til::color{ 69, 139, 116 } } },
    std::pair{ "azure"sv, std::array<til::color, 5>{ til::color{ 240, 255, 255 }, til::color{ 240, 255, 255 }, til::color{ 224, 238, 238 }, til::color{ 193, 205, 205 }, til::color{ 131, 139, 139 } } },
    std::pair{ "bisque"sv, std::array<til::color, 5>{ til::color{ 255, 228, 196 }, til::color{ 255, 228, 196 }, til::color{ 238, 213, 183 }, til::color{ 205, 183, 158 }, til::color{ 139, 125, 107 } } },
    std::pair{ "blue"sv, std::array<til::color, 5>{ til::color{ 0, 0, 255 }, til::color{ 0, 0, 255 }, til::color{ 0, 0, 238 }, til::color{ 0, 0, 205 }, til::color{ 0, 0, 139 } } },
    std::pair{ "brown"sv, std::array<til::color, 5>{ til::color{ 165, 42, 42 }, til::color{ 255, 64, 64 }, til::color{ 238, 59, 59 }, til::color{ 205, 51, 51 }, til::color{ 139, 35, 35 } } },
    std::pair{ "burlywood"sv, std::array<til::color, 5>{ til::color{ 222, 184, 135 }, til::color{ 255, 211, 155 }, til::color{ 238, 197, 145 }, til::color{ 205, 170, 125 }, til::color{ 139, 115, 85 } } },
    std::pair{ "cadetblue"sv, std::array<til::color, 5>{ til::color{ 95, 158, 160 }, til::color{ 152, 245, 255 }, til::color{ 142, 229, 238 }, til::color{ 122, 197, 205 }, til::color{ 83, 134, 139 } } },
    std::pair{ "chartreuse"sv, std::array<til::color, 5>{ til::color{ 127, 255, 0 }, til::color{ 127, 255, 0 }, til::color{ 118, 238, 0 }, til::color{ 102, 205, 0 }, til::color{ 69, 139, 0 } } },
    std::pair{ "chocolate"sv, std::array<til::color, 5>{ til::color{ 210, 105, 30 }, til::color{ 255, 127, 36 }, til::color{ 238, 118, 33 }, til::color{ 205, 102, 29 }, til::color{ 139, 69, 19 } } },
    std::pair{ "coral"sv, std::array<til::color, 5>{ til::color{ 255, 127, 80 }, til::color{ 255, 114, 86 }, til::color{ 238, 106, 80 }, til::color{ 205, 91, 69 }, til::color{ 139, 62, 47 } } },
    std::pair{ "cornsilk"sv, std::array<til::color, 5>{ til::color{ 255, 248, 220 }, til::color{ 255, 248, 220 }, til::color{ 238, 232, 205 }, til::color{ 205, 200, 177 }, til::color{ 139, 136, 120 } } },
    std::pair{ "cyan"sv, std::array<til::color, 5>{ til::color{ 0, 255, 255 }, til::color{ 0, 255, 255 }, til::color{ 0, 238, 238 }, til::color{ 0, 205, 205 }, til::color{ 0, 139, 139 } } },
    std::pair{ "darkgoldenrod"sv, std::array<til::color, 5>{ til::color{ 184, 134, 11 }, til::color{ 255, 185, 15 }, til::color{ 238, 173, 14 }, til::color{ 205, 149, 12 }, til::color{ 139, 101, 8 } } },
    std::pair{ "darkolivegreen"sv, std::array<til::color, 5>{ til::color{ 85, 107, 47 }, til::color{ 202, 255, 112 }, til::color{ 188, 238, 104 }, til::color{ 162, 205, 90 }, til::color{ 110, 139, 61 } } },
    std::pair{ "darkorange"sv, std::array<til::color, 5>{ til::color{ 255, 140, 0 }, til::color{ 255, 127, 0 }, til::color{ 238, 118, 0 }, til::color{ 205, 102, 0 }, til::color{ 139, 69, 0 } } },
    std::pair{ "darkorchid"sv, std::array<til::color, 5>{ til::color{ 153, 50, 204 }, til::color{ 191, 62, 255 }, til::color{ 178, 58, 238 }, til::color{ 154, 50, 205 }, til::color{ 104, 34, 139 } } },
    std::pair{ "darkseagreen"sv, std::array<til::color, 5>{ til::color{ 143, 188, 143 }, til::color{ 193, 255, 193 }, til::color{ 180, 238, 180 }, til::color{ 155, 205, 155 }, til::color{ 105, 139, 105 } } },
    std::pair{ "darkslategray"sv, std::array<til::color, 5>{ til::color{ 47, 79, 79 }, til::color{ 151, 255, 255 }, til::color{ 141, 238, 238 }, til::color{ 121, 205, 205 }, til::color{ 82, 139, 139 } } },
    std::pair{ "deeppink"sv, std::array<til::color, 5>{ til::color{ 255, 20, 147 }, til::color{ 255, 20, 147 }, til::color{ 238, 18, 137 }, til::color{ 205, 16, 118 }, til::color{ 139, 10, 80 } } },
    std::pair{ "deepskyblue"sv, std::array<til::color, 5>{ til::color{ 0, 191, 255 }, til::color{ 0, 191, 255 }, til::color{ 0, 178, 238 }, til::color{ 0, 154, 205 }, til::color{ 0, 104, 139 } } },
    std::pair{ "dodgerblue"sv, std::array<til::color, 5>{ til::color{ 30, 144, 255 }, til::color{ 30, 144, 255 }, til::color{ 28, 134, 238 }, til::color{ 24, 116, 205 }, til::color{ 16, 78, 139 } } },
    std::pair{ "firebrick"sv, std::array<til::color, 5>{ til::color{ 178, 34, 34 }, til::color{ 255, 48, 48 }, til::color{ 238, 44, 44 }, til::color{ 205, 38, 38 }, til::color{ 139, 26, 26 } } },
    std::pair{ "gold"sv, std::array<til::color, 5>{ til::color{ 255, 215, 0 }, til::color{ 255, 215, 0 }, til::color{ 238, 201, 0 }, til::color{ 205, 173, 0 }, til::color{ 139, 117, 0 } } },
    std::pair{ "goldenrod"sv, std::array<til::color, 5>{ til::color{ 218, 165, 32 }, til::color{ 255, 193, 37 }, til::color{ 238, 180, 34 }, til::color{ 205, 155, 29 }, til::color{ 139, 105, 20 } } },
    std::pair{ "green"sv, std::array<til::color, 5>{ til::color{ 0, 255, 0 }, til::color{ 0, 255, 0 }, til::color{ 0, 238, 0 }, til::color{ 0, 205, 0 }, til::color{ 0, 139, 0 } } },
    std::pair{ "honeydew"sv, std::array<til::color, 5>{ til::color{ 240, 255, 240 }, til::color{ 240, 255, 240 }, til::color{ 224, 238, 224 }, til::color{ 193, 205, 193 }, til::color{ 131, 139, 131 } } },
    std::pair{ "hotpink"sv, std::array<til::color, 5>{ til::color{ 255, 105, 180 }, til::color{ 255, 110, 180 }, til::color{ 238, 106, 167 }, til::color{ 205, 96, 144 }, til::color{ 139, 58, 98 } } },
    std::pair{ "indianred"sv, std::array<til::color, 5>{ til::color{ 205, 92, 92 }, til::color{ 255, 106, 106 }, til::color{ 238, 99, 99 }, til::color{ 205, 85, 85 }, til::color{ 139, 58, 58 } } },
    std::pair{ "ivory"sv, std::array<til::color, 5>{ til::color{ 255, 255, 240 }, til::color{ 255, 255, 240 }, til::color{ 238, 238, 224 }, til::color{ 205, 205, 193 }, til::color{ 139, 139, 131 } } },
    std::pair{ "khaki"sv, std::array<til::color, 5>{ til::color{ 240, 230, 140 }, til::color{ 255, 246, 143 }, til::color{ 238, 230, 133 }, til::color{ 205, 198, 115 }, til::color{ 139, 134, 78 } } },
    std::pair{ "lavenderblush"sv, std::array<til::color, 5>{ til::color{ 255, 240, 245 }, til::color{ 255, 240, 245 }, til::color{ 238, 224, 229 }, til::color{ 205, 193, 197 }, til::color{ 139, 131, 134 } } },
    std::pair{ "lemonchiffon"sv, std::array<til::color, 5>{ til::color{ 255, 250, 205 }, til::color{ 255, 250, 205 }, til::color{ 238, 233, 191 }, til::color{ 205, 201, 165 }, til::color{ 139, 137, 112 } } },
    std::pair{ "lightblue"sv, std::array<til::color, 5>{ til::color{ 173, 216, 230 }, til::color{ 191, 239, 255 }, til::color{ 178, 223, 238 }, til::color{ 154, 192, 205 }, til::color{ 104, 131, 139 } } },
    std::pair{ "lightcyan"sv, std::array<til::color, 5>{ til::color{ 224, 255, 255 }, til::color{ 224, 255, 255 }, til::color{ 209, 238, 238 }, til::color{ 180, 205, 205 }, til::color{ 122, 139, 139 } } },
    std::pair{ "lightgoldenrod"sv, std::array<til::color, 5>{ til::color{ 238, 221, 130 }, til::color{ 255, 236, 139 }, til::color{ 238, 220, 130 }, til::color{ 205, 190, 112 }, til::color{ 139, 129, 76 } } },
    std::pair{ "lightpink"sv, std::array<til::color, 5>{ til::color{ 255, 182, 193 }, til::color{ 255, 174, 185 }, til::color{ 238, 162, 173 }, til::color{ 205, 140, 149 }, til::color{ 139, 95, 101 } } },
    std::pair{ "lightsalmon"sv, std::array<til::color, 5>{ til::color{ 255, 160, 122 }, til::color{ 255, 160, 122 }, til::color{ 238, 149, 114 }, til::color{ 205, 129, 98 }, til::color{ 139, 87, 66 } } },
    std::pair{ "lightskyblue"sv, std::array<til::color, 5>{ til::color{ 135, 206, 250 }, til::color{ 176, 226, 255 }, til::color{ 164, 211, 238 }, til::color{ 141, 182, 205 }, til::color{ 96, 123, 139 } } },
    std::pair{ "lightsteelblue"sv, std::array<til::color, 5>{ til::color{ 176, 196, 222 }, til::color{ 202, 225, 255 }, til::color{ 188, 210, 238 }, til::color{ 162, 181, 205 }, til::color{ 110, 123, 139 } } },
    std::pair{ "lightyellow"sv, std::array<til::color, 5>{ til::color{ 255, 255, 224 }, til::color{ 255, 255, 224 }, til::color{ 238, 238, 209 }, til::color{ 205, 205, 180 }, til::color{ 139, 139, 122 } } },
    std::pair{ "magenta"sv, std::array<til::color, 5>{ til::color{ 255, 0, 255 }, til::color{ 255, 0, 255 }, til::color{ 238, 0, 238 }, til::color{ 205, 0, 205 }, til::color{ 139, 0, 139 } } },
    std::pair{ "maroon"sv, std::array<til::color, 5>{ til::color{ 176, 48, 96 }, til::color{ 255, 52, 179 }, til::color{ 238, 48, 167 }, til::color{ 205, 41, 144 }, til::color{ 139, 28, 98 } } },
    std::pair{ "mediumorchid"sv, std::array<til::color, 5>{ til::color{ 186, 85, 211 }, til::color{ 224, 102, 255 }, til::color{ 209, 95, 238 }, til::color{ 180, 82, 205 }, til::color{ 122, 55, 139 } } },
    std::pair{ "mediumpurple"sv, std::array<til::color, 5>{ til::color{ 147, 112, 219 }, til::color{ 171, 130, 255 }, til::color{ 159, 121, 238 }, til::color{ 137, 104, 205 }, til::color{ 93, 71, 139 } } },
    std::pair{ "mistyrose"sv, std::array<til::color, 5>{ til::color{ 255, 228, 225 }, til::color{ 255, 228, 225 }, til::color{ 238, 213, 210 }, til::color{ 205, 183, 181 }, til::color{ 139, 125, 123 } } },
    std::pair{ "navajowhite"sv, std::array<til::color, 5>{ til::color{ 255, 222, 173 }, til::color{ 255, 222, 173 }, til::color{ 238, 207, 161 }, til::color{ 205, 179, 139 }, til::color{ 139, 121, 94 } } },
    std::pair{ "olivedrab"sv, std::array<til::color, 5>{ til::color{ 107, 142, 35 }, til::color{ 192, 255, 62 }, til::color{ 179, 238, 58 }, til::color{ 154, 205, 50 }, til::color{ 105, 139, 34 } } },
    std::pair{ "orange"sv, std::array<til::color, 5>{ til::color{ 255, 165, 0 }, til::color{ 255, 165, 0 }, til::color{ 238, 154, 0 }, til::color{ 205, 133, 0 }, til::color{ 139, 90, 0 } } },
    std::pair{ "orangered"sv, std::array<til::color, 5>{ til::color{ 255, 69, 0 }, til::color{ 255, 69, 0 }, til::color{ 238, 64, 0 }, til::color{ 205, 55, 0 }, til::color{ 139, 37, 0 } } },
    std::pair{ "orchid"sv, std::array<til::color, 5>{ til::color{ 218, 112, 214 }, til::color{ 255, 131, 250 }, til::color{ 238, 122, 233 }, til::color{ 205, 105, 201 }, til::color{ 139, 71, 137 } } },
    std::pair{ "palegreen"sv, std::array<til::color, 5>{ til::color{ 152, 251, 152 }, til::color{ 154, 255, 154 }, til::color{ 144, 238, 144 }, til::color{ 124, 205, 124 }, til::color{ 84, 139, 84 } } },
    std::pair{ "paleturquoise"sv, std::array<til::color, 5>{ til::color{ 175, 238, 238 }, til::color{ 187, 255, 255 }, til::color{ 174, 238, 238 }, til::color{ 150, 205, 205 }, til::color{ 102, 139, 139 } } },
    std::pair{ "palevioletred"sv, std::array<til::color, 5>{ til::color{ 219, 112, 147 }, til::color{ 255, 130, 171 }, til::color{ 238, 121, 159 }, til::color{ 205, 104, 137 }, til::color{ 139, 71, 93 } } },
    std::pair{ "peachpuff"sv, std::array<til::color, 5>{ til::color{ 255, 218, 185 }, til::color{ 255, 218, 185 }, til::color{ 238, 203, 173 }, til::color{ 205, 175, 149 }, til::color{ 139, 119, 101 } } },
    std::pair{ "pink"sv, std::array<til::color, 5>{ til::color{ 255, 192, 203 }, til::color{ 255, 181, 197 }, til::color{ 238, 169, 184 }, til::color{ 205, 145, 158 }, til::color{ 139, 99, 108 } } },
    std::pair{ "plum"sv, std::array<til::color, 5>{ til::color{ 221, 160, 221 }, til::color{ 255, 187, 255 }, til::color{ 238, 174, 238 }, til::color{ 205, 150, 205 }, til::color{ 139, 102, 139 } } },
    std::pair{ "purple"sv, std::array<til::color, 5>{ til::color{ 160, 32, 240 }, til::color{ 155, 48, 255 }, til::color{ 145, 44, 238 }, til::color{ 125, 38, 205 }, til::color{ 85, 26, 139 } } },
    std::pair{ "red"sv, std::array<til::color, 5>{ til::color{ 255, 0, 0 }, til::color{ 255, 0, 0 }, til::color{ 238, 0, 0 }, til::color{ 205, 0, 0 }, til::color{ 139, 0, 0 } } },
    std::pair{ "rosybrown"sv, std::array<til::color, 5>{ til::color{ 188, 143, 143 }, til::color{ 255, 193, 193 }, til::color{ 238, 180, 180 }, til::color{ 205, 155, 155 }, til::color{ 139, 105, 105 } } },
    std::pair{ "royalblue"sv, std::array<til::color, 5>{ til::color{ 65, 105, 225 }, til::color{ 72, 118, 255 }, til::color{ 67, 110, 238 }, til::color{ 58, 95, 205 }, til::color{ 39, 64, 139 } } },
    std::pair{ "salmon"sv, std::array<til::color, 5>{ til::color{ 250, 128, 114 }, til::color{ 255, 140, 105 }, til::color{ 238, 130, 98 }, til::color{ 205, 112, 84 }, til::color{ 139, 76, 57 } } },
    std::pair{ "seagreen"sv, std::array<til::color, 5>{ til::color{ 46, 139, 87 }, til::color{ 84, 255, 159 }, til::color{ 78, 238, 148 }, til::color{ 67, 205, 128 }, til::color{ 46, 139, 87 } } },
    std::pair{ "seashell"sv, std::array<til::color, 5>{ til::color{ 255, 245, 238 }, til::color{ 255, 245, 238 }, til::color{ 238, 229, 222 }, til::color{ 205, 197, 191 }, til::color{ 139, 134, 130 } } },
    std::pair{ "sienna"sv, std::array<til::color, 5>{ til::color{ 160, 82, 45 }, til::color{ 255, 130, 71 }, til::color{ 238, 121, 66 }, til::color{ 205, 104, 57 }, til::color{ 139, 71, 38 } } },
    std::pair{ "skyblue"sv, std::array<til::color, 5>{ til::color{ 135, 206, 235 }, til::color{ 135, 206, 255 }, til::color{ 126, 192, 238 }, til::color{ 108, 166, 205 }, til::color{ 74, 112, 139 } } },
    std::pair{ "slateblue"sv, std::array<til::color, 5>{ til::color{ 106, 90, 205 }, til::color{ 131, 111, 255 }, til::color{ 122, 103, 238 }, til::color{ 105, 89, 205 }, til::color{ 71, 60, 139 } } },
    std::pair{ "slategray"sv, std::array<til::color, 5>{ til::color{ 112, 128, 144 }, til::color{ 198, 226, 255 }, til::color{ 185, 211, 238 }, til::color{ 159, 182, 205 }, til::color{ 108, 123, 139 } } },
    std::pair{ "snow"sv, std::array<til::color, 5>{ til::color{ 255, 250, 250 }, til::color{ 255, 250, 250 }, til::color{ 238, 233, 233 }, til::color{ 205, 201, 201 }, til::color{ 139, 137, 137 } } },
    std::pair{ "springgreen"sv, std::array<til::color, 5>{ til::color{ 0, 255, 127 }, til::color{ 0, 255, 127 }, til::color{ 0, 238, 118 }, til::color{ 0, 205, 102 }, til::color{ 0, 139, 69 } } },
    std::pair{ "steelblue"sv, std::array<til::color, 5>{ til::color{ 70, 130, 180 }, til::color{ 99, 184, 255 }, til::color{ 92, 172, 238 }, til::color{ 79, 148, 205 }, til::color{ 54, 100, 139 } } },
    std::pair{ "tan"sv, std::array<til::color, 5>{ til::color{ 210, 180, 140 }, til::color{ 255, 165, 79 }, til::color{ 238, 154, 73 }, til::color{ 205, 133, 63 }, til::color{ 139, 90, 43 } } },
    std::pair{ "thistle"sv, std::array<til::color, 5>{ til::color{ 216, 191, 216 }, til::color{ 255, 225, 255 }, til::color{ 238, 210, 238 }, til::color{ 205, 181, 205 }, til::color{ 139, 123, 139 } } },
    std::pair{ "tomato"sv, std::array<til::color, 5>{ til::color{ 255, 99, 71 }, til::color{ 255, 99, 71 }, til::color{ 238, 92, 66 }, til::color{ 205, 79, 57 }, til::color{ 139, 54, 38 } } },
    std::pair{ "turquoise"sv, std::array<til::color, 5>{ til::color{ 64, 224, 208 }, til::color{ 0, 245, 255 }, til::color{ 0, 229, 238 }, til::color{ 0, 197, 205 }, til::color{ 0, 134, 139 } } },
    std::pair{ "violetred"sv, std::array<til::color, 5>{ til::color{ 208, 32, 144 }, til::color{ 255, 62, 150 }, til::color{ 238, 58, 140 }, til::color{ 205, 50, 120 }, til::color{ 139, 34, 82 } } },
    std::pair{ "wheat"sv, std::array<til::color, 5>{ til::color{ 245, 222, 179 }, til::color{ 255, 231, 186 }, til::color{ 238, 216, 174 }, til::color{ 205, 186, 150 }, til::color{ 139, 126, 102 } } },
    std::pair{ "yellow"sv, std::array<til::color, 5>{ til::color{ 255, 255, 0 }, til::color{ 255, 255, 0 }, til::color{ 238, 238, 0 }, til::color{ 205, 205, 0 }, til::color{ 139, 139, 0 } } },
};

static constinit til::presorted_static_map xorgAppColorTable{
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

std::span<const til::color> Utils::CampbellColorTable() noexcept
{
    return std::span{ standard256ColorTable }.first(16);
}

// Function Description:
// - Fill up to 256 entries of a given color table with the default values
// Arguments:
// - table: a color table to be filled
// Return Value:
// - <none>
void Utils::InitializeColorTable(const std::span<COLORREF> table)
{
    const auto tableSize = std::min(table.size(), standard256ColorTable.size());
    std::copy_n(standard256ColorTable.begin(), tableSize, table.begin());
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
    auto foundVariant = false;
    for (const auto c : wstr)
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

    auto name = ss.str();
    const auto variantColorIter = xorgAppVariantColorTable.find(name);
    if (variantColorIter != xorgAppVariantColorTable.end())
    {
        const auto colors = variantColorIter->second;
        if (variantIndex < colors.size())
        {
            return colors.at(variantIndex);
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
