/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderData.hpp

Abstract:
- This serves as the interface defining all information needed to render to the screen.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "../../host/conimeinfo.h"
#include "../../buffer/out/TextAttribute.hpp"

class Cursor;

namespace Microsoft::Console::Render
{
    struct RenderOverlay final
    {
        // This is where the data is stored
        const TextBuffer& buffer;

        // This is where the top left of the stored buffer should be overlaid on the screen
        // (relative to the current visible viewport)
        const til::point origin;

        // This is the area of the buffer that is actually used for overlay.
        // Anything outside of this is considered empty by the overlay and shouldn't be used
        // for painting purposes.
        const Microsoft::Console::Types::Viewport region;
    };

    class IRenderData
    {
    public:
        virtual ~IRenderData() = default;

        // This block used to be IBaseData.
        virtual Microsoft::Console::Types::Viewport GetViewport() noexcept = 0;
        virtual til::point GetTextBufferEndPosition() const noexcept = 0;
        virtual const TextBuffer& GetTextBuffer() const noexcept = 0;
        virtual const FontInfo& GetFontInfo() const noexcept = 0;
        virtual std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept = 0;
        virtual void LockConsole() noexcept = 0;
        virtual void UnlockConsole() noexcept = 0;

        // This block used to be the original IRenderData.
        virtual til::point GetCursorPosition() const noexcept = 0;
        virtual bool IsCursorVisible() const noexcept = 0;
        virtual bool IsCursorOn() const noexcept = 0;
        virtual ULONG GetCursorHeight() const noexcept = 0;
        virtual CursorType GetCursorStyle() const noexcept = 0;
        virtual ULONG GetCursorPixelWidth() const noexcept = 0;
        virtual bool IsCursorDoubleWidth() const = 0;
        virtual const std::vector<RenderOverlay> GetOverlays() const noexcept = 0;
        virtual const bool IsGridLineDrawingAllowed() noexcept = 0;
        virtual const std::wstring_view GetConsoleTitle() const noexcept = 0;
        virtual const std::wstring GetHyperlinkUri(uint16_t id) const = 0;
        virtual const std::wstring GetHyperlinkCustomId(uint16_t id) const = 0;
        virtual const std::vector<size_t> GetPatternId(const til::point location) const = 0;

        // This block used to be IUiaData.
        virtual std::pair<COLORREF, COLORREF> GetAttributeColors(const TextAttribute& attr) const noexcept = 0;
        virtual const bool IsSelectionActive() const = 0;
        virtual const bool IsBlockSelection() const = 0;
        virtual void ClearSelection() = 0;
        virtual void SelectNewRegion(const til::point coordStart, const til::point coordEnd) = 0;
        virtual const til::point GetSelectionAnchor() const noexcept = 0;
        virtual const til::point GetSelectionEnd() const noexcept = 0;
        virtual void ColorSelection(const til::point coordSelectionStart, const til::point coordSelectionEnd, const TextAttribute attr) = 0;
        virtual const bool IsUiaDataInitialized() const noexcept = 0;
    };
}
