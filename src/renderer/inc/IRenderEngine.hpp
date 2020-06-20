/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderEngine.hpp

Abstract:
- This serves as the entry point for a specific graphics engine specific renderer.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "CursorOptions.h"
#include "Cluster.hpp"
#include "FontInfoDesired.hpp"

namespace Microsoft::Console::Render
{
    struct RenderFrameInfo
    {
        std::optional<CursorOptions> cursorInfo;
    };

    class IRenderEngine
    {
    public:
        enum GridLines
        {
            None = 0x0,
            Top = 0x1,
            Bottom = 0x2,
            Left = 0x4,
            Right = 0x8
        };

        virtual ~IRenderEngine() = 0;

    protected:
        IRenderEngine() = default;
        IRenderEngine(const IRenderEngine&) = default;
        IRenderEngine(IRenderEngine&&) = default;
        IRenderEngine& operator=(const IRenderEngine&) = default;
        IRenderEngine& operator=(IRenderEngine&&) = default;

    public:
        [[nodiscard]] virtual HRESULT StartPaint() noexcept = 0;
        [[nodiscard]] virtual HRESULT EndPaint() noexcept = 0;

        virtual void WaitUntilCanRender() noexcept = 0;
        [[nodiscard]] virtual HRESULT Present() noexcept = 0;

        [[nodiscard]] virtual HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept = 0;

        [[nodiscard]] virtual HRESULT ScrollFrame() noexcept = 0;

        [[nodiscard]] virtual HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateAll() noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept = 0;

        [[nodiscard]] virtual HRESULT InvalidateTitle(const std::wstring& proposedTitle) noexcept = 0;

        [[nodiscard]] virtual HRESULT PrepareRenderInfo(const RenderFrameInfo& info) noexcept = 0;

        [[nodiscard]] virtual HRESULT PaintBackground() noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                      const COORD coord,
                                                      const bool fTrimLeft,
                                                      const bool lineWrapped) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferGridLines(const GridLines lines,
                                                           const COLORREF color,
                                                           const size_t cchLine,
                                                           const COORD coordTarget) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintSelection(const SMALL_RECT rect) noexcept = 0;

        [[nodiscard]] virtual HRESULT PaintCursor(const CursorOptions& options) noexcept = 0;

        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const WORD legacyColorAttribute,
                                                           const ExtendedAttributes extendedAttrs,
                                                           const bool isSettingDefaultBrushes) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired,
                                                 _Out_ FontInfo& FontInfo) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateDpi(const int iDpi) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept = 0;

        [[nodiscard]] virtual HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired,
                                                      _Out_ FontInfo& FontInfo,
                                                      const int iDpi) noexcept = 0;

        virtual std::vector<til::rectangle> GetDirtyArea() = 0;
        [[nodiscard]] virtual HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept = 0;
        [[nodiscard]] virtual HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateTitle(const std::wstring& newTitle) noexcept = 0;
    };

    inline Microsoft::Console::Render::IRenderEngine::~IRenderEngine() {}
}

DEFINE_ENUM_FLAG_OPERATORS(Microsoft::Console::Render::IRenderEngine::GridLines)
