// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/BlinkingState.hpp"

using namespace Microsoft::Console::Render;

// Method Description:
// - Updates the flag indicating whether cells with the blinking attribute
//   can animate on and off.
// Arguments:
// - blinkingAllowed: true if blinking is permitted.
// Return Value:
// - <none>
void BlinkingState::SetBlinkingAllowed(const bool blinkingAllowed) noexcept
{
    _blinkingAllowed = blinkingAllowed;
    if (!_blinkingAllowed)
    {
        _blinkingShouldBeFaint = false;
    }
}

// Method Description:
// - Makes a record of whether the given attribute has blinking enabled or
//   not, so we can determine whether the screen will need to be refreshed
//   when the blinking rendition state changes.
// Arguments:
// - attr: a text attribute that is expected to be rendered.
// Return Value:
// - <none>
void BlinkingState::RecordBlinkingUsage(const TextAttribute& attr) noexcept
{
    _blinkingIsInUse = _blinkingIsInUse || attr.IsBlinking();
}

// Method Description:
// - Determines whether cells with the blinking attribute should be rendered
//   as normal or faint, based on the current position in the blinking cycle.
// Arguments:
// - <none>
// Return Value:
// - True if blinking cells should be rendered as faint.
bool BlinkingState::IsBlinkingFaint() const noexcept
{
    return _blinkingShouldBeFaint;
}

// Method Description:
// - Increments the position in the blinking cycle, toggling the blinking
//   rendition state on every second call, potentially triggering a redraw of
//   the given render target if there are blinking cells currently in view.
// Arguments:
// - renderTarget: the render target that will be redrawn.
// Return Value:
// - <none>
void BlinkingState::ToggleBlinkingRendition(IRenderTarget& renderTarget) noexcept
try
{
    if (_blinkingAllowed)
    {
        // This method is called with the frequency of the cursor blink rate,
        // but we only want our cells to blink at half that frequency. We thus
        // have a blinking cycle that loops through four phases...
        _blinkingCycle = (_blinkingCycle + 1) % 4;
        // ... and two of those four render the blinking attributes as faint.
        _blinkingShouldBeFaint = _blinkingCycle >= 2;
        // Every two cycles (when the state changes), we need to trigger a
        // redraw, but only if there are actually blinking attributes in use.
        if (_blinkingIsInUse && _blinkingCycle % 2 == 0)
        {
            // We reset the _blinkingIsInUse flag before redrawing, so we can
            // get a fresh assessment of the current blinking attribute usage.
            _blinkingIsInUse = false;
            renderTarget.TriggerRedrawAll();
        }
    }
}
CATCH_LOG()
