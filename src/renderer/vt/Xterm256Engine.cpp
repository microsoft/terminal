// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Xterm256Engine.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

Xterm256Engine::Xterm256Engine(_In_ wil::unique_hfile hPipe,
                               const IDefaultColorProvider& colorProvider,
                               const Viewport initialViewport,
                               _In_reads_(cColorTable) const COLORREF* const ColorTable,
                               const WORD cColorTable) :
    XtermEngine(std::move(hPipe), colorProvider, initialViewport, ColorTable, cColorTable, false),
    _lastExtendedAttrsState{ ExtendedAttributes::Normal }
{
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Writes true RGB
//      color sequences.
// Arguments:
// - colorForeground: The RGB Color to use to paint the foreground text.
// - colorBackground: The RGB Color to use to paint the background of the text.
// - legacyColorAttribute: A console attributes bit field specifying the brush
//      colors we should use.
// - extendedAttrs - extended text attributes (italic, underline, etc.) to use.
// - isSettingDefaultBrushes: indicates if we should change the background color of
//      the window. Unused for VT
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::UpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const WORD legacyColorAttribute,
                                                           const ExtendedAttributes extendedAttrs,
                                                           const bool /*isSettingDefaultBrushes*/) noexcept
{
    //When we update the brushes, check the wAttrs to see if the LVB_UNDERSCORE
    //      flag is there. If the state of that flag is different then our
    //      current state, change the underlining state.
    // We have to do this here, instead of in PaintBufferGridLines, because
    //      we'll have already painted the text by the time PaintBufferGridLines
    //      is called.
    // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
    RETURN_IF_FAILED(_UpdateUnderline(legacyColorAttribute));

    // Only do extended attributes in xterm-256color, as to not break telnet.exe.
    RETURN_IF_FAILED(_UpdateExtendedAttrs(extendedAttrs));

    return VtEngine::_RgbUpdateDrawingBrushes(colorForeground,
                                              colorBackground,
                                              WI_IsFlagSet(extendedAttrs, ExtendedAttributes::Bold),
                                              _ColorTable,
                                              _cColorTable);
}

// Routine Description:
// - Write a VT sequence to either start or stop underlining text.
// Arguments:
// - legacyColorAttribute: A console attributes bit field containing information
//      about the underlining state of the text.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::_UpdateExtendedAttrs(const ExtendedAttributes extendedAttrs) noexcept
{
    // Helper lambda to check if a state (attr) has changed since it's last
    // value (lastState), and appropriately start/end that state with the given
    // begin/end functions.
    auto updateFlagAndState = [extendedAttrs, this](const ExtendedAttributes attr,
                                                    std::function<HRESULT(Xterm256Engine*)> beginFn,
                                                    std::function<HRESULT(Xterm256Engine*)> endFn) -> HRESULT {
        const bool flagSet = WI_AreAllFlagsSet(extendedAttrs, attr);
        const bool lastState = WI_AreAllFlagsSet(_lastExtendedAttrsState, attr);
        if (flagSet != lastState)
        {
            if (flagSet)
            {
                RETURN_IF_FAILED(beginFn(this));
            }
            else
            {
                RETURN_IF_FAILED(endFn(this));
            }
            WI_ToggleAllFlags(_lastExtendedAttrsState, attr);
        }
        return S_OK;
    };

    auto hr = updateFlagAndState(ExtendedAttributes::Italics,
                                 &Xterm256Engine::_BeginItalics,
                                 &Xterm256Engine::_EndItalics);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(ExtendedAttributes::Blinking,
                            &Xterm256Engine::_BeginBlink,
                            &Xterm256Engine::_EndBlink);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(ExtendedAttributes::Invisible,
                            &Xterm256Engine::_BeginInvisible,
                            &Xterm256Engine::_EndInvisible);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(ExtendedAttributes::CrossedOut,
                            &Xterm256Engine::_BeginCrossedOut,
                            &Xterm256Engine::_EndCrossedOut);
    RETURN_IF_FAILED(hr);

    return S_OK;
}
