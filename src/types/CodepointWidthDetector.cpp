// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/CodepointWidthDetector.hpp"

// Routine Description:
// - returns the width type of codepoint by searching the map generated from the unicode spec
// Arguments:
// - glyph - the utf16 encoded codepoint to search for
// Return Value:
// - the width type of the codepoint
CodepointWidth CodepointWidthDetector::GetWidth(const std::wstring_view glyph) const noexcept
{
    if (glyph.empty())
    {
        return CodepointWidth::Invalid;
    }

    if (_map.empty())
    {
        const_cast<CodepointWidthDetector* const>(this)->_populateUnicodeSearchMap();
    }

    const auto codepoint = _extractCodepoint(glyph);
    UnicodeRange search{ codepoint };
    auto it = _map.find(search);
    if (it == _map.end())
    {
        return CodepointWidth::Invalid;
    }
    else
    {
        return it->second;
    }
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
            if (_hasFallback)
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
            if (_hasFallback)
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
    auto it = _fallbackCache.find(findMe);
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
unsigned int CodepointWidthDetector::_extractCodepoint(const std::wstring_view glyph) const noexcept
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
    _hasFallback = true;
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

void CodepointWidthDetector::_populateUnicodeSearchMap()
{
    // generated from http://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt
    _map[UnicodeRange(0, 160)] = CodepointWidth::Narrow;
    _map[UnicodeRange(161, 161)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(162, 163)] = CodepointWidth::Narrow;
    _map[UnicodeRange(164, 164)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(165, 166)] = CodepointWidth::Narrow;
    _map[UnicodeRange(167, 168)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(169, 169)] = CodepointWidth::Narrow;
    _map[UnicodeRange(170, 170)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(171, 172)] = CodepointWidth::Narrow;
    _map[UnicodeRange(173, 174)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(175, 175)] = CodepointWidth::Narrow;
    _map[UnicodeRange(176, 180)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(181, 181)] = CodepointWidth::Narrow;
    _map[UnicodeRange(182, 186)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(187, 187)] = CodepointWidth::Narrow;
    _map[UnicodeRange(188, 191)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(192, 197)] = CodepointWidth::Narrow;
    _map[UnicodeRange(198, 198)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(199, 207)] = CodepointWidth::Narrow;
    _map[UnicodeRange(208, 208)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(209, 214)] = CodepointWidth::Narrow;
    _map[UnicodeRange(215, 216)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(217, 221)] = CodepointWidth::Narrow;
    _map[UnicodeRange(222, 225)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(226, 229)] = CodepointWidth::Narrow;
    _map[UnicodeRange(230, 230)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(231, 231)] = CodepointWidth::Narrow;
    _map[UnicodeRange(232, 234)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(235, 235)] = CodepointWidth::Narrow;
    _map[UnicodeRange(236, 237)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(238, 239)] = CodepointWidth::Narrow;
    _map[UnicodeRange(240, 240)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(241, 241)] = CodepointWidth::Narrow;
    _map[UnicodeRange(242, 243)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(244, 246)] = CodepointWidth::Narrow;
    _map[UnicodeRange(247, 250)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(251, 251)] = CodepointWidth::Narrow;
    _map[UnicodeRange(252, 252)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(253, 253)] = CodepointWidth::Narrow;
    _map[UnicodeRange(254, 254)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(255, 256)] = CodepointWidth::Narrow;
    _map[UnicodeRange(257, 257)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(258, 272)] = CodepointWidth::Narrow;
    _map[UnicodeRange(273, 273)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(274, 274)] = CodepointWidth::Narrow;
    _map[UnicodeRange(275, 275)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(276, 282)] = CodepointWidth::Narrow;
    _map[UnicodeRange(283, 283)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(284, 293)] = CodepointWidth::Narrow;
    _map[UnicodeRange(294, 295)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(296, 298)] = CodepointWidth::Narrow;
    _map[UnicodeRange(299, 299)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(300, 304)] = CodepointWidth::Narrow;
    _map[UnicodeRange(305, 307)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(308, 311)] = CodepointWidth::Narrow;
    _map[UnicodeRange(312, 312)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(313, 318)] = CodepointWidth::Narrow;
    _map[UnicodeRange(319, 322)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(323, 323)] = CodepointWidth::Narrow;
    _map[UnicodeRange(324, 324)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(325, 327)] = CodepointWidth::Narrow;
    _map[UnicodeRange(328, 331)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(332, 332)] = CodepointWidth::Narrow;
    _map[UnicodeRange(333, 333)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(334, 337)] = CodepointWidth::Narrow;
    _map[UnicodeRange(338, 339)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(340, 357)] = CodepointWidth::Narrow;
    _map[UnicodeRange(358, 359)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(360, 362)] = CodepointWidth::Narrow;
    _map[UnicodeRange(363, 363)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(364, 461)] = CodepointWidth::Narrow;
    _map[UnicodeRange(462, 462)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(463, 463)] = CodepointWidth::Narrow;
    _map[UnicodeRange(464, 464)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(465, 465)] = CodepointWidth::Narrow;
    _map[UnicodeRange(466, 466)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(467, 467)] = CodepointWidth::Narrow;
    _map[UnicodeRange(468, 468)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(469, 469)] = CodepointWidth::Narrow;
    _map[UnicodeRange(470, 470)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(471, 471)] = CodepointWidth::Narrow;
    _map[UnicodeRange(472, 472)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(473, 473)] = CodepointWidth::Narrow;
    _map[UnicodeRange(474, 474)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(475, 475)] = CodepointWidth::Narrow;
    _map[UnicodeRange(476, 476)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(477, 592)] = CodepointWidth::Narrow;
    _map[UnicodeRange(593, 593)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(594, 608)] = CodepointWidth::Narrow;
    _map[UnicodeRange(609, 609)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(610, 707)] = CodepointWidth::Narrow;
    _map[UnicodeRange(708, 708)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(709, 710)] = CodepointWidth::Narrow;
    _map[UnicodeRange(711, 711)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(712, 712)] = CodepointWidth::Narrow;
    _map[UnicodeRange(713, 715)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(716, 716)] = CodepointWidth::Narrow;
    _map[UnicodeRange(717, 717)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(718, 719)] = CodepointWidth::Narrow;
    _map[UnicodeRange(720, 720)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(721, 727)] = CodepointWidth::Narrow;
    _map[UnicodeRange(728, 731)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(732, 732)] = CodepointWidth::Narrow;
    _map[UnicodeRange(733, 733)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(734, 734)] = CodepointWidth::Narrow;
    _map[UnicodeRange(735, 735)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(736, 767)] = CodepointWidth::Narrow;
    _map[UnicodeRange(768, 879)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(880, 887)] = CodepointWidth::Narrow;
    _map[UnicodeRange(890, 895)] = CodepointWidth::Narrow;
    _map[UnicodeRange(900, 906)] = CodepointWidth::Narrow;
    _map[UnicodeRange(908, 908)] = CodepointWidth::Narrow;
    _map[UnicodeRange(910, 912)] = CodepointWidth::Narrow;
    _map[UnicodeRange(913, 929)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(931, 937)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(938, 944)] = CodepointWidth::Narrow;
    _map[UnicodeRange(945, 961)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(962, 962)] = CodepointWidth::Narrow;
    _map[UnicodeRange(963, 969)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(970, 1024)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1025, 1025)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(1026, 1039)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1040, 1103)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(1104, 1104)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1105, 1105)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(1106, 1327)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1329, 1366)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1369, 1375)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1377, 1415)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1417, 1418)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1421, 1423)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1425, 1479)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1488, 1514)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1520, 1524)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1536, 1564)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1566, 1805)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1807, 1866)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1869, 1969)] = CodepointWidth::Narrow;
    _map[UnicodeRange(1984, 2042)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2048, 2093)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2096, 2110)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2112, 2139)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2142, 2142)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2144, 2154)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2208, 2228)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2230, 2237)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2260, 2435)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2437, 2444)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2447, 2448)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2451, 2472)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2474, 2480)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2482, 2482)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2486, 2489)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2492, 2500)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2503, 2504)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2507, 2510)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2519, 2519)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2524, 2525)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2527, 2531)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2534, 2557)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2561, 2563)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2565, 2570)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2575, 2576)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2579, 2600)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2602, 2608)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2610, 2611)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2613, 2614)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2616, 2617)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2620, 2620)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2622, 2626)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2631, 2632)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2635, 2637)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2641, 2641)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2649, 2652)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2654, 2654)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2662, 2677)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2689, 2691)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2693, 2701)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2703, 2705)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2707, 2728)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2730, 2736)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2738, 2739)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2741, 2745)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2748, 2757)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2759, 2761)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2763, 2765)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2768, 2768)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2784, 2787)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2790, 2801)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2809, 2815)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2817, 2819)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2821, 2828)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2831, 2832)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2835, 2856)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2858, 2864)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2866, 2867)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2869, 2873)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2876, 2884)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2887, 2888)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2891, 2893)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2902, 2903)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2908, 2909)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2911, 2915)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2918, 2935)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2946, 2947)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2949, 2954)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2958, 2960)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2962, 2965)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2969, 2970)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2972, 2972)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2974, 2975)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2979, 2980)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2984, 2986)] = CodepointWidth::Narrow;
    _map[UnicodeRange(2990, 3001)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3006, 3010)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3014, 3016)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3018, 3021)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3024, 3024)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3031, 3031)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3046, 3066)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3072, 3075)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3077, 3084)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3086, 3088)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3090, 3112)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3114, 3129)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3133, 3140)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3142, 3144)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3146, 3149)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3157, 3158)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3160, 3162)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3168, 3171)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3174, 3183)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3192, 3203)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3205, 3212)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3214, 3216)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3218, 3240)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3242, 3251)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3253, 3257)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3260, 3268)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3270, 3272)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3274, 3277)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3285, 3286)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3294, 3294)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3296, 3299)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3302, 3311)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3313, 3314)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3328, 3331)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3333, 3340)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3342, 3344)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3346, 3396)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3398, 3400)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3402, 3407)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3412, 3427)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3430, 3455)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3458, 3459)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3461, 3478)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3482, 3505)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3507, 3515)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3517, 3517)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3520, 3526)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3530, 3530)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3535, 3540)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3542, 3542)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3544, 3551)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3558, 3567)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3570, 3572)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3585, 3642)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3647, 3675)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3713, 3714)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3716, 3716)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3719, 3720)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3722, 3722)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3725, 3725)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3732, 3735)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3737, 3743)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3745, 3747)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3749, 3749)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3751, 3751)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3754, 3755)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3757, 3769)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3771, 3773)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3776, 3780)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3782, 3782)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3784, 3789)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3792, 3801)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3804, 3807)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3840, 3911)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3913, 3948)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3953, 3991)] = CodepointWidth::Narrow;
    _map[UnicodeRange(3993, 4028)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4030, 4044)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4046, 4058)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4096, 4293)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4295, 4295)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4301, 4301)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4304, 4351)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4352, 4447)] = CodepointWidth::Wide;
    _map[UnicodeRange(4448, 4680)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4682, 4685)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4688, 4694)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4696, 4696)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4698, 4701)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4704, 4744)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4746, 4749)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4752, 4784)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4786, 4789)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4792, 4798)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4800, 4800)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4802, 4805)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4808, 4822)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4824, 4880)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4882, 4885)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4888, 4954)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4957, 4988)] = CodepointWidth::Narrow;
    _map[UnicodeRange(4992, 5017)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5024, 5109)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5112, 5117)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5120, 5788)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5792, 5880)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5888, 5900)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5902, 5908)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5920, 5942)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5952, 5971)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5984, 5996)] = CodepointWidth::Narrow;
    _map[UnicodeRange(5998, 6000)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6002, 6003)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6016, 6109)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6112, 6121)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6128, 6137)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6144, 6158)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6160, 6169)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6176, 6263)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6272, 6314)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6320, 6389)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6400, 6430)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6432, 6443)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6448, 6459)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6464, 6464)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6468, 6509)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6512, 6516)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6528, 6571)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6576, 6601)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6608, 6618)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6622, 6683)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6686, 6750)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6752, 6780)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6783, 6793)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6800, 6809)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6816, 6829)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6832, 6846)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6912, 6987)] = CodepointWidth::Narrow;
    _map[UnicodeRange(6992, 7036)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7040, 7155)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7164, 7223)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7227, 7241)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7245, 7304)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7360, 7367)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7376, 7417)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7424, 7673)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7675, 7957)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7960, 7965)] = CodepointWidth::Narrow;
    _map[UnicodeRange(7968, 8005)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8008, 8013)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8016, 8023)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8025, 8025)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8027, 8027)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8029, 8029)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8031, 8061)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8064, 8116)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8118, 8132)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8134, 8147)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8150, 8155)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8157, 8175)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8178, 8180)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8182, 8190)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8192, 8207)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8208, 8208)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8209, 8210)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8211, 8214)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8215, 8215)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8216, 8217)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8218, 8219)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8220, 8221)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8222, 8223)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8224, 8226)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8227, 8227)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8228, 8231)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8232, 8239)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8240, 8240)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8241, 8241)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8242, 8243)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8244, 8244)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8245, 8245)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8246, 8250)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8251, 8251)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8252, 8253)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8254, 8254)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8255, 8292)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8294, 8305)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8308, 8308)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8309, 8318)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8319, 8319)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8320, 8320)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8321, 8324)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8325, 8334)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8336, 8348)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8352, 8363)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8364, 8364)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8365, 8383)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8400, 8432)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8448, 8450)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8451, 8451)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8452, 8452)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8453, 8453)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8454, 8456)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8457, 8457)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8458, 8466)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8467, 8467)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8468, 8469)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8470, 8470)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8471, 8480)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8481, 8482)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8483, 8485)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8486, 8486)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8487, 8490)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8491, 8491)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8492, 8530)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8531, 8532)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8533, 8538)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8539, 8542)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8543, 8543)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8544, 8555)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8556, 8559)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8560, 8569)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8570, 8584)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8585, 8585)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8586, 8587)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8592, 8601)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8602, 8631)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8632, 8633)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8634, 8657)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8658, 8658)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8659, 8659)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8660, 8660)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8661, 8678)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8679, 8679)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8680, 8703)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8704, 8704)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8705, 8705)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8706, 8707)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8708, 8710)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8711, 8712)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8713, 8714)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8715, 8715)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8716, 8718)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8719, 8719)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8720, 8720)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8721, 8721)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8722, 8724)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8725, 8725)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8726, 8729)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8730, 8730)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8731, 8732)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8733, 8736)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8737, 8738)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8739, 8739)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8740, 8740)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8741, 8741)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8742, 8742)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8743, 8748)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8749, 8749)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8750, 8750)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8751, 8755)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8756, 8759)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8760, 8763)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8764, 8765)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8766, 8775)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8776, 8776)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8777, 8779)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8780, 8780)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8781, 8785)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8786, 8786)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8787, 8799)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8800, 8801)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8802, 8803)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8804, 8807)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8808, 8809)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8810, 8811)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8812, 8813)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8814, 8815)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8816, 8833)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8834, 8835)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8836, 8837)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8838, 8839)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8840, 8852)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8853, 8853)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8854, 8856)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8857, 8857)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8858, 8868)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8869, 8869)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8870, 8894)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8895, 8895)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8896, 8977)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8978, 8978)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(8979, 8985)] = CodepointWidth::Narrow;
    _map[UnicodeRange(8986, 8987)] = CodepointWidth::Wide;
    _map[UnicodeRange(8988, 9000)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9001, 9002)] = CodepointWidth::Wide;
    _map[UnicodeRange(9003, 9192)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9193, 9196)] = CodepointWidth::Wide;
    _map[UnicodeRange(9197, 9199)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9200, 9200)] = CodepointWidth::Wide;
    _map[UnicodeRange(9201, 9202)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9203, 9203)] = CodepointWidth::Wide;
    _map[UnicodeRange(9204, 9254)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9280, 9290)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9312, 9449)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9450, 9450)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9451, 9547)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9548, 9551)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9552, 9587)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9588, 9599)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9600, 9615)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9616, 9617)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9618, 9621)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9622, 9631)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9632, 9633)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9634, 9634)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9635, 9641)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9642, 9649)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9650, 9651)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9652, 9653)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9654, 9655)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9656, 9659)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9660, 9661)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9662, 9663)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9664, 9665)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9666, 9669)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9670, 9672)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9673, 9674)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9675, 9675)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9676, 9677)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9678, 9681)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9682, 9697)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9698, 9701)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9702, 9710)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9711, 9711)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9712, 9724)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9725, 9726)] = CodepointWidth::Wide;
    _map[UnicodeRange(9727, 9732)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9733, 9734)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9735, 9736)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9737, 9737)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9738, 9741)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9742, 9743)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9744, 9747)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9748, 9749)] = CodepointWidth::Wide;
    _map[UnicodeRange(9750, 9755)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9756, 9756)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9757, 9757)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9758, 9758)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9759, 9791)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9792, 9792)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9793, 9793)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9794, 9794)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9795, 9799)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9800, 9811)] = CodepointWidth::Wide;
    _map[UnicodeRange(9812, 9823)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9824, 9825)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9826, 9826)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9827, 9829)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9830, 9830)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9831, 9834)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9835, 9835)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9836, 9837)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9838, 9838)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9839, 9839)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9840, 9854)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9855, 9855)] = CodepointWidth::Wide;
    _map[UnicodeRange(9856, 9874)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9875, 9875)] = CodepointWidth::Wide;
    _map[UnicodeRange(9876, 9885)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9886, 9887)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9888, 9888)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9889, 9889)] = CodepointWidth::Wide;
    _map[UnicodeRange(9890, 9897)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9898, 9899)] = CodepointWidth::Wide;
    _map[UnicodeRange(9900, 9916)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9917, 9918)] = CodepointWidth::Wide;
    _map[UnicodeRange(9919, 9919)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9920, 9923)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9924, 9925)] = CodepointWidth::Wide;
    _map[UnicodeRange(9926, 9933)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9934, 9934)] = CodepointWidth::Wide;
    _map[UnicodeRange(9935, 9939)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9940, 9940)] = CodepointWidth::Wide;
    _map[UnicodeRange(9941, 9953)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9954, 9954)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9955, 9955)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9956, 9959)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9960, 9961)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9962, 9962)] = CodepointWidth::Wide;
    _map[UnicodeRange(9963, 9969)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9970, 9971)] = CodepointWidth::Wide;
    _map[UnicodeRange(9972, 9972)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9973, 9973)] = CodepointWidth::Wide;
    _map[UnicodeRange(9974, 9977)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9978, 9978)] = CodepointWidth::Wide;
    _map[UnicodeRange(9979, 9980)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9981, 9981)] = CodepointWidth::Wide;
    _map[UnicodeRange(9982, 9983)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(9984, 9988)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9989, 9989)] = CodepointWidth::Wide;
    _map[UnicodeRange(9990, 9993)] = CodepointWidth::Narrow;
    _map[UnicodeRange(9994, 9995)] = CodepointWidth::Wide;
    _map[UnicodeRange(9996, 10023)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10024, 10024)] = CodepointWidth::Wide;
    _map[UnicodeRange(10025, 10044)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10045, 10045)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(10046, 10059)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10060, 10060)] = CodepointWidth::Wide;
    _map[UnicodeRange(10061, 10061)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10062, 10062)] = CodepointWidth::Wide;
    _map[UnicodeRange(10063, 10066)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10067, 10069)] = CodepointWidth::Wide;
    _map[UnicodeRange(10070, 10070)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10071, 10071)] = CodepointWidth::Wide;
    _map[UnicodeRange(10072, 10101)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10102, 10111)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(10112, 10132)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10133, 10135)] = CodepointWidth::Wide;
    _map[UnicodeRange(10136, 10159)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10160, 10160)] = CodepointWidth::Wide;
    _map[UnicodeRange(10161, 10174)] = CodepointWidth::Narrow;
    _map[UnicodeRange(10175, 10175)] = CodepointWidth::Wide;
    _map[UnicodeRange(10176, 11034)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11035, 11036)] = CodepointWidth::Wide;
    _map[UnicodeRange(11037, 11087)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11088, 11088)] = CodepointWidth::Wide;
    _map[UnicodeRange(11089, 11092)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11093, 11093)] = CodepointWidth::Wide;
    _map[UnicodeRange(11094, 11097)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(11098, 11123)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11126, 11157)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11160, 11193)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11197, 11208)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11210, 11218)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11244, 11247)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11264, 11310)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11312, 11358)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11360, 11507)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11513, 11557)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11559, 11559)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11565, 11565)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11568, 11623)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11631, 11632)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11647, 11670)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11680, 11686)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11688, 11694)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11696, 11702)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11704, 11710)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11712, 11718)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11720, 11726)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11728, 11734)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11736, 11742)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11744, 11849)] = CodepointWidth::Narrow;
    _map[UnicodeRange(11904, 11929)] = CodepointWidth::Wide;
    _map[UnicodeRange(11931, 12019)] = CodepointWidth::Wide;
    _map[UnicodeRange(12032, 12245)] = CodepointWidth::Wide;
    _map[UnicodeRange(12272, 12283)] = CodepointWidth::Wide;
    _map[UnicodeRange(12288, 12350)] = CodepointWidth::Wide;
    _map[UnicodeRange(12351, 12351)] = CodepointWidth::Narrow;
    _map[UnicodeRange(12353, 12438)] = CodepointWidth::Wide;
    _map[UnicodeRange(12441, 12543)] = CodepointWidth::Wide;
    _map[UnicodeRange(12549, 12590)] = CodepointWidth::Wide;
    _map[UnicodeRange(12593, 12686)] = CodepointWidth::Wide;
    _map[UnicodeRange(12688, 12730)] = CodepointWidth::Wide;
    _map[UnicodeRange(12736, 12771)] = CodepointWidth::Wide;
    _map[UnicodeRange(12784, 12830)] = CodepointWidth::Wide;
    _map[UnicodeRange(12832, 12871)] = CodepointWidth::Wide;
    _map[UnicodeRange(12872, 12879)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(12880, 13054)] = CodepointWidth::Wide;
    _map[UnicodeRange(13056, 19903)] = CodepointWidth::Wide;
    _map[UnicodeRange(19904, 19967)] = CodepointWidth::Narrow;
    _map[UnicodeRange(19968, 42124)] = CodepointWidth::Wide;
    _map[UnicodeRange(42128, 42182)] = CodepointWidth::Wide;
    _map[UnicodeRange(42192, 42539)] = CodepointWidth::Narrow;
    _map[UnicodeRange(42560, 42743)] = CodepointWidth::Narrow;
    _map[UnicodeRange(42752, 42926)] = CodepointWidth::Narrow;
    _map[UnicodeRange(42928, 42935)] = CodepointWidth::Narrow;
    _map[UnicodeRange(42999, 43051)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43056, 43065)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43072, 43127)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43136, 43205)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43214, 43225)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43232, 43261)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43264, 43347)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43359, 43359)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43360, 43388)] = CodepointWidth::Wide;
    _map[UnicodeRange(43392, 43469)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43471, 43481)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43486, 43518)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43520, 43574)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43584, 43597)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43600, 43609)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43612, 43714)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43739, 43766)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43777, 43782)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43785, 43790)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43793, 43798)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43808, 43814)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43816, 43822)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43824, 43877)] = CodepointWidth::Narrow;
    _map[UnicodeRange(43888, 44013)] = CodepointWidth::Narrow;
    _map[UnicodeRange(44016, 44025)] = CodepointWidth::Narrow;
    _map[UnicodeRange(44032, 55203)] = CodepointWidth::Wide;
    _map[UnicodeRange(55216, 55238)] = CodepointWidth::Narrow;
    _map[UnicodeRange(55243, 55291)] = CodepointWidth::Narrow;
    _map[UnicodeRange(55296, 57343)] = CodepointWidth::Narrow;
    _map[UnicodeRange(57344, 63743)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(63744, 64255)] = CodepointWidth::Wide;
    _map[UnicodeRange(64256, 64262)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64275, 64279)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64285, 64310)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64312, 64316)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64318, 64318)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64320, 64321)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64323, 64324)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64326, 64449)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64467, 64831)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64848, 64911)] = CodepointWidth::Narrow;
    _map[UnicodeRange(64914, 64967)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65008, 65021)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65024, 65039)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(65040, 65049)] = CodepointWidth::Wide;
    _map[UnicodeRange(65056, 65071)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65072, 65106)] = CodepointWidth::Wide;
    _map[UnicodeRange(65108, 65126)] = CodepointWidth::Wide;
    _map[UnicodeRange(65128, 65131)] = CodepointWidth::Wide;
    _map[UnicodeRange(65136, 65140)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65142, 65276)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65279, 65279)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65281, 65376)] = CodepointWidth::Wide;
    _map[UnicodeRange(65377, 65470)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65474, 65479)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65482, 65487)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65490, 65495)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65498, 65500)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65504, 65510)] = CodepointWidth::Wide;
    _map[UnicodeRange(65512, 65518)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65529, 65532)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65533, 65533)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(65536, 65547)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65549, 65574)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65576, 65594)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65596, 65597)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65599, 65613)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65616, 65629)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65664, 65786)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65792, 65794)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65799, 65843)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65847, 65934)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65936, 65947)] = CodepointWidth::Narrow;
    _map[UnicodeRange(65952, 65952)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66000, 66045)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66176, 66204)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66208, 66256)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66272, 66299)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66304, 66339)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66349, 66378)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66384, 66426)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66432, 66461)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66463, 66499)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66504, 66517)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66560, 66717)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66720, 66729)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66736, 66771)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66776, 66811)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66816, 66855)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66864, 66915)] = CodepointWidth::Narrow;
    _map[UnicodeRange(66927, 66927)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67072, 67382)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67392, 67413)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67424, 67431)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67584, 67589)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67592, 67592)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67594, 67637)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67639, 67640)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67644, 67644)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67647, 67669)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67671, 67742)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67751, 67759)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67808, 67826)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67828, 67829)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67835, 67867)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67871, 67897)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67903, 67903)] = CodepointWidth::Narrow;
    _map[UnicodeRange(67968, 68023)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68028, 68047)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68050, 68099)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68101, 68102)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68108, 68115)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68117, 68119)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68121, 68147)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68152, 68154)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68159, 68167)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68176, 68184)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68192, 68255)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68288, 68326)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68331, 68342)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68352, 68405)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68409, 68437)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68440, 68466)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68472, 68497)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68505, 68508)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68521, 68527)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68608, 68680)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68736, 68786)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68800, 68850)] = CodepointWidth::Narrow;
    _map[UnicodeRange(68858, 68863)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69216, 69246)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69632, 69709)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69714, 69743)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69759, 69825)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69840, 69864)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69872, 69881)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69888, 69940)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69942, 69955)] = CodepointWidth::Narrow;
    _map[UnicodeRange(69968, 70006)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70016, 70093)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70096, 70111)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70113, 70132)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70144, 70161)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70163, 70206)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70272, 70278)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70280, 70280)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70282, 70285)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70287, 70301)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70303, 70313)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70320, 70378)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70384, 70393)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70400, 70403)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70405, 70412)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70415, 70416)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70419, 70440)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70442, 70448)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70450, 70451)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70453, 70457)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70460, 70468)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70471, 70472)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70475, 70477)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70480, 70480)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70487, 70487)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70493, 70499)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70502, 70508)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70512, 70516)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70656, 70745)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70747, 70747)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70749, 70749)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70784, 70855)] = CodepointWidth::Narrow;
    _map[UnicodeRange(70864, 70873)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71040, 71093)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71096, 71133)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71168, 71236)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71248, 71257)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71264, 71276)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71296, 71351)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71360, 71369)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71424, 71449)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71453, 71467)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71472, 71487)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71840, 71922)] = CodepointWidth::Narrow;
    _map[UnicodeRange(71935, 71935)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72192, 72263)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72272, 72323)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72326, 72348)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72350, 72354)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72384, 72440)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72704, 72712)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72714, 72758)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72760, 72773)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72784, 72812)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72816, 72847)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72850, 72871)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72873, 72886)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72960, 72966)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72968, 72969)] = CodepointWidth::Narrow;
    _map[UnicodeRange(72971, 73014)] = CodepointWidth::Narrow;
    _map[UnicodeRange(73018, 73018)] = CodepointWidth::Narrow;
    _map[UnicodeRange(73020, 73021)] = CodepointWidth::Narrow;
    _map[UnicodeRange(73023, 73031)] = CodepointWidth::Narrow;
    _map[UnicodeRange(73040, 73049)] = CodepointWidth::Narrow;
    _map[UnicodeRange(73728, 74649)] = CodepointWidth::Narrow;
    _map[UnicodeRange(74752, 74862)] = CodepointWidth::Narrow;
    _map[UnicodeRange(74864, 74868)] = CodepointWidth::Narrow;
    _map[UnicodeRange(74880, 75075)] = CodepointWidth::Narrow;
    _map[UnicodeRange(77824, 78894)] = CodepointWidth::Narrow;
    _map[UnicodeRange(82944, 83526)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92160, 92728)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92736, 92766)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92768, 92777)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92782, 92783)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92880, 92909)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92912, 92917)] = CodepointWidth::Narrow;
    _map[UnicodeRange(92928, 92997)] = CodepointWidth::Narrow;
    _map[UnicodeRange(93008, 93017)] = CodepointWidth::Narrow;
    _map[UnicodeRange(93019, 93025)] = CodepointWidth::Narrow;
    _map[UnicodeRange(93027, 93047)] = CodepointWidth::Narrow;
    _map[UnicodeRange(93053, 93071)] = CodepointWidth::Narrow;
    _map[UnicodeRange(93952, 94020)] = CodepointWidth::Narrow;
    _map[UnicodeRange(94032, 94078)] = CodepointWidth::Narrow;
    _map[UnicodeRange(94095, 94111)] = CodepointWidth::Narrow;
    _map[UnicodeRange(94176, 94177)] = CodepointWidth::Wide;
    _map[UnicodeRange(94208, 100332)] = CodepointWidth::Wide;
    _map[UnicodeRange(100352, 101106)] = CodepointWidth::Wide;
    _map[UnicodeRange(110592, 110878)] = CodepointWidth::Wide;
    _map[UnicodeRange(110960, 111355)] = CodepointWidth::Wide;
    _map[UnicodeRange(113664, 113770)] = CodepointWidth::Narrow;
    _map[UnicodeRange(113776, 113788)] = CodepointWidth::Narrow;
    _map[UnicodeRange(113792, 113800)] = CodepointWidth::Narrow;
    _map[UnicodeRange(113808, 113817)] = CodepointWidth::Narrow;
    _map[UnicodeRange(113820, 113827)] = CodepointWidth::Narrow;
    _map[UnicodeRange(118784, 119029)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119040, 119078)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119081, 119272)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119296, 119365)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119552, 119638)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119648, 119665)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119808, 119892)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119894, 119964)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119966, 119967)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119970, 119970)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119973, 119974)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119977, 119980)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119982, 119993)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119995, 119995)] = CodepointWidth::Narrow;
    _map[UnicodeRange(119997, 120003)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120005, 120069)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120071, 120074)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120077, 120084)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120086, 120092)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120094, 120121)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120123, 120126)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120128, 120132)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120134, 120134)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120138, 120144)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120146, 120485)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120488, 120779)] = CodepointWidth::Narrow;
    _map[UnicodeRange(120782, 121483)] = CodepointWidth::Narrow;
    _map[UnicodeRange(121499, 121503)] = CodepointWidth::Narrow;
    _map[UnicodeRange(121505, 121519)] = CodepointWidth::Narrow;
    _map[UnicodeRange(122880, 122886)] = CodepointWidth::Narrow;
    _map[UnicodeRange(122888, 122904)] = CodepointWidth::Narrow;
    _map[UnicodeRange(122907, 122913)] = CodepointWidth::Narrow;
    _map[UnicodeRange(122915, 122916)] = CodepointWidth::Narrow;
    _map[UnicodeRange(122918, 122922)] = CodepointWidth::Narrow;
    _map[UnicodeRange(124928, 125124)] = CodepointWidth::Narrow;
    _map[UnicodeRange(125127, 125142)] = CodepointWidth::Narrow;
    _map[UnicodeRange(125184, 125258)] = CodepointWidth::Narrow;
    _map[UnicodeRange(125264, 125273)] = CodepointWidth::Narrow;
    _map[UnicodeRange(125278, 125279)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126464, 126467)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126469, 126495)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126497, 126498)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126500, 126500)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126503, 126503)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126505, 126514)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126516, 126519)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126521, 126521)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126523, 126523)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126530, 126530)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126535, 126535)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126537, 126537)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126539, 126539)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126541, 126543)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126545, 126546)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126548, 126548)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126551, 126551)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126553, 126553)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126555, 126555)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126557, 126557)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126559, 126559)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126561, 126562)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126564, 126564)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126567, 126570)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126572, 126578)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126580, 126583)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126585, 126588)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126590, 126590)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126592, 126601)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126603, 126619)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126625, 126627)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126629, 126633)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126635, 126651)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126704, 126705)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126976, 126979)] = CodepointWidth::Narrow;
    _map[UnicodeRange(126980, 126980)] = CodepointWidth::Wide;
    _map[UnicodeRange(126981, 127019)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127024, 127123)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127136, 127150)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127153, 127167)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127169, 127182)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127183, 127183)] = CodepointWidth::Wide;
    _map[UnicodeRange(127185, 127221)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127232, 127242)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127243, 127244)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127248, 127277)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127278, 127278)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127280, 127337)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127338, 127339)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127344, 127373)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127374, 127374)] = CodepointWidth::Wide;
    _map[UnicodeRange(127375, 127376)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127377, 127386)] = CodepointWidth::Wide;
    _map[UnicodeRange(127387, 127404)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(127462, 127487)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127488, 127490)] = CodepointWidth::Wide;
    _map[UnicodeRange(127504, 127547)] = CodepointWidth::Wide;
    _map[UnicodeRange(127552, 127560)] = CodepointWidth::Wide;
    _map[UnicodeRange(127568, 127569)] = CodepointWidth::Wide;
    _map[UnicodeRange(127584, 127589)] = CodepointWidth::Wide;
    _map[UnicodeRange(127744, 127776)] = CodepointWidth::Wide;
    _map[UnicodeRange(127777, 127788)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127789, 127797)] = CodepointWidth::Wide;
    _map[UnicodeRange(127798, 127798)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127799, 127868)] = CodepointWidth::Wide;
    _map[UnicodeRange(127869, 127869)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127870, 127891)] = CodepointWidth::Wide;
    _map[UnicodeRange(127892, 127903)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127904, 127946)] = CodepointWidth::Wide;
    _map[UnicodeRange(127947, 127950)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127951, 127955)] = CodepointWidth::Wide;
    _map[UnicodeRange(127956, 127967)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127968, 127984)] = CodepointWidth::Wide;
    _map[UnicodeRange(127985, 127987)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127988, 127988)] = CodepointWidth::Wide;
    _map[UnicodeRange(127989, 127991)] = CodepointWidth::Narrow;
    _map[UnicodeRange(127992, 128062)] = CodepointWidth::Wide;
    _map[UnicodeRange(128063, 128063)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128064, 128064)] = CodepointWidth::Wide;
    _map[UnicodeRange(128065, 128065)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128066, 128252)] = CodepointWidth::Wide;
    _map[UnicodeRange(128253, 128254)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128255, 128317)] = CodepointWidth::Wide;
    _map[UnicodeRange(128318, 128330)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128331, 128334)] = CodepointWidth::Wide;
    _map[UnicodeRange(128335, 128335)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128336, 128359)] = CodepointWidth::Wide;
    _map[UnicodeRange(128360, 128377)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128378, 128378)] = CodepointWidth::Wide;
    _map[UnicodeRange(128379, 128404)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128405, 128406)] = CodepointWidth::Wide;
    _map[UnicodeRange(128407, 128419)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128420, 128420)] = CodepointWidth::Wide;
    _map[UnicodeRange(128421, 128506)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128507, 128591)] = CodepointWidth::Wide;
    _map[UnicodeRange(128592, 128639)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128640, 128709)] = CodepointWidth::Wide;
    _map[UnicodeRange(128710, 128715)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128716, 128716)] = CodepointWidth::Wide;
    _map[UnicodeRange(128717, 128719)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128720, 128722)] = CodepointWidth::Wide;
    _map[UnicodeRange(128723, 128724)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128736, 128746)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128747, 128748)] = CodepointWidth::Wide;
    _map[UnicodeRange(128752, 128755)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128756, 128760)] = CodepointWidth::Wide;
    _map[UnicodeRange(128768, 128883)] = CodepointWidth::Narrow;
    _map[UnicodeRange(128896, 128980)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129024, 129035)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129040, 129095)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129104, 129113)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129120, 129159)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129168, 129197)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129280, 129291)] = CodepointWidth::Narrow;
    _map[UnicodeRange(129296, 129342)] = CodepointWidth::Wide;
    _map[UnicodeRange(129344, 129356)] = CodepointWidth::Wide;
    _map[UnicodeRange(129360, 129387)] = CodepointWidth::Wide;
    _map[UnicodeRange(129408, 129431)] = CodepointWidth::Wide;
    _map[UnicodeRange(129472, 129472)] = CodepointWidth::Wide;
    _map[UnicodeRange(129488, 129510)] = CodepointWidth::Wide;
    _map[UnicodeRange(131072, 196605)] = CodepointWidth::Wide;
    _map[UnicodeRange(196608, 262141)] = CodepointWidth::Wide;
    _map[UnicodeRange(917505, 917505)] = CodepointWidth::Narrow;
    _map[UnicodeRange(917536, 917631)] = CodepointWidth::Narrow;
    _map[UnicodeRange(917760, 917999)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(983040, 1048573)] = CodepointWidth::Ambiguous;
    _map[UnicodeRange(1048576, 1114109)] = CodepointWidth::Ambiguous;
}
