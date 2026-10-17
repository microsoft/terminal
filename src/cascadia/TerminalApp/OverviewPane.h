// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <limits>
#include <optional>

#include "OverviewPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct OverviewPane : OverviewPaneT<OverviewPane>
    {
        OverviewPane();
        ~OverviewPane();

        void UpdateTabContent(Windows::Foundation::Collections::IVector<TerminalApp::Tab> tabs, int32_t focusedIndex);
        void ClearTabContent();

        int32_t SelectedIndex() const;
        void SelectedIndex(int32_t value);

        bool UseMica() const;
        void UseMica(bool value);

        hstring ControlName() const;

        // Events
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IReference<int32_t>> TabSelected;
        til::typed_event<> Dismissed;

    private:
        friend struct OverviewPaneT<OverviewPane>; // for Xaml to bind events

        void _OnKeyDown(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _OnPreviewKeyDown(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _OnItemClicked(int32_t index);
        void _UpdateSelection();
        void _PlayEnterAnimation();
        void _PlayExitAnimation(std::function<void()> onComplete = nullptr);
        void _StartEnterZoomAnimation();
        std::optional<std::tuple<double, double, double>> _GetZoomParamsForCell(int32_t index);
        Windows::UI::Xaml::FrameworkElement _BuildPreviewCell(const TerminalApp::Tab& tab, int32_t index, int32_t totalCount, double previewWidth, double previewHeight, double referenceWidth, double referenceHeight);
        void _DetachContent(const Windows::UI::Xaml::FrameworkElement& content);

        struct AdaptiveLayout
        {
            double cellWidth{ 0.0 };
            double cellHeight{ 0.0 };
            double previewWidth{ 0.0 };
            double previewHeight{ 0.0 };
            int32_t columnCount{ 1 };
        };
        AdaptiveLayout _ComputeAdaptiveLayout(int32_t tabCount, double viewportWidth, double viewportHeight, double aspect) const;
        void _ApplyAdaptiveLayout();
        static void _AddDoubleAnimation(
            const Windows::UI::Xaml::Media::Animation::Storyboard& storyboard,
            const Windows::UI::Xaml::Media::CompositeTransform& target,
            const hstring& property,
            double from,
            double to,
            const Windows::UI::Xaml::Duration& duration,
            const Windows::UI::Xaml::Media::Animation::EasingFunctionBase& easing);

        int32_t _selectedIndex{ 0 };
        int32_t _columnCount{ 1 }; // updated at UpdateTabContent time to match the adaptive layout
        bool _pendingEnterAnimation{ false };
        bool _pendingAdaptiveLayout{ false };
        // Homogeneous-aspect assumption: every cell uses the active tab's
        // content aspect ratio. Per-tab aspect (e.g. one tab with a Settings
        // page that's narrow + tall, another with a TermControl that's wide)
        // would require a different layout pass — cells could no longer
        // share a single ItemWidth/ItemHeight on the WrapGrid.
        double _referenceAspect{ 16.0 / 10.0 };
        double _lastAppliedViewportWidth{ 0.0 };
        double _lastAppliedViewportHeight{ 0.0 };
        bool _useMica{ false };
        void _UpdateBackgroundForMica();
        winrt::event_token _exitAnimationToken{};
        Windows::UI::Xaml::Media::Animation::Storyboard _exitContentStoryboard{ nullptr };

        // Subscriptions on our own owned children. Stored so the destructor
        // can explicitly revoke them before ClearTabContent runs — matches
        // the token-revoke pattern used in TerminalPage for overview events.
        winrt::event_token _layoutUpdatedToken{};
        winrt::event_token _sizeChangedToken{};

        // Per-cell state for the adaptive layout pass. INVARIANT: there is
        // exactly one ReparentedEntry per cell in PreviewGrid().Items(), in
        // cell order. When the source tab had no live content,
        // `content` / `previewCanvas` are null and `layoutWidth` /
        // `layoutHeight` are zero — the cell still gets a previewBorder
        // + titleBlock sized by _ApplyAdaptiveLayout, but the
        // ScaleTransform / reparent path is skipped. _ApplyAdaptiveLayout
        // and ClearTabContent both null-check before touching content.
        struct ReparentedEntry
        {
            Windows::UI::Xaml::FrameworkElement content{ nullptr };
            Windows::UI::Xaml::Controls::Panel originalParent{ nullptr };
            double originalWidth{ std::numeric_limits<double>::quiet_NaN() };
            double originalHeight{ std::numeric_limits<double>::quiet_NaN() };
            Windows::UI::Xaml::Media::Transform originalRenderTransform{ nullptr };
            Windows::Foundation::Point originalRenderTransformOrigin{ 0.0f, 0.0f };
            // Per-cell handles for adaptive resizing after the first layout pass.
            Windows::UI::Xaml::Controls::Border previewBorder{ nullptr };
            Windows::UI::Xaml::Controls::TextBlock titleBlock{ nullptr };
            Windows::UI::Xaml::Controls::Canvas previewCanvas{ nullptr };
            double layoutWidth{ 0.0 };
            double layoutHeight{ 0.0 };
        };
        std::vector<ReparentedEntry> _reparentedContent;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(OverviewPane);
}
