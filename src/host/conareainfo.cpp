// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "conareainfo.h"

#include "_output.h"

#include "../interactivity/inc/ServiceLocator.hpp"
#include "../types/inc/viewport.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Types;
using Microsoft::Console::Interactivity::ServiceLocator;

ConversionAreaBufferInfo::ConversionAreaBufferInfo(const COORD coordBufferSize) :
    coordCaBuffer(coordBufferSize),
    rcViewCaWindow({ 0 }),
    coordConView({ 0 })
{
}

ConversionAreaInfo::ConversionAreaInfo(const COORD bufferSize,
                                       const COORD windowSize,
                                       const CHAR_INFO fill,
                                       const CHAR_INFO popupFill,
                                       const FontInfo fontInfo) :
    _caInfo{ bufferSize },
    _isHidden{ true },
    _screenBuffer{ nullptr }
{
    SCREEN_INFORMATION* pNewScreen = nullptr;

    // cursor has no height because it won't be rendered for conversion area
    THROW_IF_NTSTATUS_FAILED(SCREEN_INFORMATION::CreateInstance(windowSize,
                                                                fontInfo,
                                                                bufferSize,
                                                                { fill.Attributes },
                                                                { popupFill.Attributes },
                                                                0,
                                                                &pNewScreen));

    // Suppress painting notifications for modifying a conversion area cursor as they're not actually rendered.
    pNewScreen->GetTextBuffer().GetCursor().SetIsConversionArea(true);
    pNewScreen->ConvScreenInfo = this;

    _screenBuffer.reset(pNewScreen);
}

ConversionAreaInfo::ConversionAreaInfo(ConversionAreaInfo&& other) :
    _caInfo(other._caInfo),
    _isHidden(other._isHidden),
    _screenBuffer(nullptr)
{
    std::swap(_screenBuffer, other._screenBuffer);
}

// Routine Description:
// - Describes whether the conversion area should be drawn or should be hidden.
// Arguments:
// - <none>
// Return Value:
// - True if it should not be drawn. False if it should be drawn.
bool ConversionAreaInfo::IsHidden() const noexcept
{
    return _isHidden;
}

// Routine Description:
// - Sets a value describing whether the conversion area should be drawn or should be hidden.
// Arguments:
// - fIsHidden - True if it should not be drawn. False if it should be drawn.
// Return Value:
// - <none>
void ConversionAreaInfo::SetHidden(const bool fIsHidden) noexcept
{
    _isHidden = fIsHidden;
}

// Routine Description:
// - Retrieves the underlying text buffer for use in rendering data
const TextBuffer& ConversionAreaInfo::GetTextBuffer() const noexcept
{
    return _screenBuffer->GetTextBuffer();
}

// Routine Description:
// - Retrieves the layout/overlay information about where to place this conversion area relative to the
//   existing screen buffers and viewports.
const ConversionAreaBufferInfo& ConversionAreaInfo::GetAreaBufferInfo() const noexcept
{
    return _caInfo;
}

// Routine Description:
// - Forwards a color attribute setting request to the internal screen information
// Arguments:
// - attr - Color to apply to internal screen buffer
void ConversionAreaInfo::SetAttributes(const TextAttribute& attr)
{
    _screenBuffer->SetAttributes(attr);
}

// Routine Description:
// - Writes text into the conversion area. Since conversion areas are only
//   one line, you can only specify the column to write.
// Arguments:
// - text - Text to insert into the conversion area buffer
// - column - Column to start at (X position)
void ConversionAreaInfo::WriteText(const std::vector<OutputCell>& text,
                                   const SHORT column)
{
    std::basic_string_view<OutputCell> view(text.data(), text.size());
    _screenBuffer->Write(view, { column, 0 });
}

// Routine Description:
// - Clears out a conversion area
void ConversionAreaInfo::ClearArea() noexcept
{
    SetHidden(true);

    try
    {
        _screenBuffer->ClearTextData();
    }
    CATCH_LOG();

    Paint();
}

[[nodiscard]] HRESULT ConversionAreaInfo::Resize(const COORD newSize) noexcept
{
    // attempt to resize underlying buffers
    RETURN_IF_NTSTATUS_FAILED(_screenBuffer->ResizeScreenBuffer(newSize, FALSE));

    // store new size
    _caInfo.coordCaBuffer = newSize;

    // restrict viewport to buffer size.
    const COORD restriction = { newSize.X - 1i16, newSize.Y - 1i16 };
    _caInfo.rcViewCaWindow.Left = std::min(_caInfo.rcViewCaWindow.Left, restriction.X);
    _caInfo.rcViewCaWindow.Right = std::min(_caInfo.rcViewCaWindow.Right, restriction.X);
    _caInfo.rcViewCaWindow.Top = std::min(_caInfo.rcViewCaWindow.Top, restriction.Y);
    _caInfo.rcViewCaWindow.Bottom = std::min(_caInfo.rcViewCaWindow.Bottom, restriction.Y);

    return S_OK;
}

void ConversionAreaInfo::SetWindowInfo(const SMALL_RECT view) noexcept
{
    if (view.Left != _caInfo.rcViewCaWindow.Left ||
        view.Top != _caInfo.rcViewCaWindow.Top ||
        view.Right != _caInfo.rcViewCaWindow.Right ||
        view.Bottom != _caInfo.rcViewCaWindow.Bottom)
    {
        if (!IsHidden())
        {
            SetHidden(true);
            Paint();

            _caInfo.rcViewCaWindow = view;
            SetHidden(false);
            Paint();
        }
        else
        {
            _caInfo.rcViewCaWindow = view;
        }
    }
}

void ConversionAreaInfo::SetViewPos(const COORD pos) noexcept
{
    if (IsHidden())
    {
        _caInfo.coordConView = pos;
    }
    else
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        SMALL_RECT OldRegion = _caInfo.rcViewCaWindow;
        OldRegion.Left += _caInfo.coordConView.X;
        OldRegion.Right += _caInfo.coordConView.X;
        OldRegion.Top += _caInfo.coordConView.Y;
        OldRegion.Bottom += _caInfo.coordConView.Y;
        WriteToScreen(gci.GetActiveOutputBuffer(), Viewport::FromInclusive(OldRegion));

        _caInfo.coordConView = pos;

        SMALL_RECT NewRegion = _caInfo.rcViewCaWindow;
        NewRegion.Left += _caInfo.coordConView.X;
        NewRegion.Right += _caInfo.coordConView.X;
        NewRegion.Top += _caInfo.coordConView.Y;
        NewRegion.Bottom += _caInfo.coordConView.Y;
        WriteToScreen(gci.GetActiveOutputBuffer(), Viewport::FromInclusive(NewRegion));
    }
}

void ConversionAreaInfo::Paint() const noexcept
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();
    const auto viewport = ScreenInfo.GetViewport();

    SMALL_RECT WriteRegion;
    WriteRegion.Left = viewport.Left() + _caInfo.coordConView.X + _caInfo.rcViewCaWindow.Left;
    WriteRegion.Right = WriteRegion.Left + (_caInfo.rcViewCaWindow.Right - _caInfo.rcViewCaWindow.Left);
    WriteRegion.Top = viewport.Top() + _caInfo.coordConView.Y + _caInfo.rcViewCaWindow.Top;
    WriteRegion.Bottom = WriteRegion.Top + (_caInfo.rcViewCaWindow.Bottom - _caInfo.rcViewCaWindow.Top);

    if (!IsHidden())
    {
        WriteConvRegionToScreen(ScreenInfo, Viewport::FromInclusive(WriteRegion));
    }
    else
    {
        WriteToScreen(ScreenInfo, Viewport::FromInclusive(WriteRegion));
    }
}
