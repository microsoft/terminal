// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../../buffer/out/textBuffer.hpp"
#include "../inc/IRenderEngine.hpp"
#include "../inc/RenderSettings.hpp"

namespace Microsoft::Console::Render
{
    enum class InhibitionSource
    {
        Client, // E.g. VT sequences
        Host, // E.g. because the window is out of focus
        User, // The user turned it off
    };

    class Renderer
    {
    public:
        Renderer(RenderSettings& renderSettings, IRenderData* pData);
        ~Renderer();

        IRenderData* GetRenderData() const noexcept;

        TimerHandle RegisterTimer(const char* description, TimerCallback routine);
        bool IsTimerRunning(TimerHandle handle) const;
        TimerDuration GetTimerInterval(TimerHandle handle) const;
        void StarTimer(TimerHandle handle, TimerDuration delay);
        void StartRepeatingTimer(TimerHandle handle, TimerDuration interval);
        void StopTimer(TimerHandle handle);

        void NotifyPaintFrame() noexcept;
        void SynchronizedOutputChanged() noexcept;
        void AllowCursorVisibility(InhibitionSource source, bool enable) noexcept;
        void AllowCursorBlinking(InhibitionSource source, bool enable) noexcept;
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
        static DWORD WINAPI s_renderThread(void*) noexcept;
        DWORD _renderThread() noexcept;
        void _waitUntilCanRender() noexcept;

        // Timer handling
        void _starTimer(TimerHandle handle, TimerRepr delay, TimerRepr interval);
        DWORD _calculateTimerMaxWait() noexcept;
        void _waitUntilTimerOrRedraw() noexcept;
        void _tickTimers() noexcept;
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
        void _scheduleRenditionBlink();
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
        void _invalidateOldComposition();
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

        CursorOptions _currentCursorOptions{};
        TimerHandle _cursorBlinker;
        uint64_t _cursorBufferMutationId = 0;
        uint64_t _cursorCursorMutationId = 0; // Stupid name, but it's _cursor related and stores the cursor mutation id.
        til::enumset<InhibitionSource, uint8_t> _cursorVisibilityInhibitors;
        til::enumset<InhibitionSource, uint8_t> _cursorBlinkingInhibitors;
        bool _cursorBlinkerOn = false;

        TimerHandle _renditionBlinker;

        Microsoft::Console::Types::Viewport _viewport;
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
