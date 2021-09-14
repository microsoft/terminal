/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- BlinkingState.hpp

Abstract:
- This serves as a structure holding the state of the blinking rendition.

- It tracks the position in the blinking cycle, which determines whether any
  blinking cells should be rendered as on or off/faint. It also records whether
  blinking attributes are actually in use or not, so we can decide whether the
  screen needs to be refreshed when the blinking cycle changes.
--*/

#pragma once

#include "IRenderTarget.hpp"

namespace Microsoft::Console::Render
{
    class BlinkingState
    {
    public:
        void SetBlinkingAllowed(const bool blinkingAllowed) noexcept;
        void RecordBlinkingUsage(const TextAttribute& attr) noexcept;
        bool IsBlinkingFaint() const noexcept;
        void ToggleBlinkingRendition(IRenderTarget& renderTarget) noexcept;

    private:
        bool _blinkingAllowed = true;
        size_t _blinkingCycle = 0;
        bool _blinkingIsInUse = false;
        bool _blinkingShouldBeFaint = false;
    };
}
