// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../renderer/inc/RenderEngineBase.hpp"

namespace Microsoft::Console::Render
{
    class WddmConEngine final : public RenderEngineBase
    {
    public:
        WddmConEngine() noexcept;
        ~WddmConEngine() override;

        [[nodiscard]] HRESULT Initialize();
        bool IsInitialized() noexcept;

        // Used to release device resources so that another instance of
        // conhost can render to the screen (i.e. only one DirectX
        // application may control the screen at a time.)
        [[nodiscard]] HRESULT Enable() noexcept override;
        [[nodiscard]] HRESULT Disable() noexcept;

        til::rect GetDisplaySize() noexcept;

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters,
                                              const til::point coord,
                                              const bool trimLeft,
                                              const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet const lines, COLORREF const color, size_t const cchLine, til::point const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const RenderSettings& renderSettings,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool usingSoftFont,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, const int iDpi) noexcept override;

        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring_view newTitle) noexcept override;

    private:
        HANDLE _hWddmConCtx;

        // Helpers
        void FreeResources(ULONG displayHeight);

        // Variables
        LONG _displayHeight;
        LONG _displayWidth;
        til::rect _dirtyArea;

        PCD_IO_ROW_INFORMATION* _displayState;

        WORD _currentLegacyColorAttribute;
    };
}
