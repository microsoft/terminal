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
                               const std::basic_string_view<COLORREF> colorTable) :
    XtermEngine(std::move(hPipe), colorProvider, initialViewport, colorTable, false)
{
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Writes true RGB
//      color sequences.
// Arguments:
// - colorForeground: The RGB Color to use to paint the foreground text.
// - colorBackground: The RGB Color to use to paint the background of the text.
// - textAttributes - text attributes (bold, italic, underline, etc.) to use.
// - isSettingDefaultBrushes: indicates if we should change the background color of
//      the window. Unused for VT
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::UpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const TextAttribute& textAttributes,
                                                           const bool /*isSettingDefaultBrushes*/) noexcept
{
    //When we update the brushes, check the wAttrs to see if the LVB_UNDERSCORE
    //      flag is there. If the state of that flag is different then our
    //      current state, change the underlining state.
    // We have to do this here, instead of in PaintBufferGridLines, because
    //      we'll have already painted the text by the time PaintBufferGridLines
    //      is called.
    // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
    RETURN_IF_FAILED(_UpdateUnderline(textAttributes));

    // Only do extended attributes in xterm-256color, as to not break telnet.exe.
    RETURN_IF_FAILED(_UpdateExtendedAttrs(textAttributes));

    return VtEngine::_RgbUpdateDrawingBrushes(colorForeground,
                                              colorBackground,
                                              textAttributes.IsBold(),
                                              _colorTable);
}

// Routine Description:
// - Write a VT sequence to either start or stop underlining text.
// Arguments:
// - textAttributes - text attributes (bold, italic, underline, etc.) to use.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::_UpdateExtendedAttrs(const TextAttribute& textAttributes) noexcept
{
    // Helper lambda to check if a state (attr) has changed since it's last
    // value (lastState), and appropriately start/end that state with the given
    // begin/end functions.
    auto updateFlagAndState = [this](const bool flagSet,
                                     const bool lastState,
                                     std::function<HRESULT(Xterm256Engine*)> beginFn,
                                     std::function<HRESULT(Xterm256Engine*)> endFn) -> HRESULT {
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
        }
        return S_OK;
    };

    auto hr = updateFlagAndState(textAttributes.IsItalic(),
                                 _lastTextAttributes.IsItalic(),
                                 &Xterm256Engine::_BeginItalics,
                                 &Xterm256Engine::_EndItalics);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(textAttributes.IsBlinking(),
                            _lastTextAttributes.IsBlinking(),
                            &Xterm256Engine::_BeginBlink,
                            &Xterm256Engine::_EndBlink);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(textAttributes.IsInvisible(),
                            _lastTextAttributes.IsInvisible(),
                            &Xterm256Engine::_BeginInvisible,
                            &Xterm256Engine::_EndInvisible);
    RETURN_IF_FAILED(hr);

    hr = updateFlagAndState(textAttributes.IsCrossedOut(),
                            _lastTextAttributes.IsCrossedOut(),
                            &Xterm256Engine::_BeginCrossedOut,
                            &Xterm256Engine::_EndCrossedOut);
    RETURN_IF_FAILED(hr);

    _lastTextAttributes = textAttributes;
    return S_OK;
}

// Method Description:
// - Manually emit a "Erase Scrollback" sequence to the connected terminal. We
//   need to do this in certain cases that we've identified where we believe the
//   client wanted the entire terminal buffer cleared, not just the viewport.
//   For more information, see GH#3126.
// Arguments:
// - <none>
// Return Value:
// - S_OK if we wrote the sequences successfully, otherwise an appropriate HRESULT
[[nodiscard]] HRESULT Xterm256Engine::ManuallyClearScrollback() noexcept
{
    return _ClearScrollback();
}
