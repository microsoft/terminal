/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Renderer.hpp

Abstract:
- This is the definition of our renderer.
- It provides interfaces for the console application to notify when various portions of the console state have changed and need to be redrawn.
- It requires a data interface to fetch relevant console structures required for drawing and a drawing engine target for output.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "../inc/IRenderEngine.hpp"
#include "../inc/RenderSettings.hpp"

#include "thread.hpp"

#include "../../buffer/out/textBuffer.hpp"

// fwdecl unittest classes
#ifdef UNIT_TESTING
namespace TerminalCoreUnitTests
{
    class ConptyRoundtripTests;
};
#endif

namespace Microsoft::Console::Render
{
    class Renderer
    {
    public:
        Renderer(const RenderSettings& renderSettings,
                 IRenderData* pData,
                 _In_reads_(cEngines) IRenderEngine** const pEngine,
                 const size_t cEngines,
                 std::unique_ptr<RenderThread> thread);

        ~Renderer();

        [[nodiscard]] HRESULT PaintFrame();

        void NotifyPaintFrame() noexcept;
        void TriggerSystemRedraw(const til::rect* const prcDirtyClient);
        void TriggerRedraw(const Microsoft::Console::Types::Viewport& region);
        void TriggerRedraw(const til::point* const pcoord);
        void TriggerRedrawCursor(const til::point* const pcoord);
        void TriggerRedrawAll(const bool backgroundChanged = false, const bool frameChanged = false);
        void TriggerTeardown() noexcept;

        void TriggerSelection();
        void TriggerScroll();
        void TriggerScroll(const til::point* const pcoordDelta);

        void TriggerFlush(const bool circling);
        void TriggerTitleChange();

        void TriggerNewTextNotification(const std::wstring_view newText);

        void TriggerFontChange(const int iDpi,
                               const FontInfoDesired& FontInfoDesired,
                               _Out_ FontInfo& FontInfo);

        void UpdateSoftFont(const std::span<const uint16_t> bitPattern,
                            const til::size cellSize,
                            const size_t centeringHint);

        [[nodiscard]] HRESULT GetProposedFont(const int iDpi,
                                              const FontInfoDesired& FontInfoDesired,
                                              _Out_ FontInfo& FontInfo);

        bool IsGlyphWideByFont(const std::wstring_view glyph);

        void EnablePainting();
        void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs);
        void WaitUntilCanRender();

        void AddRenderEngine(_In_ IRenderEngine* const pEngine);
        void RemoveRenderEngine(_In_ IRenderEngine* const pEngine);

        void SetBackgroundColorChangedCallback(std::function<void()> pfn);
        void SetFrameColorChangedCallback(std::function<void()> pfn);
        void SetRendererEnteredErrorStateCallback(std::function<void()> pfn);
        void ResetErrorStateAndResume();

        void UpdateHyperlinkHoveredId(uint16_t id) noexcept;
        void UpdateLastHoveredInterval(const std::optional<interval_tree::IntervalTree<til::point, size_t>::interval>& newInterval);

    private:
        static GridLineSet s_GetGridlines(const TextAttribute& textAttribute) noexcept;
        static bool s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar);

        [[nodiscard]] HRESULT _PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept;
        bool _CheckViewportAndScroll();
        [[nodiscard]] HRESULT _PaintBackground(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutput(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine, TextBufferCellIterator it, const til::point target, const bool lineWrapped);
        void _PaintBufferOutputGridLineHelper(_In_ IRenderEngine* const pEngine, const TextAttribute textAttribute, const size_t cchLine, const til::point coordTarget);
        bool _isHoveredHyperlink(const TextAttribute& textAttribute) const noexcept;
        void _PaintSelection(_In_ IRenderEngine* const pEngine);
        void _PaintCursor(_In_ IRenderEngine* const pEngine);
        void _PaintOverlays(_In_ IRenderEngine* const pEngine);
        void _PaintOverlay(IRenderEngine& engine, const RenderOverlay& overlay);
        [[nodiscard]] HRESULT _UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine, const TextAttribute attr, const bool usingSoftFont, const bool isSettingDefaultBrushes);
        [[nodiscard]] HRESULT _PerformScrolling(_In_ IRenderEngine* const pEngine);
        std::vector<til::rect> _GetSelectionRects() const;
        void _ScrollPreviousSelection(const til::point delta);
        [[nodiscard]] HRESULT _PaintTitle(IRenderEngine* const pEngine);
        bool _isInHoveredInterval(til::point coordTarget) const noexcept;
        [[nodiscard]] std::optional<CursorOptions> _GetCursorInfo();
        [[nodiscard]] HRESULT _PrepareRenderInfo(_In_ IRenderEngine* const pEngine);

        const RenderSettings& _renderSettings;
        std::array<IRenderEngine*, 2> _engines{};
        IRenderData* _pData = nullptr; // Non-ownership pointer
        std::unique_ptr<RenderThread> _pThread;
        static constexpr size_t _firstSoftFontChar = 0xEF20;
        size_t _lastSoftFontChar = 0;
        uint16_t _hyperlinkHoveredId = 0;
        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _hoveredInterval;
        Microsoft::Console::Types::Viewport _viewport;
        std::vector<Cluster> _clusterBuffer;
        std::vector<til::rect> _previousSelection;
        std::function<void()> _pfnBackgroundColorChanged;
        std::function<void()> _pfnFrameColorChanged;
        std::function<void()> _pfnRendererEnteredErrorState;
        bool _destructing = false;
        bool _forceUpdateViewport = false;

#ifdef UNIT_TESTING
        friend class ConptyOutputTests;
        friend class TerminalCoreUnitTests::ConptyRoundtripTests;
#endif
    };
}
