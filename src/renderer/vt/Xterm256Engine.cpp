// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "Xterm256Engine.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

Xterm256Engine::Xterm256Engine(_In_ wil::unique_hfile hPipe,
                               const Viewport initialViewport) :
    XtermEngine(std::move(hPipe), initialViewport, false)
{
}

// Routine Description:
// - Write a VT sequence to change the current colors of text. Writes true RGB
//      color sequences.
// Arguments:
// - textAttributes - Text attributes to use for the colors and character rendition
// - pData - The interface to console data structures required for rendering
// - isSettingDefaultBrushes: indicates if we should change the background color of
//      the window. Unused for VT
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                           const gsl::not_null<IRenderData*> /*pData*/,
                                                           const bool /*isSettingDefaultBrushes*/) noexcept
{
    RETURN_IF_FAILED(VtEngine::_RgbUpdateDrawingBrushes(textAttributes));
    // Only do extended attributes in xterm-256color, as to not break telnet.exe.
    return _UpdateExtendedAttrs(textAttributes);
}

// Routine Description:
// - Write a VT sequence to update the character rendition attributes.
// Arguments:
// - textAttributes - text attributes (bold, italic, underline, etc.) to use.
// Return Value:
// - S_OK if we succeeded, else an appropriate HRESULT for failing to allocate or write.
[[nodiscard]] HRESULT Xterm256Engine::_UpdateExtendedAttrs(const TextAttribute& textAttributes) noexcept
{
    if (textAttributes.IsBold() != _lastTextAttributes.IsBold())
    {
        RETURN_IF_FAILED(_SetBold(textAttributes.IsBold()));
        _lastTextAttributes.SetBold(textAttributes.IsBold());
    }

    if (textAttributes.IsUnderlined() != _lastTextAttributes.IsUnderlined())
    {
        RETURN_IF_FAILED(_SetUnderline(textAttributes.IsUnderlined()));
        _lastTextAttributes.SetUnderline(textAttributes.IsUnderlined());
    }

    if (textAttributes.IsItalic() != _lastTextAttributes.IsItalic())
    {
        RETURN_IF_FAILED(_SetItalics(textAttributes.IsItalic()));
        _lastTextAttributes.SetItalics(textAttributes.IsItalic());
    }

    if (textAttributes.IsBlinking() != _lastTextAttributes.IsBlinking())
    {
        RETURN_IF_FAILED(_SetBlinking(textAttributes.IsBlinking()));
        _lastTextAttributes.SetBlinking(textAttributes.IsBlinking());
    }

    if (textAttributes.IsInvisible() != _lastTextAttributes.IsInvisible())
    {
        RETURN_IF_FAILED(_SetInvisible(textAttributes.IsInvisible()));
        _lastTextAttributes.SetInvisible(textAttributes.IsInvisible());
    }

    if (textAttributes.IsCrossedOut() != _lastTextAttributes.IsCrossedOut())
    {
        RETURN_IF_FAILED(_SetCrossedOut(textAttributes.IsCrossedOut()));
        _lastTextAttributes.SetCrossedOut(textAttributes.IsCrossedOut());
    }

    if (textAttributes.IsReverseVideo() != _lastTextAttributes.IsReverseVideo())
    {
        RETURN_IF_FAILED(_SetReverseVideo(textAttributes.IsReverseVideo()));
        _lastTextAttributes.SetReverseVideo(textAttributes.IsReverseVideo());
    }

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
