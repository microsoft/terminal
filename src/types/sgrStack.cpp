// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/sgrStack.hpp"

using namespace Microsoft::Console::VirtualTerminal::DispatchTypes;

namespace Microsoft::Console::VirtualTerminal
{
    SgrStack::SgrStack() noexcept :
        _nextPushIndex{ 0 },
        _numSavedAttrs{ 0 }
    {
    }

    void SgrStack::Push(const TextAttribute& currentAttributes,
                        const VTParameters options) noexcept
    {
        AttrBitset validParts;

        if (options.empty())
        {
            // We save all current attributes.
            validParts.set(SgrSaveRestoreStackOptions::All);
        }
        else
        {
            // Each option is encoded as a bit in validParts. All options (that fit) are
            // encoded; options that aren't supported are ignored when read back (popped).
            // So if you try to save only unsupported aspects of the current text
            // attributes, you'll do what is effectively an "empty" push (the subsequent
            // pop will not change the current attributes), which is the correct behavior.

            for (size_t i = 0; i < options.size(); i++)
            {
                const auto option = static_cast<SgrSaveRestoreStackOptions>(options.at(i).value_or(0));

                // Options must be specified singly; not in combination. Values that are
                // out of range will be ignored.
                if (option > SgrSaveRestoreStackOptions::All && option <= SgrSaveRestoreStackOptions::Max)
                {
                    validParts.set(option);
                }
            }
        }

        if (_numSavedAttrs < gsl::narrow<int>(_storedSgrAttributes.size()))
        {
            _numSavedAttrs++;
        }

        _storedSgrAttributes.at(_nextPushIndex) = { currentAttributes, validParts };
        _nextPushIndex = (_nextPushIndex + 1) % gsl::narrow<int>(_storedSgrAttributes.size());
    }

    const TextAttribute SgrStack::Pop(const TextAttribute& currentAttributes) noexcept
    {
        if (_numSavedAttrs > 0)
        {
            _numSavedAttrs--;

            if (_nextPushIndex == 0)
            {
                _nextPushIndex = gsl::narrow<int>(_storedSgrAttributes.size() - 1);
            }
            else
            {
                _nextPushIndex--;
            }

            auto& restoreMe = _storedSgrAttributes.at(_nextPushIndex);

            if (restoreMe.ValidParts.test(SgrSaveRestoreStackOptions::All))
            {
                return restoreMe.TextAttributes;
            }
            else
            {
                return _CombineWithCurrentAttributes(currentAttributes,
                                                     restoreMe.TextAttributes,
                                                     restoreMe.ValidParts);
            }
        }

        return currentAttributes;
    }

    TextAttribute SgrStack::_CombineWithCurrentAttributes(const TextAttribute& currentAttributes,
                                                          const TextAttribute& savedAttribute,
                                                          const AttrBitset validParts) noexcept // of savedAttribute
    {
        // If we are restoring all attributes, we should have just taken savedAttribute
        // before we even got here.
        FAIL_FAST_IF(validParts.test(SgrSaveRestoreStackOptions::All));

        auto result = currentAttributes;

        // From xterm documentation:
        //
        //  CSI # {
        //  CSI Ps ; Ps # {
        //            Push video attributes onto stack (XTPUSHSGR), xterm.  The
        //            optional parameters correspond to the SGR encoding for video
        //            attributes, except for colors (which do not have a unique SGR
        //            code):
        //              Ps = 1  -> Intense.
        //              Ps = 2  -> Faint.
        //              Ps = 3  -> Italicized.
        //              Ps = 4  -> Underlined.
        //              Ps = 5  -> Blink.
        //              Ps = 7  -> Inverse.
        //              Ps = 8  -> Invisible.
        //              Ps = 9  -> Crossed-out characters.
        //              Ps = 2 1  -> Doubly-underlined.
        //              Ps = 3 0  -> Foreground color.
        //              Ps = 3 1  -> Background color.
        //
        //  (some closing braces for people with editors that get thrown off without them: }})
        //
        //  Additionally, we support extended underline styles to be pushed/popped
        //  using Parameter 4, except doubly underlined, which uses Parameter 21.

        // Intense = 1,
        if (validParts.test(SgrSaveRestoreStackOptions::Intense))
        {
            result.SetIntense(savedAttribute.IsIntense());
        }

        // Faintness = 2,
        if (validParts.test(SgrSaveRestoreStackOptions::Faintness))
        {
            result.SetFaint(savedAttribute.IsFaint());
        }

        // Italics = 3,
        if (validParts.test(SgrSaveRestoreStackOptions::Italics))
        {
            result.SetItalic(savedAttribute.IsItalic());
        }

        // Underline = 4, DoublyUnderlined = 21,
        const bool isUnderlinedValid = validParts.test(SgrSaveRestoreStackOptions::Underline);
        const bool isDoublyUnderlinedValid = validParts.test(SgrSaveRestoreStackOptions::DoublyUnderlined);
        if (isUnderlinedValid && isDoublyUnderlinedValid)
        {
            // all the styles are valid, we can simply apply the saved style.
            result.SetUnderlineStyle(savedAttribute.GetUnderlineStyle());
        }
        else if (isUnderlinedValid)
        {
            const auto savedUl = savedAttribute.GetUnderlineStyle();
            const auto isUnderlinedOn = savedUl != UnderlineStyle::NoUnderline &&
                                        savedUl != UnderlineStyle::DoublyUnderlined;
            if (isUnderlinedOn)
            {
                result.SetUnderlineStyle(savedUl);
            }
            else
            {
                // Turn off singly and extended styles, but if the current style is
                // doubly underlined, no need to overwrite it. This mimics having
                // two flags each for singly and doubly underlined, where flag for
                // doubly underlined would be left 'on' even if we had turned off
                // the singly underlined.
                if (result.GetUnderlineStyle() != UnderlineStyle::DoublyUnderlined)
                {
                    result.SetUnderlineStyle(UnderlineStyle::NoUnderline);
                }
            }
        }
        else if (isDoublyUnderlinedValid)
        {
            const auto isDoublyUnderlinedOn = savedAttribute.GetUnderlineStyle() == UnderlineStyle::DoublyUnderlined;
            if (isDoublyUnderlinedOn)
            {
                result.SetUnderlineStyle(UnderlineStyle::DoublyUnderlined);
            }
            else
            {
                // Turn off doubly underlined, but if the current style is
                // singly underlined (or extended style), no need to overwrite it.
                // This mimics having two flags each for singly and doubly
                // underlined, where flag for singly underlined would be left 'on'
                // even if we had turned off the doubly underlined.
                if (result.GetUnderlineStyle() == UnderlineStyle::DoublyUnderlined)
                {
                    result.SetUnderlineStyle(UnderlineStyle::NoUnderline);
                }
            }
        }

        // Blink = 5,
        if (validParts.test(SgrSaveRestoreStackOptions::Blink))
        {
            result.SetBlinking(savedAttribute.IsBlinking());
        }

        // Negative = 7,
        if (validParts.test(SgrSaveRestoreStackOptions::Negative))
        {
            result.SetReverseVideo(savedAttribute.IsReverseVideo());
        }

        // Invisible = 8,
        if (validParts.test(SgrSaveRestoreStackOptions::Invisible))
        {
            result.SetInvisible(savedAttribute.IsInvisible());
        }

        // CrossedOut = 9,
        if (validParts.test(SgrSaveRestoreStackOptions::CrossedOut))
        {
            result.SetCrossedOut(savedAttribute.IsCrossedOut());
        }

        // SaveForegroundColor = 30,
        if (validParts.test(SgrSaveRestoreStackOptions::SaveForegroundColor))
        {
            result.SetForeground(savedAttribute.GetForeground());
        }

        // SaveBackgroundColor = 31,
        if (validParts.test(SgrSaveRestoreStackOptions::SaveBackgroundColor))
        {
            result.SetBackground(savedAttribute.GetBackground());
        }

        return result;
    }

}
