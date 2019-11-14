// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/CodepointWidthDetector.hpp"

namespace
{
    // used to store range data in CodepointWidthDetector's internal map
    struct UnicodeRange final
    {
        unsigned int lowerBound;
        unsigned int upperBound;
        CodepointWidth width;
    };

    static bool operator<(const UnicodeRange& range, const unsigned int searchTerm) noexcept
    {
        return range.upperBound < searchTerm;
    }

    static constexpr std::array<UnicodeRange, 285> s_wideAndAmbiguousTable{
        // generated from http://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
        // anything not present here is presumed to be Narrow.
        UnicodeRange{ 0xa1, 0xa1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xa4, 0xa4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xa7, 0xa8, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xaa, 0xaa, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xad, 0xae, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xb0, 0xb4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xb6, 0xba, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xbc, 0xbf, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xc6, 0xc6, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xd0, 0xd0, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xd7, 0xd8, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xde, 0xe1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xe6, 0xe6, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xe8, 0xea, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xec, 0xed, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xf0, 0xf0, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xf2, 0xf3, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xf7, 0xfa, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xfc, 0xfc, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xfe, 0xfe, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x101, 0x101, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x111, 0x111, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x113, 0x113, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x11b, 0x11b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x126, 0x127, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x12b, 0x12b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x131, 0x133, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x138, 0x138, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x13f, 0x142, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x144, 0x144, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x148, 0x14b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x14d, 0x14d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x152, 0x153, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x166, 0x167, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x16b, 0x16b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1ce, 0x1ce, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1d0, 0x1d0, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1d2, 0x1d2, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1d4, 0x1d4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1d6, 0x1d6, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1d8, 0x1d8, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1da, 0x1da, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1dc, 0x1dc, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x251, 0x251, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x261, 0x261, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2c4, 0x2c4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2c7, 0x2c7, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2c9, 0x2cb, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2cd, 0x2cd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2d0, 0x2d0, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2d8, 0x2db, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2dd, 0x2dd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2df, 0x2df, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x300, 0x36f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x391, 0x3a1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x3a3, 0x3a9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x3b1, 0x3c1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x3c3, 0x3c9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x401, 0x401, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x410, 0x44f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x451, 0x451, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1100, 0x115f, CodepointWidth::Wide },
        UnicodeRange{ 0x2010, 0x2010, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2013, 0x2016, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2018, 0x2019, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x201c, 0x201d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2020, 0x2022, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2024, 0x2027, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2030, 0x2030, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2032, 0x2033, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2035, 0x2035, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x203b, 0x203b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x203e, 0x203e, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2074, 0x2074, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x207f, 0x207f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2081, 0x2084, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x20ac, 0x20ac, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2103, 0x2103, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2105, 0x2105, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2109, 0x2109, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2113, 0x2113, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2116, 0x2116, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2121, 0x2122, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2126, 0x2126, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x212b, 0x212b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2153, 0x2154, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x215b, 0x215e, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2160, 0x216b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2170, 0x2179, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2189, 0x2189, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2190, 0x2199, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x21b8, 0x21b9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x21d2, 0x21d2, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x21d4, 0x21d4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x21e7, 0x21e7, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2200, 0x2200, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2202, 0x2203, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2207, 0x2208, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x220b, 0x220b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x220f, 0x220f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2211, 0x2211, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2215, 0x2215, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x221a, 0x221a, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x221d, 0x2220, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2223, 0x2223, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2225, 0x2225, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2227, 0x222c, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x222e, 0x222e, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2234, 0x2237, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x223c, 0x223d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2248, 0x2248, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x224c, 0x224c, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2252, 0x2252, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2260, 0x2261, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2264, 0x2267, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x226a, 0x226b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x226e, 0x226f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2282, 0x2283, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2286, 0x2287, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2295, 0x2295, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2299, 0x2299, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x22a5, 0x22a5, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x22bf, 0x22bf, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2312, 0x2312, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x231a, 0x231b, CodepointWidth::Wide },
        UnicodeRange{ 0x2329, 0x232a, CodepointWidth::Wide },
        UnicodeRange{ 0x23e9, 0x23ec, CodepointWidth::Wide },
        UnicodeRange{ 0x23f0, 0x23f0, CodepointWidth::Wide },
        UnicodeRange{ 0x23f3, 0x23f3, CodepointWidth::Wide },
        UnicodeRange{ 0x2460, 0x24e9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x24eb, 0x254b, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2550, 0x2573, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2580, 0x258f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2592, 0x2595, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25a0, 0x25a1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25a3, 0x25a9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25b2, 0x25b3, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25b6, 0x25b7, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25bc, 0x25bd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25c0, 0x25c1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25c6, 0x25c8, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25cb, 0x25cb, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25ce, 0x25d1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25e2, 0x25e5, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25ef, 0x25ef, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x25fd, 0x25fe, CodepointWidth::Wide },
        UnicodeRange{ 0x2605, 0x2606, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2609, 0x2609, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x260e, 0x260f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2614, 0x2615, CodepointWidth::Wide },
        UnicodeRange{ 0x261c, 0x261c, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x261e, 0x261e, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2640, 0x2640, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2642, 0x2642, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2648, 0x2653, CodepointWidth::Wide },
        UnicodeRange{ 0x2660, 0x2661, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2663, 0x2665, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2667, 0x266a, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x266c, 0x266d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x266f, 0x266f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x267f, 0x267f, CodepointWidth::Wide },
        UnicodeRange{ 0x2693, 0x2693, CodepointWidth::Wide },
        UnicodeRange{ 0x269e, 0x269f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26a1, 0x26a1, CodepointWidth::Wide },
        UnicodeRange{ 0x26aa, 0x26ab, CodepointWidth::Wide },
        UnicodeRange{ 0x26bd, 0x26be, CodepointWidth::Wide },
        UnicodeRange{ 0x26bf, 0x26bf, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26c4, 0x26c5, CodepointWidth::Wide },
        UnicodeRange{ 0x26c6, 0x26cd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26ce, 0x26ce, CodepointWidth::Wide },
        UnicodeRange{ 0x26cf, 0x26d3, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26d4, 0x26d4, CodepointWidth::Wide },
        UnicodeRange{ 0x26d5, 0x26e1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26e3, 0x26e3, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26e8, 0x26e9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26ea, 0x26ea, CodepointWidth::Wide },
        UnicodeRange{ 0x26eb, 0x26f1, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26f2, 0x26f3, CodepointWidth::Wide },
        UnicodeRange{ 0x26f4, 0x26f4, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26f5, 0x26f5, CodepointWidth::Wide },
        UnicodeRange{ 0x26f6, 0x26f9, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26fa, 0x26fa, CodepointWidth::Wide },
        UnicodeRange{ 0x26fb, 0x26fc, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x26fd, 0x26fd, CodepointWidth::Wide },
        UnicodeRange{ 0x26fe, 0x26ff, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2705, 0x2705, CodepointWidth::Wide },
        UnicodeRange{ 0x270a, 0x270b, CodepointWidth::Wide },
        UnicodeRange{ 0x2728, 0x2728, CodepointWidth::Wide },
        UnicodeRange{ 0x273d, 0x273d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x274c, 0x274c, CodepointWidth::Wide },
        UnicodeRange{ 0x274e, 0x274e, CodepointWidth::Wide },
        UnicodeRange{ 0x2753, 0x2755, CodepointWidth::Wide },
        UnicodeRange{ 0x2757, 0x2757, CodepointWidth::Wide },
        UnicodeRange{ 0x2776, 0x277f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2795, 0x2797, CodepointWidth::Wide },
        UnicodeRange{ 0x27b0, 0x27b0, CodepointWidth::Wide },
        UnicodeRange{ 0x27bf, 0x27bf, CodepointWidth::Wide },
        UnicodeRange{ 0x2b1b, 0x2b1c, CodepointWidth::Wide },
        UnicodeRange{ 0x2b50, 0x2b50, CodepointWidth::Wide },
        UnicodeRange{ 0x2b55, 0x2b55, CodepointWidth::Wide },
        UnicodeRange{ 0x2b56, 0x2b59, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x2e80, 0x2e99, CodepointWidth::Wide },
        UnicodeRange{ 0x2e9b, 0x2ef3, CodepointWidth::Wide },
        UnicodeRange{ 0x2f00, 0x2fd5, CodepointWidth::Wide },
        UnicodeRange{ 0x2ff0, 0x2ffb, CodepointWidth::Wide },
        UnicodeRange{ 0x3000, 0x303e, CodepointWidth::Wide },
        UnicodeRange{ 0x3041, 0x3096, CodepointWidth::Wide },
        UnicodeRange{ 0x3099, 0x30ff, CodepointWidth::Wide },
        UnicodeRange{ 0x3105, 0x312e, CodepointWidth::Wide },
        UnicodeRange{ 0x3131, 0x318e, CodepointWidth::Wide },
        UnicodeRange{ 0x3190, 0x31ba, CodepointWidth::Wide },
        UnicodeRange{ 0x31c0, 0x31e3, CodepointWidth::Wide },
        UnicodeRange{ 0x31f0, 0x321e, CodepointWidth::Wide },
        UnicodeRange{ 0x3220, 0x3247, CodepointWidth::Wide },
        UnicodeRange{ 0x3248, 0x324f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x3250, 0x32fe, CodepointWidth::Wide },
        UnicodeRange{ 0x3300, 0x4dbf, CodepointWidth::Wide },
        UnicodeRange{ 0x4e00, 0xa48c, CodepointWidth::Wide },
        UnicodeRange{ 0xa490, 0xa4c6, CodepointWidth::Wide },
        UnicodeRange{ 0xa960, 0xa97c, CodepointWidth::Wide },
        UnicodeRange{ 0xac00, 0xd7a3, CodepointWidth::Wide },
        UnicodeRange{ 0xe000, 0xf8ff, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xf900, 0xfaff, CodepointWidth::Wide },
        UnicodeRange{ 0xfe00, 0xfe0f, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xfe10, 0xfe19, CodepointWidth::Wide },
        UnicodeRange{ 0xfe30, 0xfe52, CodepointWidth::Wide },
        UnicodeRange{ 0xfe54, 0xfe66, CodepointWidth::Wide },
        UnicodeRange{ 0xfe68, 0xfe6b, CodepointWidth::Wide },
        UnicodeRange{ 0xff01, 0xff60, CodepointWidth::Wide },
        UnicodeRange{ 0xffe0, 0xffe6, CodepointWidth::Wide },
        UnicodeRange{ 0xfffd, 0xfffd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x16fe0, 0x16fe1, CodepointWidth::Wide },
        UnicodeRange{ 0x17000, 0x187ec, CodepointWidth::Wide },
        UnicodeRange{ 0x18800, 0x18af2, CodepointWidth::Wide },
        UnicodeRange{ 0x1b000, 0x1b11e, CodepointWidth::Wide },
        UnicodeRange{ 0x1b170, 0x1b2fb, CodepointWidth::Wide },
        UnicodeRange{ 0x1f004, 0x1f004, CodepointWidth::Wide },
        UnicodeRange{ 0x1f0cf, 0x1f0cf, CodepointWidth::Wide },
        UnicodeRange{ 0x1f100, 0x1f10a, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f110, 0x1f12d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f130, 0x1f169, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f170, 0x1f18d, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f18e, 0x1f18e, CodepointWidth::Wide },
        UnicodeRange{ 0x1f18f, 0x1f190, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f191, 0x1f19a, CodepointWidth::Wide },
        UnicodeRange{ 0x1f19b, 0x1f1ac, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x1f200, 0x1f202, CodepointWidth::Wide },
        UnicodeRange{ 0x1f210, 0x1f23b, CodepointWidth::Wide },
        UnicodeRange{ 0x1f240, 0x1f248, CodepointWidth::Wide },
        UnicodeRange{ 0x1f250, 0x1f251, CodepointWidth::Wide },
        UnicodeRange{ 0x1f260, 0x1f265, CodepointWidth::Wide },
        UnicodeRange{ 0x1f300, 0x1f320, CodepointWidth::Wide },
        UnicodeRange{ 0x1f32d, 0x1f335, CodepointWidth::Wide },
        UnicodeRange{ 0x1f337, 0x1f37c, CodepointWidth::Wide },
        UnicodeRange{ 0x1f37e, 0x1f393, CodepointWidth::Wide },
        UnicodeRange{ 0x1f3a0, 0x1f3ca, CodepointWidth::Wide },
        UnicodeRange{ 0x1f3cf, 0x1f3d3, CodepointWidth::Wide },
        UnicodeRange{ 0x1f3e0, 0x1f3f0, CodepointWidth::Wide },
        UnicodeRange{ 0x1f3f4, 0x1f3f4, CodepointWidth::Wide },
        UnicodeRange{ 0x1f3f8, 0x1f43e, CodepointWidth::Wide },
        UnicodeRange{ 0x1f440, 0x1f440, CodepointWidth::Wide },
        UnicodeRange{ 0x1f442, 0x1f4fc, CodepointWidth::Wide },
        UnicodeRange{ 0x1f4ff, 0x1f53d, CodepointWidth::Wide },
        UnicodeRange{ 0x1f54b, 0x1f54e, CodepointWidth::Wide },
        UnicodeRange{ 0x1f550, 0x1f567, CodepointWidth::Wide },
        UnicodeRange{ 0x1f57a, 0x1f57a, CodepointWidth::Wide },
        UnicodeRange{ 0x1f595, 0x1f596, CodepointWidth::Wide },
        UnicodeRange{ 0x1f5a4, 0x1f5a4, CodepointWidth::Wide },
        UnicodeRange{ 0x1f5fb, 0x1f64f, CodepointWidth::Wide },
        UnicodeRange{ 0x1f680, 0x1f6c5, CodepointWidth::Wide },
        UnicodeRange{ 0x1f6cc, 0x1f6cc, CodepointWidth::Wide },
        UnicodeRange{ 0x1f6d0, 0x1f6d2, CodepointWidth::Wide },
        UnicodeRange{ 0x1f6eb, 0x1f6ec, CodepointWidth::Wide },
        UnicodeRange{ 0x1f6f4, 0x1f6f8, CodepointWidth::Wide },
        UnicodeRange{ 0x1f910, 0x1f93e, CodepointWidth::Wide },
        UnicodeRange{ 0x1f940, 0x1f94c, CodepointWidth::Wide },
        UnicodeRange{ 0x1f950, 0x1f96b, CodepointWidth::Wide },
        UnicodeRange{ 0x1f980, 0x1f997, CodepointWidth::Wide },
        UnicodeRange{ 0x1f9c0, 0x1f9c0, CodepointWidth::Wide },
        UnicodeRange{ 0x1f9d0, 0x1f9e6, CodepointWidth::Wide },
        UnicodeRange{ 0x20000, 0x2fffd, CodepointWidth::Wide },
        UnicodeRange{ 0x30000, 0x3fffd, CodepointWidth::Wide },
        UnicodeRange{ 0xe0100, 0xe01ef, CodepointWidth::Ambiguous },
        UnicodeRange{ 0xf0000, 0xffffd, CodepointWidth::Ambiguous },
        UnicodeRange{ 0x100000, 0x10fffd, CodepointWidth::Ambiguous }
    };
}

// Routine Description:
// - Constructs an instance of the CodepointWidthDetector class
CodepointWidthDetector::CodepointWidthDetector() noexcept :
    _fallbackCache{},
    _pfnFallbackMethod{}
{
}

// Routine Description:
// - returns the width type of codepoint by searching the map generated from the unicode spec
// Arguments:
// - glyph - the utf16 encoded codepoint to search for
// Return Value:
// - the width type of the codepoint
CodepointWidth CodepointWidthDetector::GetWidth(const std::wstring_view glyph) const
{
    if (glyph.empty())
    {
        return CodepointWidth::Invalid;
    }

    const auto codepoint = _extractCodepoint(glyph);
    const auto it = std::lower_bound(s_wideAndAmbiguousTable.begin(), s_wideAndAmbiguousTable.end(), codepoint);

    // For characters that are not _in_ the table, lower_bound will return the nearest item that is.
    // We must check its bounds to make sure that our hit was a true hit.
    if (it != s_wideAndAmbiguousTable.end() && codepoint >= it->lowerBound && codepoint <= it->upperBound)
    {
        return it->width;
    }

    return CodepointWidth::Narrow;
}

// Routine Description:
// - checks if wch is wide. will attempt to fallback as much possible until an answer is determined
// Arguments:
// - wch - the wchar to check width of
// Return Value:
// - true if wch is wide
bool CodepointWidthDetector::IsWide(const wchar_t wch) const noexcept
{
    try
    {
        return IsWide({ &wch, 1 });
    }
    CATCH_LOG();

    return true;
}

// Routine Description:
// - checks if codepoint is wide. will attempt to fallback as much possible until an answer is determined
// Arguments:
// - glyph - the utf16 encoded codepoint to check width of
// Return Value:
// - true if codepoint is wide
bool CodepointWidthDetector::IsWide(const std::wstring_view glyph) const
{
    THROW_HR_IF(E_INVALIDARG, glyph.empty());
    if (glyph.size() == 1)
    {
        // We first attempt to look at our custom quick lookup table of char width preferences.
        const auto width = GetQuickCharWidth(glyph.front());

        // If it's invalid, the quick width had no opinion, so go to the lookup table.
        if (width == CodepointWidth::Invalid)
        {
            return _lookupIsWide(glyph);
        }
        // If it's ambiguous, the quick width wanted us to ask the font directly, try that if we can.
        // If not, go to the lookup table.
        else if (width == CodepointWidth::Ambiguous)
        {
            if (_pfnFallbackMethod)
            {
                return _checkFallbackViaCache(glyph);
            }
            else
            {
                return _lookupIsWide(glyph);
            }
        }
        // Otherwise, return Wide as True and Narrow as False.
        else
        {
            return width == CodepointWidth::Wide;
        }
    }
    else
    {
        return _lookupIsWide(glyph);
    }
}

// Routine Description:
// - checks if codepoint is wide using fallback methods.
// Arguments:
// - glyph - the utf16 encoded codepoint to check width of
// Return Value:
// - true if codepoint is wide or if it can't be confirmed to be narrow
bool CodepointWidthDetector::_lookupIsWide(const std::wstring_view glyph) const noexcept
{
    try
    {
        // Use our generated table to try to lookup the width based on the Unicode standard.
        const CodepointWidth width = GetWidth(glyph);

        // If it's ambiguous, then ask the font if we can.
        if (width == CodepointWidth::Ambiguous)
        {
            if (_pfnFallbackMethod)
            {
                return _checkFallbackViaCache(glyph);
            }
        }
        // If it's not ambiguous, it should say wide or narrow. Turn that into True = Wide or False = Narrow.
        else
        {
            return width == CodepointWidth::Wide;
        }
    }
    CATCH_LOG();

    // If we got this far, we couldn't figure it out.
    // It's better to be too wide than too narrow.
    return true;
}

// Routine Description:
// - Checks the fallback function but caches the results until the font changes
//   because the lookup function is usually very expensive and will return the same results
//   for the same inputs.
// Arguments:
// - glyph - the utf16 encoded codepoint to check width of
// - true if codepoint is wide or false if it is narrow
bool CodepointWidthDetector::_checkFallbackViaCache(const std::wstring_view glyph) const
{
    const std::wstring findMe{ glyph };

    // TODO: Cache needs to be emptied when font changes.
    const auto it = _fallbackCache.find(findMe);
    if (it == _fallbackCache.end())
    {
        auto result = _pfnFallbackMethod(glyph);
        _fallbackCache.insert_or_assign(findMe, result);
        return result;
    }
    else
    {
        return it->second;
    }
}

// Routine Description:
// - extract unicode codepoint from utf16 encoding
// Arguments:
// - glyph - the utf16 encoded codepoint convert
// Return Value:
// - the codepoint being stored
unsigned int CodepointWidthDetector::_extractCodepoint(const std::wstring_view glyph) noexcept
{
    if (glyph.size() == 1)
    {
        return static_cast<unsigned int>(glyph.front());
    }
    else
    {
        const unsigned int mask = 0x3FF;
        // leading bits, shifted over to make space for trailing bits
        unsigned int codepoint = (glyph.at(0) & mask) << 10;
        // trailing bits
        codepoint |= (glyph.at(1) & mask);
        // 0x10000 is subtracted from the codepoint to encode a surrogate pair, add it back
        codepoint += 0x10000;
        return codepoint;
    }
}

// Method Description:
// - Sets a function that should be used as the fallback mechanism for
//      determining a particular glyph's width, should the glyph be an ambiguous
//      width.
//   A Terminal could hook in a Renderer's IsGlyphWideByFont method as the
//      fallback to ask the renderer for the glyph's width (for example).
// Arguments:
// - pfnFallback - the function to use as the fallback method.
// Return Value:
// - <none>
void CodepointWidthDetector::SetFallbackMethod(std::function<bool(const std::wstring_view)> pfnFallback)
{
    _pfnFallbackMethod = pfnFallback;
}

// Method Description:
// - Resets the internal ambiguous character width cache mechanism
//   since it will be different when the font changes and we should
//   re-query the new font for that information.
// Arguments:
// - <none>
// Return Value:
// - <none>
void CodepointWidthDetector::NotifyFontChanged() const noexcept
{
    _fallbackCache.clear();
}
