// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "OverviewPane.h"

#include "OverviewPane.g.cpp"

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Media::Animation;
using namespace winrt::Windows::System;

// Lifecycle of an OverviewPane show/hide cycle:
//
//   1. TerminalPage calls UpdateTabContent(tabs, focusedIndex). This runs
//      BEFORE the overlay's Visibility flips to Visible, so ContentWrapper
//      has no measured size yet (ActualWidth() == 0). Cells are built with
//      MinCellWidth/MinCellHeight ("floor") sizes; _pendingAdaptiveLayout
//      is set so the real layout runs later.
//   2. TerminalPage flips overlay Visibility to Visible. XAML measures the
//      tree and raises LayoutUpdated on PreviewGrid.
//   3. First LayoutUpdated: ContentWrapper now has a real size. The handler
//      drains _pendingAdaptiveLayout by calling _ApplyAdaptiveLayout(),
//      which sizes every preview cell from the actual viewport.
//   4. Next LayoutUpdated: the first cell has a non-zero ActualWidth. The
//      handler drains _pendingEnterAnimation by calling
//      _StartEnterZoomAnimation(), which kicks off the zoom-out + fade-in.
//   5. Steady state: ContentWrapper.SizeChanged is the resize hook —
//      window resize re-runs _ApplyAdaptiveLayout (debounced to >=1px).
//      LayoutUpdated fires too often to use as the resize signal.
//   6. Exit (Enter / Escape / outside click): _PlayExitAnimation runs the
//      zoom-into-cell + fade-out, then ClearTabContent reparents the live
//      tab content back to its original parent. Destructor also calls
//      ClearTabContent and tolerates partially-disposed visual trees.

namespace winrt::TerminalApp::implementation
{
    // Floor — the preview rectangle never goes below this. Matches the
    // original fixed cell size.
    static constexpr double MinCellWidth = 320.0;
    static constexpr double MinCellHeight = 220.0;
    // Margin (6px each side) baked into the outerBorder. The WrapGrid slot
    // (ItemWidth/ItemHeight) needs to include this so cells don't overlap.
    static constexpr double CellMargin = 12.0;
    // ItemsControl.Margin (12 each side) — small, constant gutter around
    // the whole grid regardless of window size.
    static constexpr double GridOuterMargin = 24.0;
    // Hard cap on columns regardless of how wide the window is.
    static constexpr int32_t MaxColumns = 4;
    // Fallback aspect ratio (W/H) used when the active tab hasn't reported a
    // valid size yet. Roughly matches a typical terminal window.
    static constexpr double FallbackAspect = 16.0 / 10.0;

    static constexpr std::chrono::milliseconds EnterAnimDuration{ 400 };
    // Must match the BackgroundFadeOut storyboard duration in OverviewPane.xaml
    // (currently 0:0:0.15). The Completed handler fires on the content storyboard
    // and drives _DismissOverviewVisuals — if this is shorter, the background
    // overlay gets yanked mid-fade and snaps visibly. Curves stay distinct
    // (CubicEaseIn on content, QuadEaseIn on background); only the duration is
    // coupled.
    static constexpr std::chrono::milliseconds ExitAnimDuration{ 150 };

    OverviewPane::OverviewPane()
    {
        InitializeComponent();

        // Apply the initial background (opaque by default, since _useMica
        // starts false). Without this the XAML-declared brush stays in place
        // until someone explicitly calls UseMica().
        _UpdateBackgroundForMica();

        // Listen for layout passes so we can start the zoom-in animation
        // once the WrapGrid has measured and positioned its children.
        _layoutUpdatedToken = PreviewGrid().LayoutUpdated([weakThis = get_weak()](auto&&, auto&&) {
            if (auto self = weakThis.get())
            {
                if (!self->_pendingEnterAnimation && !self->_pendingAdaptiveLayout)
                {
                    return;
                }

                // We need ContentWrapper to have a real size before we can
                // do anything useful — both the adaptive layout and the
                // enter animation depend on it.
                auto wrapper = self->ContentWrapper();
                if (wrapper.ActualWidth() <= 0 || wrapper.ActualHeight() <= 0)
                {
                    return;
                }

                if (self->_pendingAdaptiveLayout)
                {
                    self->_pendingAdaptiveLayout = false;
                    self->_ApplyAdaptiveLayout();
                }

                if (!self->_pendingEnterAnimation)
                {
                    return;
                }

                // Check that at least the first cell is laid out
                auto items = self->PreviewGrid().Items();
                if (items.Size() == 0)
                {
                    return;
                }
                auto first = items.GetAt(0).try_as<FrameworkElement>();
                if (!first || first.ActualWidth() <= 0)
                {
                    return;
                }

                self->_pendingEnterAnimation = false;
                self->_StartEnterZoomAnimation();
            }
        });

        // Window resize → re-run the adaptive layout. This is what makes the
        // overview actually responsive (not just the WrapGrid passively
        // reflowing already-sized cells), and self-heals from bad initial
        // measurements (e.g. when opened on a window whose ContentWrapper
        // hadn't been laid out yet).
        _sizeChangedToken = ContentWrapper().SizeChanged([weakThis = get_weak()](auto&&, auto&&) {
            if (auto self = weakThis.get())
            {
                if (self->_reparentedContent.empty())
                {
                    return;
                }
                auto wrapper = self->ContentWrapper();
                const auto w = wrapper.ActualWidth();
                const auto h = wrapper.ActualHeight();
                if (w <= 0 || h <= 0)
                {
                    return;
                }
                // Skip no-op size changes (SizeChanged can coalesce-fire during
                // animations and these constants are pixel-precise).
                if (std::abs(w - self->_lastAppliedViewportWidth) < 1.0 &&
                    std::abs(h - self->_lastAppliedViewportHeight) < 1.0)
                {
                    return;
                }
                self->_ApplyAdaptiveLayout();
            }
        });
    }

    OverviewPane::~OverviewPane()
    {
        // Revoke our own subscriptions on owned children before teardown.
        // The publishers (PreviewGrid, ContentWrapper) are owned by this
        // control so they'd die with us anyway, but explicit revoke matches
        // TerminalPage's overview token pattern and future-proofs against
        // either publisher's lifetime being extended past ours. Best-effort
        // for the same reason ClearTabContent is — XAML named-element
        // access can throw during partial visual-tree disposal.
        try
        {
            PreviewGrid().LayoutUpdated(_layoutUpdatedToken);
        }
        CATCH_LOG()
        try
        {
            ContentWrapper().SizeChanged(_sizeChangedToken);
        }
        CATCH_LOG()

        ClearTabContent();
    }

    void OverviewPane::UpdateTabContent(Windows::Foundation::Collections::IVector<TerminalApp::Tab> tabs, int32_t focusedIndex)
    {
        // Clear any previous state
        ClearTabContent();

        if (!tabs || tabs.Size() == 0)
        {
            return;
        }

        const auto itemsControl = PreviewGrid();

        // Determine a reference size from the currently visible tab's content.
        // Inactive tabs have zero ActualWidth/Height since they're not laid out
        // in the visual tree, but all tabs share the same content area, so the
        // active tab's size is the right reference for all of them.
        double referenceWidth = 0;
        double referenceHeight = 0;
        if (focusedIndex >= 0 && focusedIndex < static_cast<int32_t>(tabs.Size()))
        {
            auto focusedContent = tabs.GetAt(static_cast<uint32_t>(focusedIndex)).Content();
            if (focusedContent)
            {
                referenceWidth = focusedContent.ActualWidth();
                referenceHeight = focusedContent.ActualHeight();
            }
        }

        // Pick column count and per-cell size based on how much room we have.
        // ContentWrapper is the area we render into. NOTE: when the overview
        // is being shown for the first time, the wrapper hasn't been laid
        // out yet — we'll get ActualWidth() == 0 here. In that case we
        // build cells with floor sizes and apply the real adaptive layout
        // from the LayoutUpdated handler once XAML has measured.
        const auto wrapper = ContentWrapper();
        const auto aspect = (referenceWidth > 0 && referenceHeight > 0)
                                ? referenceWidth / referenceHeight
                                : FallbackAspect;
        _referenceAspect = aspect;
        const auto layout = _ComputeAdaptiveLayout(static_cast<int32_t>(tabs.Size()),
                                                   wrapper.ActualWidth(),
                                                   wrapper.ActualHeight(),
                                                   aspect);

        _columnCount = layout.columnCount;

        for (uint32_t i = 0; i < tabs.Size(); i++)
        {
            const auto& tab = tabs.GetAt(i);
            auto cell = _BuildPreviewCell(tab, static_cast<int32_t>(i), static_cast<int32_t>(tabs.Size()), layout.previewWidth, layout.previewHeight, referenceWidth, referenceHeight);
            itemsControl.Items().Append(cell);
        }

        // Push the computed cell size and column cap into the WrapGrid. The
        // ItemsPanelRoot is materialized lazily (on first measure or first
        // item add), so this runs AFTER appending to guarantee it exists.
        // If it isn't materialized yet, force a layout pass first.
        if (!itemsControl.ItemsPanelRoot())
        {
            itemsControl.UpdateLayout();
        }
        if (auto wrapGrid = itemsControl.ItemsPanelRoot().try_as<winrt::Windows::UI::Xaml::Controls::WrapGrid>())
        {
            wrapGrid.ItemWidth(layout.cellWidth + CellMargin);
            wrapGrid.ItemHeight(layout.cellHeight + CellMargin);
            wrapGrid.MaximumRowsOrColumns(layout.columnCount);
            wrapGrid.InvalidateMeasure();
        }

        // If we couldn't compute a real layout (overview not yet sized), defer
        // the actual sizing to the LayoutUpdated handler.
        _pendingAdaptiveLayout = wrapper.ActualWidth() <= 0 || wrapper.ActualHeight() <= 0;

        _selectedIndex = focusedIndex;
        _UpdateSelection();
        _PlayEnterAnimation();

        // Focus self for keyboard input
        Focus(FocusState::Programmatic);
    }

    OverviewPane::AdaptiveLayout OverviewPane::_ComputeAdaptiveLayout(int32_t tabCount, double viewportWidth, double viewportHeight, double aspect) const
    {
        // Vertical chrome BELOW the preview rect: title text (~22) + 6 top
        // margin + outerBorder padding/border (4+2 each side = 12 total) +
        // a few px of cushion so the StackPanel doesn't get squished. Top
        // of the cell pays 6 chrome too.
        constexpr double cellChromeV = 44.0;
        // Horizontal chrome (outerBorder padding/border, 6 each side).
        constexpr double cellChromeH = 12.0;

        AdaptiveLayout layout{};
        layout.columnCount = 1;
        layout.cellWidth = MinCellWidth + cellChromeH;
        layout.cellHeight = MinCellHeight + cellChromeV;
        layout.previewWidth = MinCellWidth;
        layout.previewHeight = MinCellHeight;

        if (tabCount <= 0 || aspect <= 0)
        {
            return layout;
        }

        const auto availW = std::max(0.0, viewportWidth - GridOuterMargin);
        const auto availH = std::max(0.0, viewportHeight - GridOuterMargin);

        // Try every column count from 1 to min(N, MaxColumns) and score each
        // arrangement. Scoring: total preview area (per-cell area × tab
        // count), with a strong bonus for layouts that fit the entire grid
        // inside the viewport (no scrolling) and a smaller bonus for grids
        // that fill more of the vertical space. This is the "alt-tab feel"
        // — big previews AND many visible without scroll.
        const auto maxCols = std::min(MaxColumns, tabCount);
        double bestScore = -1.0;

        for (int32_t cols = 1; cols <= maxCols; ++cols)
        {
            const auto rows = (tabCount + cols - 1) / cols;

            // Width-driven candidate
            const auto cellSlotW = (availW / cols) - CellMargin;
            const auto previewW_fromW = cellSlotW - cellChromeH;
            const auto previewH_fromW = previewW_fromW / aspect;

            // Height-driven candidate
            const auto cellSlotH = (availH / rows) - CellMargin;
            const auto previewH_fromH = cellSlotH - cellChromeV;
            const auto previewW_fromH = previewH_fromH * aspect;

            // Aspect-locked: the preview rect must fit both constraints —
            // pick the smaller of the two candidates.
            double previewW;
            double previewH;
            if (previewH_fromW <= previewH_fromH)
            {
                previewW = previewW_fromW;
                previewH = previewH_fromW;
            }
            else
            {
                previewW = previewW_fromH;
                previewH = previewH_fromH;
            }

            // Enforce the floor.
            const auto belowFloor = previewW < MinCellWidth || previewH < MinCellHeight;
            previewW = std::max(previewW, MinCellWidth);
            previewH = std::max(previewH, MinCellHeight);

            const auto cellW = previewW + cellChromeH;
            const auto cellH = previewH + cellChromeV;
            const auto gridH = rows * (cellH + CellMargin);
            const auto fitsVertically = gridH <= availH;

            // Score: total preview area covered. Reward layouts that fit
            // without scrolling. Reward layouts that fill more vertical
            // space (encourages fewer-columns/more-rows when there's room).
            const auto totalArea = previewW * previewH * tabCount;
            const auto fitBonus = fitsVertically ? 1.25 : 0.7;
            const auto vfill = std::clamp(gridH / std::max(availH, 1.0), 0.0, 1.0);
            const auto fillBonus = 0.5 + 0.5 * vfill; // 0.5 ... 1.0
            const auto floorPenalty = belowFloor ? 0.5 : 1.0;
            const auto score = totalArea * fitBonus * fillBonus * floorPenalty;

            if (score > bestScore)
            {
                bestScore = score;
                layout.previewWidth = previewW;
                layout.previewHeight = previewH;
                layout.cellWidth = cellW;
                layout.cellHeight = cellH;
                layout.columnCount = cols;
            }
        }

        return layout;
    }

    // Re-runs the adaptive layout calc once ContentWrapper has a real size,
    // and applies the result to every already-built cell + the WrapGrid.
    // This is the path that fires on the first LayoutUpdated after the
    // overview becomes visible (since UpdateTabContent is called BEFORE
    // visibility flips, ActualWidth is 0 there).
    void OverviewPane::_ApplyAdaptiveLayout()
    {
        const auto wrapper = ContentWrapper();
        const auto vpW = wrapper.ActualWidth();
        const auto vpH = wrapper.ActualHeight();
        if (vpW <= 0 || vpH <= 0)
        {
            return;
        }

        const auto tabCount = static_cast<int32_t>(_reparentedContent.size());
        if (tabCount == 0)
        {
            return;
        }

        const auto layout = _ComputeAdaptiveLayout(tabCount, vpW, vpH, _referenceAspect);
        _columnCount = layout.columnCount;
        _lastAppliedViewportWidth = vpW;
        _lastAppliedViewportHeight = vpH;

        // Resize each preview rect + rescale the reparented tab content to
        // match. The tabContent is locked to its layoutWidth/layoutHeight; we
        // recompute the ScaleTransform to fit the new preview rect.
        for (auto& entry : _reparentedContent)
        {
            if (entry.previewBorder)
            {
                entry.previewBorder.Width(layout.previewWidth);
                entry.previewBorder.Height(layout.previewHeight);
            }
            if (entry.previewCanvas)
            {
                entry.previewCanvas.Width(layout.previewWidth);
                entry.previewCanvas.Height(layout.previewHeight);
                // Update clip to new preview bounds.
                Windows::UI::Xaml::Media::RectangleGeometry clipGeometry;
                clipGeometry.Rect({ 0, 0, static_cast<float>(layout.previewWidth), static_cast<float>(layout.previewHeight) });
                entry.previewCanvas.Clip(clipGeometry);
            }
            if (entry.titleBlock)
            {
                entry.titleBlock.MaxWidth(layout.previewWidth);
            }
            if (entry.content && entry.layoutWidth > 0 && entry.layoutHeight > 0)
            {
                const auto scale = std::min(layout.previewWidth / entry.layoutWidth,
                                            layout.previewHeight / entry.layoutHeight);
                if (auto st = entry.content.RenderTransform().try_as<Windows::UI::Xaml::Media::ScaleTransform>())
                {
                    st.ScaleX(scale);
                    st.ScaleY(scale);
                }
            }
        }

        // Apply WrapGrid sizing.
        const auto itemsControl = PreviewGrid();
        if (auto wrapGrid = itemsControl.ItemsPanelRoot().try_as<winrt::Windows::UI::Xaml::Controls::WrapGrid>())
        {
            wrapGrid.ItemWidth(layout.cellWidth + CellMargin);
            wrapGrid.ItemHeight(layout.cellHeight + CellMargin);
            wrapGrid.MaximumRowsOrColumns(layout.columnCount);
            wrapGrid.InvalidateMeasure();
        }
    }

    void OverviewPane::ClearTabContent()
    {
        _pendingEnterAnimation = false;
        _pendingAdaptiveLayout = false;
        _lastAppliedViewportWidth = 0.0;
        _lastAppliedViewportHeight = 0.0;

        // Stop any running exit animation. Wrapped because ClearTabContent
        // is destructor-reachable and storyboard / named-element access can
        // throw winrt::hresult_error during partial visual-tree disposal.
        // (See 2026-05-20 destructor-safety rule.)
        try
        {
            if (_exitContentStoryboard)
            {
                _exitContentStoryboard.Completed(_exitAnimationToken);
                _exitAnimationToken = {};
                _exitContentStoryboard.Stop();
                _exitContentStoryboard = nullptr;
            }
        }
        CATCH_LOG()

        // Reset the zoom transform to identity. Named-element access
        // (ContentTransform / ContentWrapper) is unsafe during teardown for
        // the same reason — guard the whole block.
        try
        {
            auto transform = ContentTransform();
            transform.ScaleX(1.0);
            transform.ScaleY(1.0);
            transform.TranslateX(0.0);
            transform.TranslateY(0.0);
            ContentWrapper().Opacity(1.0);
        }
        CATCH_LOG()

        // Reparent content back to original parents. This can fail when the
        // window is being torn down — the originalParent may already be in
        // a disposed/disconnected state. Make it best-effort; the destructor
        // calls us and we must not throw.
        size_t reparentIdx = 0;
        for (auto& entry : _reparentedContent)
        {
            if (!entry.content)
            {
                continue;
            }
            try
            {
                // Restore original Width/Height (NaN = auto-sizing)
                entry.content.Width(entry.originalWidth);
                entry.content.Height(entry.originalHeight);

                // Restore original RenderTransform and origin
                entry.content.RenderTransform(entry.originalRenderTransform);
                entry.content.RenderTransformOrigin(entry.originalRenderTransformOrigin);

                _DetachContent(entry.content);

                const bool hadOrigParent = static_cast<bool>(entry.originalParent);
                // Put it back where it came from
                if (entry.originalParent)
                {
                    entry.originalParent.Children().Append(entry.content);
                }
                const bool stillParented = static_cast<bool>(VisualTreeHelper::GetParent(entry.content));
            }
            CATCH_LOG()
        }
        _reparentedContent.clear();

        try
        {
            const auto itemsControl = PreviewGrid();
            itemsControl.Items().Clear();
        }
        CATCH_LOG()
    }

    int32_t OverviewPane::SelectedIndex() const
    {
        return _selectedIndex;
    }

    void OverviewPane::SelectedIndex(int32_t value)
    {
        if (_selectedIndex != value)
        {
            _selectedIndex = value;
            _UpdateSelection();
        }
    }

    bool OverviewPane::UseMica() const
    {
        return _useMica;
    }

    void OverviewPane::UseMica(bool value)
    {
        if (_useMica != value)
        {
            _useMica = value;
            _UpdateBackgroundForMica();
        }
    }

    // Localized name for assistive tech (Narrator). Bound from XAML via
    // AutomationProperties.Name="{x:Bind ControlName, Mode=OneWay}" on the
    // UserControl root. Same pattern as CommandPalette::ControlName.
    hstring OverviewPane::ControlName() const
    {
        return RS_(L"OverviewPaneControlName");
    }

    void OverviewPane::_UpdateBackgroundForMica()
    {
        // TODO (Ripley, 2026-05-20 review): The dual-background code path
        // here exists to support a transparent overlay when the host window
        // has Mica enabled (so the Mica backdrop shows through). It IS
        // wired up — TerminalPage::_EnterOverview calls
        // overview.UseMica(theme.Window().UseMica()) — but the opaque
        // branch's SolidBackgroundFillColorBaseBrush is the same brush the
        // XAML default (SmokeFillColorDefaultBrush) would produce, which
        // makes the BackgroundOverlay XAML attribute look like a third
        // source of truth. Pre-merge cleanup pass deferred until Mica
        // verification on a real Mica-enabled window — do NOT delete this
        // method without confirming both visual paths look correct first.
        auto overlay = BackgroundOverlay();
        if (_useMica)
        {
            // Transparent background — let the Mica backdrop show through
            overlay.Background(SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
        }
        else
        {
            // Opaque background when Mica is not active.
            // Use the theme-aware SolidBackgroundFillColorBaseBrush so we
            // match the correct color for light / dark theme.
            auto res = Application::Current().Resources();
            auto brush = res.TryLookup(winrt::box_value(L"SolidBackgroundFillColorBaseBrush"));
            if (brush)
            {
                overlay.Background(brush.as<Brush>());
            }
            else
            {
                overlay.Background(SolidColorBrush{ winrt::Windows::UI::ColorHelper::FromArgb(255, 32, 32, 32) });
            }
        }
    }

    // PreviewKeyDown runs during the *tunneling* phase, before KeyDown
    // bubbles. We need to claim our local keys here because TerminalPage's
    // global `_KeyDownHandler` is *also* hooked to PreviewKeyDown on this
    // UserControl (see TerminalPage.xaml's OverviewPaneElement). If we let
    // Enter (or anything with a global keybinding) tunnel through, the
    // global handler eats it, dismisses the overview, and dispatches the
    // bound action instead of selecting the highlighted cell.
    //
    // Anything we DON'T claim (e.g. Ctrl+Shift+O for ToggleOverview) still
    // bubbles to the global handler so other shortcuts keep working.
    void OverviewPane::_OnPreviewKeyDown(const IInspectable& sender, const KeyRoutedEventArgs& e)
    {
        const auto items = PreviewGrid().Items();
        const auto itemCount = static_cast<int32_t>(items.Size());
        if (itemCount == 0)
        {
            return;
        }

        const auto key = e.OriginalKey();
        switch (key)
        {
        case VirtualKey::Tab:
        {
            const auto shiftPressed = (Windows::UI::Core::CoreWindow::GetForCurrentThread().GetKeyState(VirtualKey::Shift) & Windows::UI::Core::CoreVirtualKeyStates::Down) == Windows::UI::Core::CoreVirtualKeyStates::Down;
            if (shiftPressed)
            {
                if (_selectedIndex > 0)
                {
                    _selectedIndex--;
                    _UpdateSelection();
                }
            }
            else
            {
                if (_selectedIndex < itemCount - 1)
                {
                    _selectedIndex++;
                    _UpdateSelection();
                }
            }
            e.Handled(true);
            return;
        }
        case VirtualKey::Left:
        case VirtualKey::Right:
        case VirtualKey::Up:
        case VirtualKey::Down:
        case VirtualKey::Enter:
        case VirtualKey::Escape:
            // Delegate to the same logic as KeyDown. _OnKeyDown sets
            // e.Handled itself for these keys, so the global PreviewKeyDown
            // handler will be skipped (XAML attribute handlers are
            // registered with handledEventsToo=false).
            _OnKeyDown(sender, e);
            return;
        default:
            // Let other keys bubble to the global handler.
            return;
        }
    }

    void OverviewPane::_OnKeyDown(const IInspectable& /*sender*/, const KeyRoutedEventArgs& e)
    {
        const auto items = PreviewGrid().Items();
        const auto itemCount = static_cast<int32_t>(items.Size());
        if (itemCount == 0)
        {
            return;
        }

        auto handled = true;
        switch (e.OriginalKey())
        {
        case VirtualKey::Left:
            if (_selectedIndex > 0)
            {
                _selectedIndex--;
                _UpdateSelection();
            }
            break;
        case VirtualKey::Right:
            if (_selectedIndex < itemCount - 1)
            {
                _selectedIndex++;
                _UpdateSelection();
            }
            break;
        case VirtualKey::Up:
            if (_selectedIndex - _columnCount >= 0)
            {
                _selectedIndex -= _columnCount;
                _UpdateSelection();
            }
            break;
        case VirtualKey::Down:
            if (_selectedIndex + _columnCount < itemCount)
            {
                _selectedIndex += _columnCount;
                _UpdateSelection();
            }
            break;
        case VirtualKey::Enter:
            _OnItemClicked(_selectedIndex);
            break;
        case VirtualKey::Escape:
            _PlayExitAnimation([weakThis = get_weak()]() {
                if (auto self = weakThis.get())
                {
                    self->Dismissed.raise(*self, nullptr);
                }
            });
            break;
        default:
            handled = false;
            break;
        }

        e.Handled(handled);
    }

    void OverviewPane::_OnItemClicked(int32_t index)
    {
        _PlayExitAnimation([weakThis = get_weak(), index]() {
            if (auto self = weakThis.get())
            {
                self->TabSelected.raise(*self, winrt::Windows::Foundation::IReference<int32_t>{ index });
            }
        });
    }

    void OverviewPane::_UpdateSelection()
    {
        const auto items = PreviewGrid().Items();
        const auto itemCount = static_cast<int32_t>(items.Size());

        // Clamp selection
        _selectedIndex = std::clamp(_selectedIndex, 0, std::max(0, itemCount - 1));

        for (int32_t i = 0; i < itemCount; i++)
        {
            if (auto cellElement = items.GetAt(i).try_as<FrameworkElement>())
            {
                if (auto border = cellElement.try_as<Border>())
                {
                    if (i == _selectedIndex)
                    {
                        // Accent-colored border for selected item
                        const auto accentBrush = Application::Current()
                                                     .Resources()
                                                     .Lookup(winrt::box_value(L"SystemAccentColor"))
                                                     .as<winrt::Windows::UI::Color>();
                        border.BorderBrush(SolidColorBrush{ accentBrush });
                        border.BorderThickness(ThicknessHelper::FromUniformLength(2));

                        // Scroll into view if needed
                        border.StartBringIntoView();
                    }
                    else
                    {
                        border.BorderBrush(SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                        border.BorderThickness(ThicknessHelper::FromUniformLength(2));
                    }
                }
            }
        }
    }

    void OverviewPane::_PlayEnterAnimation()
    {
        // Hide the content wrapper until the LayoutUpdated callback fires
        // and we can read cell positions to set up the zoom transform.
        ContentWrapper().Opacity(0);
        _pendingEnterAnimation = true;
    }

    void OverviewPane::_StartEnterZoomAnimation()
    {
        // Start the background fade-in together with the zoom so both
        // animations are visible at the same moment (avoids opacity flash).
        if (auto bgSb = Resources().Lookup(winrt::box_value(L"BackgroundFadeIn")).try_as<Storyboard>())
        {
            bgSb.Begin();
        }

        auto wrapper = ContentWrapper();
        auto transform = ContentTransform();

        auto zoomParams = _GetZoomParamsForCell(_selectedIndex);
        if (!zoomParams)
        {
            wrapper.Opacity(1.0);
            return;
        }
        auto [scale, tx, ty] = *zoomParams;

        // Set the initial transform so the focused cell fills the viewport
        transform.ScaleX(scale);
        transform.ScaleY(scale);
        transform.TranslateX(tx);
        transform.TranslateY(ty);
        wrapper.Opacity(1.0);

        // Animate from the zoomed-in state to identity (zoom out to grid)
        Storyboard storyboard;
        const auto duration = DurationHelper::FromTimeSpan(EnterAnimDuration);
        CubicEase easing;
        easing.EasingMode(EasingMode::EaseOut);

        _AddDoubleAnimation(storyboard, transform, L"ScaleX", scale, 1.0, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"ScaleY", scale, 1.0, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"TranslateX", tx, 0.0, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"TranslateY", ty, 0.0, duration, easing);

        storyboard.Begin();
    }

    void OverviewPane::_PlayExitAnimation(std::function<void()> onComplete)
    {
        // Fade out the background overlay
        if (auto bgSb = Resources().Lookup(winrt::box_value(L"BackgroundFadeOut")).try_as<Storyboard>())
        {
            bgSb.Begin();
        }

        auto zoomParams = _GetZoomParamsForCell(_selectedIndex);
        if (!zoomParams)
        {
            if (onComplete)
            {
                onComplete();
            }
            return;
        }
        auto [scale, tx, ty] = *zoomParams;

        // Animate from the current grid view into the selected cell
        auto transform = ContentTransform();
        Storyboard storyboard;
        const auto duration = DurationHelper::FromTimeSpan(ExitAnimDuration);
        CubicEase easing;
        easing.EasingMode(EasingMode::EaseIn);

        _AddDoubleAnimation(storyboard, transform, L"ScaleX", 1.0, scale, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"ScaleY", 1.0, scale, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"TranslateX", 0.0, tx, duration, easing);
        _AddDoubleAnimation(storyboard, transform, L"TranslateY", 0.0, ty, duration, easing);

        // Revoke any previously registered Completed handler AND stop the
        // prior storyboard. Just revoking the Completed token leaves the
        // old storyboard actively mutating ContentTransform — on rapid
        // clicks, two storyboards would fight over the same target for the
        // ExitAnimDuration window. Stop() is wrapped because storyboard
        // state can be invalid during shutdown.
        if (_exitContentStoryboard)
        {
            _exitContentStoryboard.Completed(_exitAnimationToken);
            try
            {
                _exitContentStoryboard.Stop();
            }
            CATCH_LOG()
        }
        _exitContentStoryboard = storyboard;
        // This Completed lambda raises Dismissed / TabSelected, whose
        // subscriber (TerminalPage::_DismissOverviewVisuals) synchronously
        // calls ClearTabContent — which revokes _exitAnimationToken and
        // nulls _exitContentStoryboard while we're still inside it. This is
        // safe because WinRT's delegate-dispatch path holds a strong ref to
        // the in-flight delegate (and therefore this lambda's captures) for
        // the duration of the invoke; the stack frame survives until
        // dispatch returns.
        _exitAnimationToken = storyboard.Completed([weakThis = get_weak(), onComplete](auto&&, auto&&) {
            if (auto self = weakThis.get())
            {
                if (onComplete)
                {
                    onComplete();
                }
            }
        });

        storyboard.Begin();
    }

    std::optional<std::tuple<double, double, double>> OverviewPane::_GetZoomParamsForCell(int32_t index)
    {
        auto wrapper = ContentWrapper();
        const auto vpW = wrapper.ActualWidth();
        const auto vpH = wrapper.ActualHeight();

        if (vpW <= 0 || vpH <= 0)
        {
            return std::nullopt;
        }

        auto items = PreviewGrid().Items();
        if (index < 0 || index >= static_cast<int32_t>(items.Size()))
        {
            return std::nullopt;
        }

        auto cell = items.GetAt(static_cast<uint32_t>(index)).try_as<FrameworkElement>();
        if (!cell || cell.ActualWidth() <= 0 || cell.ActualHeight() <= 0)
        {
            return std::nullopt;
        }

        const auto cellW = cell.ActualWidth();
        const auto cellH = cell.ActualHeight();

        // Cell center relative to ContentWrapper (accounts for scroll offset)
        auto cellTransform = cell.TransformToVisual(wrapper);
        auto topLeft = cellTransform.TransformPoint({ 0.0f, 0.0f });
        const auto cellCX = static_cast<double>(topLeft.X) + cellW / 2.0;
        const auto cellCY = static_cast<double>(topLeft.Y) + cellH / 2.0;

        // Scale so the cell fits the viewport
        const auto scale = std::min(vpW / cellW, vpH / cellH);

        // With RenderTransformOrigin={0.5,0.5} the scale origin is the
        // center of ContentWrapper. Translate so the cell center lands
        // at the viewport center after scaling.
        const auto translateX = (vpW / 2.0 - cellCX) * scale;
        const auto translateY = (vpH / 2.0 - cellCY) * scale;

        return std::tuple{ scale, translateX, translateY };
    }

    FrameworkElement OverviewPane::_BuildPreviewCell(const TerminalApp::Tab& tab, int32_t index, int32_t totalCount, double previewWidth, double previewHeight, double referenceWidth, double referenceHeight)
    {
        // Outer border — serves as the selection indicator. Stretch so it
        // fills its WrapGrid slot (otherwise the WrapGrid centers it at
        // content size and we get visible dead space around each cell).
        Border outerBorder;
        outerBorder.HorizontalAlignment(HorizontalAlignment::Stretch);
        outerBorder.VerticalAlignment(VerticalAlignment::Stretch);
        outerBorder.BorderBrush(SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
        outerBorder.BorderThickness(ThicknessHelper::FromUniformLength(2));
        outerBorder.CornerRadius({ 8, 8, 8, 8 });
        outerBorder.Padding(ThicknessHelper::FromUniformLength(4));
        outerBorder.Margin(ThicknessHelper::FromUniformLength(6));

        // Vertical stack: preview box + title
        StackPanel cellStack;
        cellStack.Orientation(Orientation::Vertical);
        cellStack.HorizontalAlignment(HorizontalAlignment::Center);
        cellStack.VerticalAlignment(VerticalAlignment::Center);

        // Preview container with a dark background. Sized to the precomputed
        // aspect-fitted preview rect (already accounts for the cell slot and
        // the active tab's aspect ratio).
        Border previewBorder;
        previewBorder.Width(previewWidth);
        previewBorder.Height(previewHeight);
        previewBorder.CornerRadius({ 6, 6, 6, 6 });
        auto bgBrush = Application::Current().Resources().TryLookup(winrt::box_value(L"SystemControlBackgroundChromeMediumBrush"));
        if (bgBrush)
        {
            previewBorder.Background(bgBrush.as<Brush>());
        }
        else
        {
            previewBorder.Background(SolidColorBrush{ winrt::Windows::UI::ColorHelper::FromArgb(255, 30, 30, 30) });
        }

        // Get the tab's content and reparent it with a ScaleTransform.
        // Locals below are gathered for the ReparentedEntry constructed at
        // the bottom of this function — we push exactly one entry per cell,
        // even when the tab has no live content, so _reparentedContent stays
        // index-aligned with PreviewGrid().Items().
        Canvas previewCanvas{ nullptr };
        Windows::UI::Xaml::FrameworkElement entryContent{ nullptr };
        Windows::UI::Xaml::Controls::Panel entryOriginalParent{ nullptr };
        double entryOriginalWidth = std::numeric_limits<double>::quiet_NaN();
        double entryOriginalHeight = std::numeric_limits<double>::quiet_NaN();
        Windows::UI::Xaml::Media::Transform entryOriginalRenderTransform{ nullptr };
        Windows::Foundation::Point entryOriginalRenderTransformOrigin{ 0.0f, 0.0f };
        double layoutWidth = 0.0;
        double layoutHeight = 0.0;

        auto tabContent = tab.Content();
        if (tabContent)
        {
            // Save the Width/Height *property* values (likely NaN for auto-sized
            // elements). These differ from ActualWidth/Height (the rendered size).
            // We need the property values to restore auto-sizing on exit.
            entryOriginalWidth = tabContent.Width();
            entryOriginalHeight = tabContent.Height();
            entryOriginalRenderTransform = tabContent.RenderTransform();
            entryOriginalRenderTransformOrigin = tabContent.RenderTransformOrigin();

            // Use ActualWidth/Height if the content is currently laid out (active tab).
            // Inactive tabs aren't in the visual tree and report zero — fall back
            // to the reference size from the active tab's content area.
            layoutWidth = tabContent.ActualWidth();
            layoutHeight = tabContent.ActualHeight();
            if (layoutWidth <= 0 || layoutHeight <= 0)
            {
                layoutWidth = referenceWidth;
                layoutHeight = referenceHeight;
            }

            entryOriginalParent = VisualTreeHelper::GetParent(tabContent).try_as<Panel>();

            // XAML single-parent rule: remove from current parent first
            _DetachContent(tabContent);

            // Lock the content to the layout size — this prevents
            // TermControl from seeing a resize and reflowing its buffer
            if (layoutWidth > 0 && layoutHeight > 0)
            {
                tabContent.Width(layoutWidth);
                tabContent.Height(layoutHeight);

                // Calculate uniform scale to fit in the aspect-fitted preview rect.
                const double scale = std::min(previewWidth / layoutWidth, previewHeight / layoutHeight);

                // RenderTransform is applied AFTER layout — the content still
                // thinks it's at its original size
                ScaleTransform scaleTransform;
                scaleTransform.ScaleX(scale);
                scaleTransform.ScaleY(scale);
                tabContent.RenderTransform(scaleTransform);
                tabContent.RenderTransformOrigin({ 0.0f, 0.0f });

                // Use a Canvas so the content is not constrained by the preview
                // container's layout. Canvas gives children infinite measure
                // space and arranges at desired size.
                Canvas canvas;
                canvas.Width(previewWidth);
                canvas.Height(previewHeight);

                // Clip to preview bounds so the scaled content doesn't overflow
                RectangleGeometry clipGeometry;
                clipGeometry.Rect({ 0, 0, static_cast<float>(previewWidth), static_cast<float>(previewHeight) });
                canvas.Clip(clipGeometry);

                canvas.Children().Append(tabContent);
                previewCanvas = canvas;

                // Layer the canvas behind a transparent overlay so
                // pointer events never reach the TermControl content.
                Grid previewGrid;
                previewGrid.Children().Append(canvas);

                Border inputOverlay;
                inputOverlay.Background(SolidColorBrush{ winrt::Windows::UI::Colors::Transparent() });
                previewGrid.Children().Append(inputOverlay);

                previewBorder.Child(previewGrid);
            }

            entryContent = tabContent;
        }

        cellStack.Children().Append(previewBorder);

        // Tab title text. Built before the ReparentedEntry so we can populate
        // every entry field in a single push_back below.
        TextBlock titleBlock;
        titleBlock.Text(tab.Title());
        titleBlock.FontSize(14);
        // Theme-aware foreground — Colors::White() would be invisible in
        // light mode. Mirror the same TryLookup pattern used by
        // _UpdateBackgroundForMica; fall back to white only if the resource
        // dictionary is missing the brush.
        {
            auto res = Application::Current().Resources();
            auto fgBrush = res.TryLookup(winrt::box_value(L"TextFillColorPrimaryBrush"));
            if (fgBrush)
            {
                titleBlock.Foreground(fgBrush.as<Brush>());
            }
            else
            {
                titleBlock.Foreground(SolidColorBrush{ winrt::Windows::UI::Colors::White() });
            }
        }
        titleBlock.HorizontalAlignment(HorizontalAlignment::Center);
        titleBlock.TextTrimming(TextTrimming::CharacterEllipsis);
        titleBlock.MaxWidth(previewWidth);
        titleBlock.Margin({ 0, 6, 0, 0 });
        // Hide the title from Narrator's content view — the per-cell
        // AutomationProperties.Name (set on outerBorder below) already
        // exposes the same string. Without this, Narrator reads the
        // title twice for every cell.
        Windows::UI::Xaml::Automation::AutomationProperties::SetAccessibilityView(
            titleBlock,
            Windows::UI::Xaml::Automation::Peers::AccessibilityView::Raw);

        cellStack.Children().Append(titleBlock);

        // One entry per cell, in cell order. content / previewCanvas /
        // layoutWidth / layoutHeight are null/zero when the tab had no live
        // content; _ApplyAdaptiveLayout and ClearTabContent both null-check
        // before touching those fields.
        ReparentedEntry entry{};
        entry.content = entryContent;
        entry.originalParent = entryOriginalParent;
        entry.originalWidth = entryOriginalWidth;
        entry.originalHeight = entryOriginalHeight;
        entry.originalRenderTransform = entryOriginalRenderTransform;
        entry.originalRenderTransformOrigin = entryOriginalRenderTransformOrigin;
        entry.previewBorder = previewBorder;
        entry.previewCanvas = previewCanvas;
        entry.titleBlock = titleBlock;
        entry.layoutWidth = layoutWidth;
        entry.layoutHeight = layoutHeight;
        _reparentedContent.push_back(std::move(entry));

        outerBorder.Child(cellStack);

        // Per-cell accessibility: Narrator says "<title>, N of M".
        // Set on outerBorder (not cellStack) because outerBorder is what
        // the WrapGrid surfaces as the focusable / pointer-target element.
        Windows::UI::Xaml::Automation::AutomationProperties::SetName(outerBorder, tab.Title());
        Windows::UI::Xaml::Automation::AutomationProperties::SetPositionInSet(outerBorder, index + 1);
        Windows::UI::Xaml::Automation::AutomationProperties::SetSizeOfSet(outerBorder, totalCount);

        // Click handler
        outerBorder.PointerPressed([weakThis = get_weak(), index](auto&&, auto&&) {
            if (auto self = weakThis.get())
            {
                self->_selectedIndex = index;
                self->_UpdateSelection();
                self->_OnItemClicked(index);
            }
        });

        // Hover effect
        outerBorder.PointerEntered([weakThis = get_weak(), index](auto&&, auto&&) {
            if (auto self = weakThis.get())
            {
                self->_selectedIndex = index;
                self->_UpdateSelection();
            }
        });

        return outerBorder;
    }

    void OverviewPane::_AddDoubleAnimation(
        const Storyboard& storyboard,
        const CompositeTransform& target,
        const hstring& property,
        double from,
        double to,
        const Duration& duration,
        const EasingFunctionBase& easing)
    {
        DoubleAnimation anim;
        anim.From(from);
        anim.To(to);
        anim.Duration(duration);
        anim.EasingFunction(easing);
        Storyboard::SetTarget(anim, target);
        Storyboard::SetTargetProperty(anim, property);
        storyboard.Children().Append(anim);
    }

    void OverviewPane::_DetachContent(const FrameworkElement& content)
    {
        // Try removing from various XAML container types
        if (auto parent = VisualTreeHelper::GetParent(content))
        {
            if (auto panel = parent.try_as<Panel>())
            {
                uint32_t idx;
                if (panel.Children().IndexOf(content, idx))
                {
                    panel.Children().RemoveAt(idx);
                }
            }
            else if (auto border = parent.try_as<Border>())
            {
                border.Child(nullptr);
            }
            else if (auto contentControl = parent.try_as<ContentControl>())
            {
                contentControl.Content(nullptr);
            }
        }
    }
}
