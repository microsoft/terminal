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

#include "../inc/IRenderer.hpp"
#include "../inc/IRenderEngine.hpp"
#include "../inc/IRenderData.hpp"

#include "thread.hpp"

#include "../../buffer/out/textBuffer.hpp"
#include "../../buffer/out/CharRow.hpp"

namespace Microsoft::Console::Render
{
    class Renderer sealed : public IRenderer
    {
    public:
        Renderer(IRenderData* pData,
                 _In_reads_(cEngines) IRenderEngine** const pEngine,
                 const size_t cEngines,
                 std::unique_ptr<IRenderThread> thread);

        [[nodiscard]] static HRESULT s_CreateInstance(IRenderData* pData,
                                                      _In_reads_(cEngines) IRenderEngine** const rgpEngines,
                                                      const size_t cEngines,
                                                      _Outptr_result_nullonfailure_ Renderer** const ppRenderer);

        [[nodiscard]] static HRESULT s_CreateInstance(IRenderData* pData,
                                                      _Outptr_result_nullonfailure_ Renderer** const ppRenderer);

        virtual ~Renderer() override;

        [[nodiscard]] HRESULT PaintFrame();

        void TriggerSystemRedraw(const RECT* const prcDirtyClient) override;
        void TriggerRedraw(const Microsoft::Console::Types::Viewport& region) override;
        void TriggerRedraw(const COORD* const pcoord) override;
        void TriggerRedrawCursor(const COORD* const pcoord) override;
        void TriggerRedrawAll() override;
        void TriggerTeardown() noexcept override;

        void TriggerSelection() override;
        void TriggerScroll() override;
        void TriggerScroll(const COORD* const pcoordDelta) override;

        void TriggerCircling() override;
        void TriggerTitleChange() override;

        void TriggerFontChange(const int iDpi,
                               const FontInfoDesired& FontInfoDesired,
                               _Out_ FontInfo& FontInfo) override;

        void UpdateSoftFont(const gsl::span<const uint16_t> bitPattern,
                            const SIZE cellSize,
                            const size_t centeringHint) override;

        [[nodiscard]] HRESULT GetProposedFont(const int iDpi,
                                              const FontInfoDesired& FontInfoDesired,
                                              _Out_ FontInfo& FontInfo) override;

        bool IsGlyphWideByFont(const std::wstring_view glyph) override;

        void EnablePainting() override;
        void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs) override;
        void WaitUntilCanRender() override;

        void AddRenderEngine(_In_ IRenderEngine* const pEngine) override;

        void SetRendererEnteredErrorStateCallback(std::function<void()> pfn);
        void ResetErrorStateAndResume();

        void UpdateLastHoveredInterval(const std::optional<interval_tree::IntervalTree<til::point, size_t>::interval>& newInterval);

    private:
        static IRenderEngine::GridLines s_GetGridlines(const TextAttribute& textAttribute) noexcept;
        static bool s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar);

        void _NotifyPaintFrame();
        [[nodiscard]] HRESULT _PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept;
        bool _CheckViewportAndScroll();
        [[nodiscard]] HRESULT _PaintBackground(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutput(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine, TextBufferCellIterator it, const COORD target, const bool lineWrapped);
        void _PaintBufferOutputGridLineHelper(_In_ IRenderEngine* const pEngine, const TextAttribute textAttribute, const size_t cchLine, const COORD coordTarget);
        void _PaintSelection(_In_ IRenderEngine* const pEngine);
        void _PaintCursor(_In_ IRenderEngine* const pEngine);
        void _PaintOverlays(_In_ IRenderEngine* const pEngine);
        void _PaintOverlay(IRenderEngine& engine, const RenderOverlay& overlay);
        [[nodiscard]] HRESULT _UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine, const TextAttribute attr, const bool usingSoftFont, const bool isSettingDefaultBrushes);
        [[nodiscard]] HRESULT _PerformScrolling(_In_ IRenderEngine* const pEngine);
        std::vector<SMALL_RECT> _GetSelectionRects() const;
        void _ScrollPreviousSelection(const til::point delta);
        [[nodiscard]] HRESULT _PaintTitle(IRenderEngine* const pEngine);
        [[nodiscard]] std::optional<CursorOptions> _GetCursorInfo();
        [[nodiscard]] HRESULT _PrepareRenderInfo(_In_ IRenderEngine* const pEngine);

        std::array<IRenderEngine*, 2> _engines{};
        IRenderData* _pData = nullptr; // Non-ownership pointer
        std::unique_ptr<IRenderThread> _pThread;
        static constexpr size_t _firstSoftFontChar = 0xEF20;
        size_t _lastSoftFontChar = 0;
        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _hoveredInterval;
        Microsoft::Console::Types::Viewport _viewport;
        std::vector<Cluster> _clusterBuffer;
        std::vector<SMALL_RECT> _previousSelection;
        std::function<void()> _pfnRendererEnteredErrorState;
        bool _destructing = false;

#ifdef UNIT_TESTING
        friend class ConptyOutputTests;
#endif
    };
}
