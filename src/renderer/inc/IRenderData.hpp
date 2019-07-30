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
#include "../../types/inc/viewport.hpp"

class TextBuffer;
class Cursor;
struct ITextRangeProvider;

namespace Microsoft::Console::Render
{
    struct RenderOverlay final
    {
        // This is where the data is stored
        const TextBuffer& buffer;

        // This is where the top left of the stored buffer should be overlayed on the screen
        // (relative to the current visible viewport)
        const COORD origin;

        // This is the area of the buffer that is actually used for overlay.
        // Anything outside of this is considered empty by the overlay and shouldn't be used
        // for painting purposes.
        const Microsoft::Console::Types::Viewport region;
    };

    class IRenderData
    {
    public:
        virtual ~IRenderData() = 0;
        virtual Microsoft::Console::Types::Viewport GetViewport() noexcept = 0;
        virtual const TextBuffer& GetTextBuffer() noexcept = 0;
        virtual const FontInfo& GetFontInfo() noexcept = 0;
        virtual const TextAttribute GetDefaultBrushColors() noexcept = 0;

        virtual const COLORREF GetForegroundColor(const TextAttribute& attr) const noexcept = 0;
        virtual const COLORREF GetBackgroundColor(const TextAttribute& attr) const noexcept = 0;

        virtual COORD GetCursorPosition() const noexcept = 0;
        virtual bool IsCursorVisible() const noexcept = 0;
        virtual bool IsCursorOn() const noexcept = 0;
        virtual ULONG GetCursorHeight() const noexcept = 0;
        virtual CursorType GetCursorStyle() const noexcept = 0;
        virtual ULONG GetCursorPixelWidth() const noexcept = 0;
        virtual COLORREF GetCursorColor() const noexcept = 0;
        virtual bool IsCursorDoubleWidth() const noexcept = 0;

        virtual const std::vector<RenderOverlay> GetOverlays() const noexcept = 0;

        virtual const bool IsGridLineDrawingAllowed() noexcept = 0;

        // TODO GitHub #1992: Move some of these functions to IAccessibilityData (or IUiaData)
        virtual std::vector<Microsoft::Console::Types::Viewport> GetSelectionRects() noexcept = 0;
        virtual bool IsAreaSelected() const = 0;
        virtual void ClearSelection() = 0;
        virtual void SelectNewRegion(const COORD coordStart, const COORD coordEnd) = 0;

        // TODO GitHub #605: Search functionality
        // For now, just adding it here to make UiaTextRange easier to create (Accessibility)
        // We should actually abstract this out better once Windows Terminal has Search
        virtual HRESULT SearchForText(_In_ BSTR text,
                                      _In_ BOOL searchBackward,
                                      _In_ BOOL ignoreCase,
                                      _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal,
                                      unsigned int _start,
                                      unsigned int _end,
                                      std::function<unsigned int(IRenderData*, const COORD)> _coordToEndpoint,
                                      std::function<COORD(IRenderData*, const unsigned int)> _endpointToCoord,
                                      std::function<IFACEMETHODIMP(ITextRangeProvider**)> Clone) = 0;

        virtual const std::wstring GetConsoleTitle() const noexcept = 0;

        virtual void LockConsole() noexcept = 0;
        virtual void UnlockConsole() noexcept = 0;
    };

    // See docs/virtual-dtors.md for an explanation of why this is weird.
    inline IRenderData::~IRenderData() {}
}
