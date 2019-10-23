// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/convert.hpp"

#include "../inc/unicode.hpp"

#ifdef BUILD_ONECORE_INTERACTIVITY
#include "../../interactivity/inc/VtApiRedirection.hpp"
#endif

#pragma hdrstop

// TODO: MSFT 14150722 - can these const values be generated at
// runtime without breaking compatibility?
static const WORD altScanCode = 0x38;
static const WORD leftShiftScanCode = 0x2A;

// Routine Description:
// - Takes a multibyte string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Unicode UTF-16 result in the smart pointer (and the length).
// Arguments:
// - codepage - Windows Code Page representing the multibyte source text
// - source - View of multibyte characters of source text
// Return Value:
// - The UTF-16 wide string.
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::wstring ConvertToW(const UINT codePage, const std::string_view source)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        return {};
    }

    int iSource; // convert to int because Mb2Wc requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    int const iTarget = MultiByteToWideChar(codePage, 0, source.data(), iSource, nullptr, 0);
    THROW_LAST_ERROR_IF(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    std::wstring out;
    out.resize(cchNeeded);

    // Attempt conversion for real.
    THROW_LAST_ERROR_IF(0 == MultiByteToWideChar(codePage, 0, source.data(), iSource, out.data(), iTarget));

    // Return as a string
    return out;
}

// Routine Description:
// - Takes a wide string, allocates the appropriate amount of memory for the conversion, performs the conversion,
//   and returns the Multibyte result
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Unicode (UTF-16) characters of source text
// Return Value:
// - The multibyte string encoded in the given codepage
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or MultiByteToWideChar failures.
[[nodiscard]] std::string ConvertToA(const UINT codepage, const std::wstring_view source)
{
    // If there's nothing to convert, bail early.
    if (source.empty())
    {
        return {};
    }

    int iSource; // convert to int because Wc2Mb requires it.
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how much space we will need.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    int const iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    size_t cchNeeded;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchNeeded));

    // Allocate ourselves some space
    std::string out;
    out.resize(cchNeeded);

    // Attempt conversion for real.
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    THROW_LAST_ERROR_IF(0 == WideCharToMultiByte(codepage, 0, source.data(), iSource, out.data(), iTarget, nullptr, nullptr));

    // Return as a string
    return out;
}

// Routine Description:
// - Takes a wide string, and determines how many bytes it would take to store it with the given Multibyte codepage.
// Arguments:
// - codepage - Windows Code Page representing the multibyte destination text
// - source - Array of Unicode characters of source text
// Return Value:
// - Length in characters of multibyte buffer that would be required to hold this text after conversion
// - NOTE: Throws suitable HRESULT errors from memory allocation, safe math, or WideCharToMultiByte failures.
[[nodiscard]] size_t GetALengthFromW(const UINT codepage, const std::wstring_view source)
{
    // If there's no bytes, bail early.
    if (source.empty())
    {
        return 0;
    }

    int iSource; // convert to int because Wc2Mb requires it
    THROW_IF_FAILED(SizeTToInt(source.size(), &iSource));

    // Ask how many bytes this string consumes in the other codepage
    // clang-format off
#pragma prefast(suppress: __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
    // clang-format on
    int const iTarget = WideCharToMultiByte(codepage, 0, source.data(), iSource, nullptr, 0, nullptr, nullptr);
    THROW_LAST_ERROR_IF(0 == iTarget);

    // Convert types safely.
    size_t cchTarget;
    THROW_IF_FAILED(IntToSizeT(iTarget, &cchTarget));

    return cchTarget;
}

std::deque<std::unique_ptr<KeyEvent>> CharToKeyEvents(const wchar_t wch,
                                                      const unsigned int codepage)
{
    const short invalidKey = -1;
    short keyState = VkKeyScanW(wch);

    if (keyState == invalidKey)
    {
        // Determine DBCS character because these character does not know by VkKeyScan.
        // GetStringTypeW(CT_CTYPE3) & C3_ALPHA can determine all linguistic characters. However, this is
        // not include symbolic character for DBCS.
        WORD CharType = 0;
        GetStringTypeW(CT_CTYPE3, &wch, 1, &CharType);

        if (WI_IsFlagSet(CharType, C3_ALPHA) || GetQuickCharWidth(wch) == CodepointWidth::Wide)
        {
            keyState = 0;
        }
    }

    std::deque<std::unique_ptr<KeyEvent>> convertedEvents;
    if (keyState == invalidKey)
    {
        // if VkKeyScanW fails (char is not in kbd layout), we must
        // emulate the key being input through the numpad
        convertedEvents = SynthesizeNumpadEvents(wch, codepage);
    }
    else
    {
        convertedEvents = SynthesizeKeyboardEvents(wch, keyState);
    }

    return convertedEvents;
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using the keyboard
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed
// Note:
// - will throw exception on error
std::deque<std::unique_ptr<KeyEvent>> SynthesizeKeyboardEvents(const wchar_t wch, const short keyState)
{
    const byte modifierState = HIBYTE(keyState);

    bool altGrSet = false;
    bool shiftSet = false;
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    // add modifier key event if necessary
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        altGrSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       (ENHANCED_KEY | LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED)));
    }
    else if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        shiftSet = true;
        keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       SHIFT_PRESSED));
    }

    const auto vk = LOBYTE(keyState);
    const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    KeyEvent keyEvent{ true, 1, LOBYTE(keyState), virtualScanCode, wch, 0 };

    // add modifier flags if necessary
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::ShiftPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::Shift);
    }
    if (WI_IsFlagSet(modifierState, VkKeyScanModState::CtrlPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::LeftCtrl);
    }
    if (WI_AreAllFlagsSet(modifierState, VkKeyScanModState::CtrlAndAltPressed))
    {
        keyEvent.ActivateModifierKey(ModifierKeyState::RightAlt);
    }

    // add key event down and up
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));
    keyEvent.SetKeyDown(false);
    keyEvents.push_back(std::make_unique<KeyEvent>(keyEvent));

    // add modifier key up event
    if (altGrSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_MENU),
                                                       altScanCode,
                                                       UNICODE_NULL,
                                                       ENHANCED_KEY));
    }
    else if (shiftSet)
    {
        keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                       1ui16,
                                                       static_cast<WORD>(VK_SHIFT),
                                                       leftShiftScanCode,
                                                       UNICODE_NULL,
                                                       0));
    }

    return keyEvents;
}

// Routine Description:
// - converts a wchar_t into a series of KeyEvents as if it was typed
// using Alt + numpad
// Arguments:
// - wch - the wchar_t to convert
// Return Value:
// - deque of KeyEvents that represent the wchar_t being typed using
// alt + numpad
// Note:
// - will throw exception on error
std::deque<std::unique_ptr<KeyEvent>> SynthesizeNumpadEvents(const wchar_t wch, const unsigned int codepage)
{
    std::deque<std::unique_ptr<KeyEvent>> keyEvents;

    //alt keydown
    keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   UNICODE_NULL,
                                                   LEFT_ALT_PRESSED));

    const int radix = 10;
    std::wstring wstr{ wch };
    const auto convertedChars = ConvertToA(codepage, wstr);
    if (convertedChars.size() == 1)
    {
        // It is OK if the char is "signed -1", we want to interpret that as "unsigned 255" for the
        // "integer to character" conversion below with ::to_string, thus the static_cast.
        // Prime example is nonbreaking space U+00A0 will convert to OEM by codepage 437 to 0xFF which is -1 signed.
        // But it is absolutely valid as 0xFF or 255 unsigned as the correct CP437 character.
        // We need to treat it as unsigned because we're going to pretend it was a keypad entry
        // and you don't enter negative numbers on the keypad.
        unsigned char const uch = static_cast<unsigned char>(convertedChars.at(0));

        // unsigned char values are in the range [0, 255] so we need to be
        // able to store up to 4 chars from the conversion (including the end of string char)
        auto charString = std::to_string(uch);

        for (auto& ch : std::string_view(charString))
        {
            if (ch == 0)
            {
                break;
            }
            const WORD virtualKey = ch - '0' + VK_NUMPAD0;
            const WORD virtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC));

            keyEvents.push_back(std::make_unique<KeyEvent>(true,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
            keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                           1ui16,
                                                           virtualKey,
                                                           virtualScanCode,
                                                           UNICODE_NULL,
                                                           LEFT_ALT_PRESSED));
        }
    }

    // alt keyup
    keyEvents.push_back(std::make_unique<KeyEvent>(false,
                                                   1ui16,
                                                   static_cast<WORD>(VK_MENU),
                                                   altScanCode,
                                                   wch,
                                                   0));
    return keyEvents;
}

// Routine Description:
// - naively determines the width of a UCS2 encoded wchar
// Arguments:
// - wch - the wchar_t to measure
// Return Value:
// - CodepointWidth indicating width of wch
// Notes:
// 04-08-92 ShunK       Created.
// Jul-27-1992 KazuM    Added Screen Information and Code Page Information.
// Jan-29-1992 V-Hirots Substruct Screen Information.
// Oct-06-1996 KazuM    Not use RtlUnicodeToMultiByteSize and WideCharToMultiByte
//                      Because 950 (Chinese Traditional) only defined 13500 chars,
//                     and unicode defined almost 18000 chars.
//                      So there are almost 4000 chars can not be mapped to big5 code.
// Apr-30-2015 MiNiksa  Corrected unknown character code assumption. Max Width in Text Metric
//                      is not reliable for calculating half/full width. Must use current
//                      display font data (cached) instead.
// May-23-2017 migrie   Forced Box-Drawing Characters (x2500-x257F) to narrow.
// Jan-16-2018 migrie   Seperated core lookup from asking the renderer the width
// May-01-2019 MiNiksa  Forced lookup-via-renderer for retroactively recategorized emoji
//                      that used to be narrow but now might be wide. (approx x2194-x2b55, not inclusive)
//                      Also forced block characters segment (x2580-x259F) to narrow
CodepointWidth GetQuickCharWidth(const wchar_t wch) noexcept
{
    // 0x00-0x1F is ambiguous by font
    if (0x20 <= wch && wch <= 0x7e)
    {
        /* ASCII */
        return CodepointWidth::Narrow;
    }
    // 0x80 - 0x0451 varies from narrow to ambiguous by character and font (Unicode 9.0)
    else if (0x0452 <= wch && wch <= 0x10FF)
    {
        // From Unicode 9.0, this range is narrow (assorted languages)
        return CodepointWidth::Narrow;
    }
    else if (0x1100 <= wch && wch <= 0x115F)
    {
        // From Unicode 9.0, Hangul Choseong is wide
        return CodepointWidth::Wide;
    }
    else if (0x1160 <= wch && wch <= 0x200F)
    {
        // From Unicode 9.0, this range is narrow (assorted languages)
        return CodepointWidth::Narrow;
    }
    // 0x2500 - 0x257F is the box drawing character range -
    // Technically, these are ambiguous width characters, but applications that
    // use them generally assume that they're narrow to ensure proper alignment.
    else if (0x2500 <= wch && wch <= 0x257F)
    {
        return CodepointWidth::Narrow;
    }
    // 0x2580 - 0x259F is the block element characters.
    // Technically these are ambiguous width, but many many things assume they're narrow.
    else if (0x2580 <= wch && wch <= 0x259F)
    {
        return CodepointWidth::Narrow;
    }
    // 0x2010 - 0x2B59 varies between narrow, ambiguous, and wide by character and font (Unicode 9.0)
    // However, there are a bunch of retroactive-emoji in this range. Things that weren't emoji and then they became
    // "emoji" later. As a result, they jumped from a fixed narrow definition to a now ambiguous definition.
    // There are others in this range already defined as wide or ambiguous, but we're just going to
    // implicitly say they're all ambiguous here to force a font lookup.
    // I picked the ones that looked like color double-wide emoji in my browser that weren't already
    // covered easily by the half-width/full-width table (see CodepointWidthDetector.cpp)
    // See https://unicode.org/Public/emoji/12.0/emoji-data.txt
    else if ((0x2194 <= wch && wch <= 0x2199) ||
             (0x21A9 <= wch && wch <= 0x21AA) ||
             (0x231A <= wch && wch <= 0x231B) ||
             0x2328 == wch ||
             0x23CF == wch ||
             (0x23E9 <= wch && wch <= 0x23F3) ||
             (0x23F8 <= wch && wch <= 0x23FA) ||
             0x24C2 == wch ||
             (0x25AA <= wch && wch <= 0x25AB) ||
             0x25B6 == wch ||
             0x25C0 == wch ||
             (0x25FB <= wch && wch <= 0x25FE) ||
             (0x2600 <= wch && wch <= 0x2604) ||
             0x260E == wch ||
             0x2611 == wch ||
             (0x2614 <= wch && wch <= 0x2615) ||
             0x2618 == wch ||
             0x261D == wch ||
             0x2620 == wch ||
             (0x2622 <= wch && wch <= 0x2623) ||
             0x2626 == wch ||
             0x262A == wch ||
             (0x262E <= wch && wch <= 0x262F) ||
             (0x2638 <= wch && wch <= 0x263A) ||
             0x2640 == wch ||
             0x2642 == wch ||
             (0x2648 <= wch && wch <= 0x2653) ||
             (0x265F <= wch && wch <= 0x2660) ||
             0x2663 == wch ||
             (0x2665 <= wch && wch <= 0x2666) ||
             0x2668 == wch ||
             0x267B == wch ||
             (0x267E <= wch && wch <= 0x267F) ||
             (0x2692 <= wch && wch <= 0x2697) ||
             0x2699 == wch ||
             (0x269B <= wch && wch <= 0x269C) ||
             (0x26A0 <= wch && wch <= 0x26A1) ||
             (0x26AA <= wch && wch <= 0x26AB) ||
             (0x26B0 <= wch && wch <= 0x26B1) ||
             (0x26BD <= wch && wch <= 0x26BE) ||
             (0x26C4 <= wch && wch <= 0x26C5) ||
             0x26C8 == wch ||
             0x26CE == wch ||
             0x26CF == wch ||
             0x26D1 == wch ||
             (0x26D3 <= wch && wch <= 0x26D4) ||
             (0x26E9 <= wch && wch <= 0x26EA) ||
             (0x26F0 <= wch && wch <= 0x26F5) ||
             (0x26F7 <= wch && wch <= 0x26FA) ||
             0x26FD == wch ||
             0x2702 == wch ||
             0x2705 == wch ||
             (0x2708 <= wch && wch <= 0x2709) ||
             (0x270A <= wch && wch <= 0x270B) ||
             (0x270C <= wch && wch <= 0x270D) ||
             0x270F == wch ||
             0x2712 == wch ||
             0x2714 == wch ||
             0x2716 == wch ||
             0x271D == wch ||
             0x2721 == wch ||
             0x2728 == wch ||
             (0x2733 <= wch && wch <= 0x2734) ||
             0x2744 == wch ||
             0x2747 == wch ||
             0x274C == wch ||
             0x274E == wch ||
             (0x2753 <= wch && wch <= 0x2755) ||
             0x2757 == wch ||
             (0x2763 <= wch && wch <= 0x2764) ||
             (0x2795 <= wch && wch <= 0x2797) ||
             0x27A1 == wch ||
             0x27B0 == wch ||
             0x27BF == wch ||
             (0x2934 <= wch && wch <= 0x2935) ||
             (0x2B05 <= wch && wch <= 0x2B07) ||
             (0x2B1B <= wch && wch <= 0x2B1C) ||
             0x2B50 == wch ||
             0x2B55 == wch)
    {
        return CodepointWidth::Ambiguous;
    }
    else if (0x2B5A <= wch && wch <= 0x2E44)
    {
        // From Unicode 9.0, this range is narrow (assorted languages)
        return CodepointWidth::Narrow;
    }
    else if (0x2E80 <= wch && wch <= 0x303e)
    {
        // From Unicode 9.0, this range is wide (assorted languages)
        return CodepointWidth::Wide;
    }
    else if (0x3041 <= wch && wch <= 0x3094)
    {
        /* Hiragana */
        return CodepointWidth::Wide;
    }
    else if (0x30a1 <= wch && wch <= 0x30f6)
    {
        /* Katakana */
        return CodepointWidth::Wide;
    }
    else if (0x3105 <= wch && wch <= 0x312c)
    {
        /* Bopomofo */
        return CodepointWidth::Wide;
    }
    else if (0x3131 <= wch && wch <= 0x318e)
    {
        /* Hangul Elements */
        return CodepointWidth::Wide;
    }
    else if (0x3190 <= wch && wch <= 0x3247)
    {
        // From Unicode 9.0, this range is wide
        return CodepointWidth::Wide;
    }
    else if (0x3251 <= wch && wch <= 0xA4C6)
    {
        // This exception range is narrow width hexagrams.
        if (0x4DC0 <= wch && wch <= 0x4DFF)
        {
            return CodepointWidth::Narrow;
        }
        else
        {
            // From Unicode 9.0, this range is wide
            // CJK Unified Ideograph and Yi and Reserved.
            // Includes Han Ideographic range.
            return CodepointWidth::Wide;
        }
    }
    else if (0xA4D0 <= wch && wch <= 0xABF9)
    {
        // This exception range is wide Hangul Choseong
        if (0xA960 <= wch && wch <= 0xA97C)
        {
            return CodepointWidth::Wide;
        }
        else
        {
            // From Unicode 9.0, this range is narrow (assorted languages)
            return CodepointWidth::Narrow;
        }
    }
    else if (0xac00 <= wch && wch <= 0xd7a3)
    {
        /* Korean Hangul Syllables */
        return CodepointWidth::Wide;
    }
    else if (0xD7B0 <= wch && wch <= 0xD7FB)
    {
        // From Unicode 9.0, this range is narrow
        // Hangul Jungseong and Hangul Jongseong
        return CodepointWidth::Narrow;
    }
    // 0xD800-0xDFFF is reserved for UTF-16 surrogate pairs.
    // 0xE000-0xF8FF is reserved for private use characters and is therefore always ambiguous.
    else if (0xF900 <= wch && wch <= 0xFAFF)
    {
        // From Unicode 9.0, this range is wide
        // CJK Compatibility Ideographs
        // Includes Han Compatibility Ideographs
        return CodepointWidth::Wide;
    }
    else if (0xFB00 <= wch && wch <= 0xFDFD)
    {
        // From Unicode 9.0, this range is narrow (assorted languages)
        return CodepointWidth::Narrow;
    }
    else if (0xFE10 <= wch && wch <= 0xFE6B)
    {
        // This exception range has narrow combining ligatures
        if (0xFE20 <= wch && wch <= 0xFE2F)
        {
            return CodepointWidth::Narrow;
        }
        else
        {
            // From Unicode 9.0, this range is wide
            // Presentation forms
            return CodepointWidth::Wide;
        }
    }
    else if (0xFE70 <= wch && wch <= 0xFEFF)
    {
        // From Unicode 9.0, this range is narrow
        return CodepointWidth::Narrow;
    }
    else if (0xff01 <= wch && wch <= 0xff5e)
    {
        /* Fullwidth ASCII variants */
        return CodepointWidth::Wide;
    }
    else if (0xff61 <= wch && wch <= 0xff9f)
    {
        /* Halfwidth Katakana variants */
        return CodepointWidth::Narrow;
    }
    else if ((0xffa0 <= wch && wch <= 0xffbe) ||
             (0xffc2 <= wch && wch <= 0xffc7) ||
             (0xffca <= wch && wch <= 0xffcf) ||
             (0xffd2 <= wch && wch <= 0xffd7) ||
             (0xffda <= wch && wch <= 0xffdc))
    {
        /* Halfwidth Hangule variants */
        return CodepointWidth::Narrow;
    }
    else if (0xffe0 <= wch && wch <= 0xffe6)
    {
        /* Fullwidth symbol variants */
        return CodepointWidth::Wide;
    }
    // Currently we do not support codepoints above 0xffff
    else
    {
        return CodepointWidth::Invalid;
    }
}

wchar_t Utf16ToUcs2(const std::wstring_view charData)
{
    THROW_HR_IF(E_INVALIDARG, charData.empty());
    if (charData.size() > 1)
    {
        return UNICODE_REPLACEMENT;
    }
    else
    {
        return charData.front();
    }
}
