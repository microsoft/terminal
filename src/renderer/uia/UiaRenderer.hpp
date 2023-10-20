/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UiaRenderer.hpp

Abstract:
- This is the definition of the UIA specific implementation of the renderer
- It keeps track of what regions of the display have changed and notifies automation clients.

Author(s):
- Carlos Zamora (CaZamor) Sep-2019
--*/

#pragma once

#include "../../renderer/inc/RenderEngineBase.hpp"

#include "../../types/IUiaEventDispatcher.h"
#include "../../types/inc/Viewport.hpp"

namespace Microsoft::Console::Render
{
    class UiaEngine final : public RenderEngineBase
    {
    public:
        UiaEngine(Microsoft::Console::Types::IUiaEventDispatcher* dispatcher);

        // Only one UiaEngine may present information at a time.
        // This ensures that an automation client isn't overwhelmed
        // by events when there are multiple TermControls
        [[nodiscard]] HRESULT Enable() noexcept override;
        [[nodiscard]] HRESULT Disable() noexcept;

        // IRenderEngine Members
        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        void WaitUntilCanRender() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT ScrollFrame() noexcept override;
        [[nodiscard]] HRESULT Invalidate(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT NotifyNewText(const std::wstring_view newText) noexcept override;
        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters, const til::point coord, const bool fTrimLeft, const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(const GridLineSet lines, const COLORREF color, const size_t cchLine, const til::point coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;
        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;
        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, const gsl::not_null<IRenderData*> pData, const bool usingSoftFont, const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;
        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, const int iDpi) noexcept override;
        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(const std::wstring_view newTitle) noexcept override;

    private:
        bool _isEnabled;
        bool _isPainting;
        bool _selectionChanged;
        bool _textBufferChanged;
        bool _cursorChanged;
        std::wstring _newOutput;
        std::wstring _queuedOutput;

        Microsoft::Console::Types::IUiaEventDispatcher* _dispatcher;

        std::vector<til::rect> _prevSelection;
        til::rect _prevCursorRegion;
    };
}
