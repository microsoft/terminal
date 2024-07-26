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

#include "../../buffer/out/TextAttribute.hpp"
#include "../../renderer/inc/FontInfo.hpp"
#include "../../types/inc/viewport.hpp"

class Cursor;
class TextBuffer;

namespace Microsoft::Console::Render
{
    struct CompositionRange
    {
        size_t len; // The number of chars in Composition::text that this .attr applies to
        TextAttribute attr;
    };

    struct Composition
    {
        std::wstring text;
        til::small_vector<CompositionRange, 2> attributes;
        size_t cursorPos = 0;
    };

    class IRenderData
    {
    public:
        virtual ~IRenderData() = default;

        // This block used to be IBaseData.
        virtual Microsoft::Console::Types::Viewport GetViewport() noexcept = 0;
        virtual til::point GetTextBufferEndPosition() const noexcept = 0;
        virtual TextBuffer& GetTextBuffer() const noexcept = 0;
        virtual const FontInfo& GetFontInfo() const noexcept = 0;
        virtual std::span<const til::point_span> GetSearchHighlights() const noexcept = 0;
        virtual const til::point_span* GetSearchHighlightFocused() const noexcept = 0;
        virtual std::span<const til::point_span> GetSelectionSpans() const noexcept = 0;
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
        virtual const bool IsUiaDataInitialized() const noexcept = 0;

        // Ideally this would not be stored on an interface, however ideally IRenderData should not be an interface in the first place.
        // This is because we should have only 1 way how to represent render data across the codebase anyway, and it should
        // be by-value in a struct so that we can snapshot it and release the terminal lock as quickly as possible.
        const Composition& GetActiveComposition() const noexcept
        {
            return !snippetPreview.text.empty() ? snippetPreview : tsfPreview;
        }

        Composition tsfPreview;
        Composition snippetPreview;
    };
}
