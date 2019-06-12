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
    XtermEngine(std::move(hPipe), colorProvider, initialViewport, ColorTable, cColorTable, false)
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
                                                           const bool isBold,
                                                           const bool /*isSettingDefaultBrushes*/) noexcept
{
    //When we update the brushes, check the wAttrs to see if the LVB_UNDERSCORE
    //      flag is there. If the state of that flag is different then our
    //      current state, change the underlining state.
    // We have to do this here, instead of in PaintBufferGridLines, because
    //      we'll have already painted the text by the time PaintBufferGridLines
    //      is called.
    RETURN_IF_FAILED(_UpdateUnderline(legacyColorAttribute));

    return VtEngine::_RgbUpdateDrawingBrushes(colorForeground,
                                              colorBackground,
                                              isBold,
                                              _ColorTable,
                                              _cColorTable);
}
