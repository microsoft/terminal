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

ConversionAreaBufferInfo::ConversionAreaBufferInfo(const til::size coordBufferSize) :
    coordCaBuffer(coordBufferSize)
{
}

ConversionAreaInfo::ConversionAreaInfo(const til::size bufferSize,
                                       const til::size windowSize,
                                       const TextAttribute& fill,
                                       const TextAttribute& popupFill,
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
                                                                fill,
                                                                popupFill,
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
                                   const til::CoordType column)
{
    std::span<const OutputCell> view(text.data(), text.size());
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

[[nodiscard]] HRESULT ConversionAreaInfo::Resize(const til::size newSize) noexcept
{
    // attempt to resize underlying buffers
    RETURN_IF_NTSTATUS_FAILED(_screenBuffer->ResizeScreenBuffer(newSize, FALSE));

    // store new size
    _caInfo.coordCaBuffer = newSize;

    // restrict viewport to buffer size.
    const til::point restriction = { newSize.width - 1, newSize.height - 1 };
    _caInfo.rcViewCaWindow.left = std::min(_caInfo.rcViewCaWindow.left, restriction.x);
    _caInfo.rcViewCaWindow.right = std::min(_caInfo.rcViewCaWindow.right, restriction.x);
    _caInfo.rcViewCaWindow.top = std::min(_caInfo.rcViewCaWindow.top, restriction.y);
    _caInfo.rcViewCaWindow.bottom = std::min(_caInfo.rcViewCaWindow.bottom, restriction.y);

    return S_OK;
}

void ConversionAreaInfo::SetWindowInfo(const til::inclusive_rect& view) noexcept
{
    if (view.left != _caInfo.rcViewCaWindow.left ||
        view.top != _caInfo.rcViewCaWindow.top ||
        view.right != _caInfo.rcViewCaWindow.right ||
        view.bottom != _caInfo.rcViewCaWindow.bottom)
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

void ConversionAreaInfo::SetViewPos(const til::point pos) noexcept
{
    if (IsHidden())
    {
        _caInfo.coordConView = pos;
    }
    else
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        auto OldRegion = _caInfo.rcViewCaWindow;
        OldRegion.left += _caInfo.coordConView.x;
        OldRegion.right += _caInfo.coordConView.x;
        OldRegion.top += _caInfo.coordConView.y;
        OldRegion.bottom += _caInfo.coordConView.y;
        WriteToScreen(gci.GetActiveOutputBuffer(), Viewport::FromInclusive(OldRegion));

        _caInfo.coordConView = pos;

        auto NewRegion = _caInfo.rcViewCaWindow;
        NewRegion.left += _caInfo.coordConView.x;
        NewRegion.right += _caInfo.coordConView.x;
        NewRegion.top += _caInfo.coordConView.y;
        NewRegion.bottom += _caInfo.coordConView.y;
        WriteToScreen(gci.GetActiveOutputBuffer(), Viewport::FromInclusive(NewRegion));
    }
}

void ConversionAreaInfo::Paint() const noexcept
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& ScreenInfo = gci.GetActiveOutputBuffer();
    const auto viewport = ScreenInfo.GetViewport();

    til::inclusive_rect WriteRegion;
    WriteRegion.left = viewport.Left() + _caInfo.coordConView.x + _caInfo.rcViewCaWindow.left;
    WriteRegion.right = WriteRegion.left + (_caInfo.rcViewCaWindow.right - _caInfo.rcViewCaWindow.left);
    WriteRegion.top = viewport.Top() + _caInfo.coordConView.y + _caInfo.rcViewCaWindow.top;
    WriteRegion.bottom = WriteRegion.top + (_caInfo.rcViewCaWindow.bottom - _caInfo.rcViewCaWindow.top);

    if (!IsHidden())
    {
        WriteConvRegionToScreen(ScreenInfo, Viewport::FromInclusive(WriteRegion));
    }
    else
    {
        WriteToScreen(ScreenInfo, Viewport::FromInclusive(WriteRegion));
    }
}
