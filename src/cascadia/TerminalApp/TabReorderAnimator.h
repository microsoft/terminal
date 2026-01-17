// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace winrt::TerminalApp::implementation
{
    class TabReorderAnimator
    {
    public:
        TabReorderAnimator(const Microsoft::UI::Xaml::Controls::TabView& tabView, bool animationsEnabled);

        void OnDragStarting(uint32_t draggedTabIndex);
        void OnDragOver(const Windows::UI::Xaml::DragEventArgs& e);
        void OnDragCompleted();
        void OnDragLeave();

        void SetAnimationsEnabled(bool enabled);

    private:
        void _AnimateTabsToMakeGap(int gapIndex);
        void _ResetAllTransforms(bool animated);
        int _CalculateGapIndex(double pointerX) const;
        void _EnsureTransformsSetup();
        double _GetTabWidth() const;
        void _AnimateTransformTo(const Windows::UI::Xaml::Media::TranslateTransform& transform, double targetX);
        void _StopAllAnimations();
        void _DisableBuiltInTransitions();
        void _RestoreBuiltInTransitions();

        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        int _draggedTabIndex{ -1 };
        int _currentGapIndex{ -1 };
        std::vector<Windows::UI::Xaml::Media::TranslateTransform> _transforms;
        std::vector<Windows::UI::Xaml::Media::Animation::Storyboard> _activeAnimations;
        bool _animationsEnabled{ true };
        bool _isDragging{ false };

        Windows::UI::Xaml::Media::Animation::TransitionCollection _savedTransitions{ nullptr };
        bool _transitionsSaved{ false };
    };
}
