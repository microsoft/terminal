// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Xterm256Engine.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

Xterm256Engine::Xterm256Engine(_In_ wil::unique_hfile hPipe,
                               wil::shared_event shutdownEvent,
                               const IDefaultColorProvider& colorProvider,
                               const Viewport initialViewport,
                               _In_reads_(cColorTable) const COLORREF* const ColorTable,
                               const WORD cColorTable) :
    XtermEngine(std::move(hPipe), shutdownEvent, colorProvider, initialViewport, ColorTable, cColorTable, false)
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
    RETURN_IF_FAILED(_UpdateUnderline(legacyColorAttribute));

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
    auto updateFlagAndState = [extendedAttrs](const ExtendedAttributes attr,
                                              bool& lastState,
                                              std::function<HRESULT(void)> beginFn,
                                              std::function<HRESULT(void)> endFn) -> HRESULT {
        // Unfortunately, we can't use WI_IsFlagSet here, since that requires a
        // compile-time check that the flag (attr) is a _single_ flag, and that
        // won't work in this case.
        const bool flagSet = (extendedAttrs & attr) == attr;
        if (flagSet != lastState)
        {
            if (flagSet)
            {
                RETURN_IF_FAILED(beginFn());
            }
            else
            {
                RETURN_IF_FAILED(endFn());
            }
            lastState = flagSet;
        }
        return S_OK;
    };

    auto hr = updateFlagAndState(ExtendedAttributes::Italics,
                                 _usingItalics,
                                 std::bind(&Xterm256Engine::_BeginUnderline, this),
                                 std::bind(&Xterm256Engine::_EndUnderline, this));
    RETURN_IF_FAILED(hr);

    // const bool textItalics = WI_IsFlagSet(extendedAttrs, ExtendedAttributes::Italics);
    // if (textItalics != _usingItalics)
    // {
    //     if (textUnderlined)
    //     {
    //         RETURN_IF_FAILED(_BeginUnderline());
    //     }
    //     else
    //     {
    //         RETURN_IF_FAILED(_EndUnderline());
    //     }
    //     _usingUnderLine = textUnderlined;
    // }
    return S_OK;
}
