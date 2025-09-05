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

#include "../../buffer/out/textBuffer.hpp"
#include "../inc/IRenderEngine.hpp"
#include "../inc/RenderSettings.hpp"

namespace Microsoft::Console::Render
{
    class Renderer
    {
    public:
        struct TimerHandle
        {
            explicit operator bool() const noexcept
            {
                return id != SIZE_T_MAX;
            }

            size_t id = SIZE_T_MAX;
        };

        using TimerRepr = ULONGLONG;
        using TimerDuration = std::chrono::duration<TimerRepr, std::ratio<1, 10000000>>;
        using TimerCallback = std::function<void(Renderer&, TimerHandle)>;

        Renderer(RenderSettings& renderSettings, IRenderData* pData);
        ~Renderer();

        IRenderData* GetRenderData() const noexcept;

        TimerHandle RegisterTimer(const char* description, TimerCallback routine);
        void StartTimerWithInterval(TimerHandle handle, TimerDuration interval);
        void StopTimer(TimerHandle handle);

        void NotifyPaintFrame() noexcept;
        void SynchronizedOutputChanged() noexcept;
        void InhibitCursorVisibility(InhibitionSource source, bool enable) noexcept;
        void InhibitCursorBlinking(InhibitionSource source, bool enable) noexcept;
        void TriggerSystemRedraw(const til::rect* const prcDirtyClient);
        void TriggerRedraw(const Microsoft::Console::Types::Viewport& region);
        void TriggerRedraw(const til::point* const pcoord);
        void TriggerRedrawAll(const bool backgroundChanged = false, const bool frameChanged = false);
        void TriggerTeardown() noexcept;

        void TriggerSelection();
        void TriggerSearchHighlight(const std::vector<til::point_span>& oldHighlights);
        void TriggerScroll();
        void TriggerScroll(const til::point* const pcoordDelta);

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

        void AddRenderEngine(_In_ IRenderEngine* const pEngine);
        void RemoveRenderEngine(_In_ IRenderEngine* const pEngine);

        void SetBackgroundColorChangedCallback(std::function<void()> pfn);
        void SetFrameColorChangedCallback(std::function<void()> pfn);
        void SetRendererEnteredErrorStateCallback(std::function<void()> pfn);

        void UpdateHyperlinkHoveredId(uint16_t id) noexcept;
        void UpdateLastHoveredInterval(const std::optional<interval_tree::IntervalTree<til::point, size_t>::interval>& newInterval);

    private:
        struct TimerRoutine
        {
            const char* description = nullptr;
            TimerRepr interval = 0; // Timers with a 0 interval are marked for deletion.
            TimerRepr next = 0;
            TimerCallback routine;
        };

        // Caches some essential information about the active composition.
        // This allows us to properly invalidate it between frames, etc.
        struct CompositionCache
        {
            til::point absoluteOrigin;
            TextAttribute baseAttribute;
        };

        static GridLineSet s_GetGridlines(const TextAttribute& textAttribute) noexcept;
        static bool s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar);

        // Base rendering loop
        static DWORD s_renderThread(void*) noexcept;
        DWORD _renderThread() noexcept;
        void _waitUntilCanRender() noexcept;

        // Timer handling
        DWORD _calculateTimerMaxWait() noexcept;
        void _tickTimers() noexcept;
        TimerRoutine& _getTimer(TimerHandle handle) noexcept;
        static TimerRepr _timerInstant() noexcept;
        static TimerRepr _timerSaturatingAdd(TimerRepr a, TimerRepr b) noexcept;
        static TimerRepr _timerSaturatingSub(TimerRepr a, TimerRepr b) noexcept;
        static DWORD _timerToMillis(TimerRepr t) noexcept;

        // Actual rendering
        [[nodiscard]] HRESULT PaintFrame();
        [[nodiscard]] HRESULT _PaintFrame() noexcept;
        [[nodiscard]] HRESULT _PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept;
        void _disablePainting() noexcept;
        void _synchronizeWithOutput() noexcept;
        bool _CheckViewportAndScroll();
        [[nodiscard]] HRESULT _PaintBackground(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutput(_In_ IRenderEngine* const pEngine);
        void _PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine, TextBufferCellIterator it, const til::point target, const bool lineWrapped);
        void _PaintBufferOutputGridLineHelper(_In_ IRenderEngine* const pEngine, const TextAttribute textAttribute, const size_t cchLine, const til::point coordTarget);
        bool _isHoveredHyperlink(const TextAttribute& textAttribute) const noexcept;
        void _PaintSelection(_In_ IRenderEngine* const pEngine);
        void _PaintCursor(_In_ IRenderEngine* const pEngine);
        [[nodiscard]] HRESULT _UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine, const TextAttribute attr, const bool usingSoftFont, const bool isSettingDefaultBrushes);
        [[nodiscard]] HRESULT _PerformScrolling(_In_ IRenderEngine* const pEngine);
        void _ScrollPreviousSelection(const til::point delta);
        [[nodiscard]] HRESULT _PaintTitle(IRenderEngine* const pEngine);
        bool _isInHoveredInterval(til::point coordTarget) const noexcept;
        void _updateCursorInfo();
        void _invalidateCurrentCursor() const;
        void _blinkMotherfucker();
        void _invalidateOldComposition() const;
        void _prepareNewComposition();
        [[nodiscard]] HRESULT _PrepareRenderInfo(_In_ IRenderEngine* const pEngine);

        // Constructor parameters, weakly referenced
        RenderSettings& _renderSettings;
        IRenderData* _pData = nullptr; // Non-ownership pointer

        // Base render loop & timer management
        wil::srwlock _threadMutex;
        wil::unique_handle _thread;
        wil::slim_event_manual_reset _enable;
        std::atomic<bool> _redraw;
        std::atomic<bool> _threadKeepRunning{ false };
        til::small_vector<IRenderEngine*, 2> _engines;
        til::small_vector<TimerRoutine, 4> _timers;
        size_t _nextTimerId = 0;

        static constexpr size_t _firstSoftFontChar = 0xEF20;
        size_t _lastSoftFontChar = 0;

        uint16_t _hyperlinkHoveredId = 0;
        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _hoveredInterval;

        Microsoft::Console::Types::Viewport _viewport;
        CursorOptions _currentCursorOptions{};
        TimerHandle _cursorBlinker;
        std::optional<CompositionCache> _compositionCache;
        std::vector<Cluster> _clusterBuffer;
        std::function<void()> _pfnBackgroundColorChanged;
        std::function<void()> _pfnFrameColorChanged;
        std::function<void()> _pfnRendererEnteredErrorState;
        bool _isSynchronizingOutput = false;
        bool _forceUpdateViewport = false;

        til::point_span _lastSelectionPaintSpan{};
        size_t _lastSelectionPaintSize{};
        std::vector<til::rect> _lastSelectionRectsByViewport{};
    };
}
