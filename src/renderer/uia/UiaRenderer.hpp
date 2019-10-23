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

#include "../../types/inc/Viewport.hpp"

namespace Microsoft::Console::Render
{
    class UiaEngine final : public RenderEngineBase
    {
    public:
        UiaEngine() noexcept;

        // Only one UiaEngine may present information at a time.
        // This ensures that an automation client isn't overwhelmed
        // by events when there are multiple TermControls
        [[nodiscard]] HRESULT Enable() noexcept;
        [[nodiscard]] HRESULT Disable() noexcept;

        // IRenderEngine Members
        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                              COORD const coord,
                                              bool const fTrimLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLines const lines, COLORREF const color, size_t const cchLine, COORD const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                   const COLORREF colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int const iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, int const iDpi) noexcept override;

        [[nodiscard]] SMALL_RECT GetDirtyRectInChars() noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(const std::wstring& newTitle) noexcept override;

    private:
        bool _isEnabled;
        bool _isPainting;
    };
}
