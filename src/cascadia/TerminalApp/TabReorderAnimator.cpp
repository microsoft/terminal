// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabReorderAnimator.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::UI::Xaml::Media::Animation;

namespace MUX = winrt::Microsoft::UI::Xaml::Controls;

static constexpr int AnimationDurationMs = 200;
static constexpr double DefaultTabWidthFallback = 200.0;

namespace winrt::TerminalApp::implementation
{
    TabReorderAnimator::TabReorderAnimator(const MUX::TabView& tabView, bool animationsEnabled) :
        _tabView{ tabView },
        _animationsEnabled{ animationsEnabled }
    {
    }

    void TabReorderAnimator::SetAnimationsEnabled(bool enabled)
    {
        _animationsEnabled = enabled;
    }

    void TabReorderAnimator::OnDragStarting(uint32_t draggedTabIndex)
    {
        _isDragging = true;
        _draggedTabIndex = static_cast<int>(draggedTabIndex);
        _currentGapIndex = _draggedTabIndex;

        _EnsureTransformsSetup();
        _DisableBuiltInTransitions();
    }

    void TabReorderAnimator::OnDragOver(const DragEventArgs& e)
    {
        if (!_isDragging && _draggedTabIndex < 0)
        {
            // Cross-window drag initialization
            _isDragging = true;
            _draggedTabIndex = -1;
            _currentGapIndex = -1;
            _EnsureTransformsSetup();
            _DisableBuiltInTransitions();
        }

        const auto pos = e.GetPosition(_tabView);
        const auto newGapIndex = _CalculateGapIndex(pos.X);

        if (newGapIndex != _currentGapIndex)
        {
            _AnimateTabsToMakeGap(newGapIndex);
        }
    }

    void TabReorderAnimator::OnDragCompleted()
    {
        // Snap transforms back immediately (no animation) so we don't conflict
        // with TabView's built-in reorder animation
        _ResetAllTransforms(false);
        _RestoreBuiltInTransitions();

        _isDragging = false;
        _draggedTabIndex = -1;
        _currentGapIndex = -1;
        _transforms.clear();
    }

    void TabReorderAnimator::OnDragLeave()
    {
        _ResetAllTransforms(true);
        _RestoreBuiltInTransitions();

        _isDragging = false;
        _draggedTabIndex = -1;
        _currentGapIndex = -1;
        _transforms.clear();
    }

    void TabReorderAnimator::_EnsureTransformsSetup()
    {
        _StopAllAnimations();
        _transforms.clear();

        const auto tabCount = _tabView.TabItems().Size();

        for (uint32_t i = 0; i < tabCount; i++)
        {
            if (const auto item = _tabView.ContainerFromIndex(i).try_as<MUX::TabViewItem>())
            {
                auto transform = item.RenderTransform().try_as<TranslateTransform>();
                if (!transform)
                {
                    transform = TranslateTransform{};
                    item.RenderTransform(transform);
                }
                transform.X(0.0);
                _transforms.push_back(transform);
            }
            else
            {
                _transforms.push_back(nullptr);
            }
        }
    }

    int TabReorderAnimator::_CalculateGapIndex(double pointerX) const
    {
        const auto tabCount = static_cast<int>(_tabView.TabItems().Size());

        for (int i = 0; i < tabCount; i++)
        {
            if (i == _draggedTabIndex)
            {
                continue;
            }

            if (const auto item = _tabView.ContainerFromIndex(i).try_as<MUX::TabViewItem>())
            {
                const auto itemTransform = item.TransformToVisual(_tabView);
                const auto itemPos = itemTransform.TransformPoint({ 0, 0 });
                const auto tabMidpoint = itemPos.X + (item.ActualWidth() / 2);

                if (pointerX < tabMidpoint)
                {
                    return i;
                }
            }
        }

        return tabCount;
    }

    double TabReorderAnimator::_GetTabWidth() const
    {
        const auto tabCount = _tabView.TabItems().Size();

        for (uint32_t i = 0; i < tabCount; i++)
        {
            if (static_cast<int>(i) != _draggedTabIndex)
            {
                if (const auto item = _tabView.ContainerFromIndex(i).try_as<MUX::TabViewItem>())
                {
                    return item.ActualWidth();
                }
            }
        }

        if (_draggedTabIndex >= 0 && _draggedTabIndex < static_cast<int>(tabCount))
        {
            if (const auto item = _tabView.ContainerFromIndex(_draggedTabIndex).try_as<MUX::TabViewItem>())
            {
                return item.ActualWidth();
            }
        }

        return DefaultTabWidthFallback;
    }

    void TabReorderAnimator::_AnimateTabsToMakeGap(int gapIndex)
    {
        _currentGapIndex = gapIndex;
        const auto tabWidth = _GetTabWidth();
        const auto tabCount = static_cast<int>(_transforms.size());

        _StopAllAnimations();

        for (int i = 0; i < tabCount; i++)
        {
            if (i == _draggedTabIndex)
            {
                continue;
            }

            auto& transform = _transforms[i];
            if (!transform)
            {
                continue;
            }

            double targetOffset = 0.0;

            // Only animate for same-window drags. Cross-window drags don't shift tabs
            // because there's no source gap to fill, and shifting right would push
            // tabs off-screen.
            if (_draggedTabIndex >= 0)
            {
                if (_draggedTabIndex < gapIndex)
                {
                    if (i > _draggedTabIndex && i < gapIndex)
                    {
                        targetOffset = -tabWidth;
                    }
                }
                else if (_draggedTabIndex > gapIndex)
                {
                    if (i >= gapIndex && i < _draggedTabIndex)
                    {
                        targetOffset = tabWidth;
                    }
                }
            }

            _AnimateTransformTo(transform, targetOffset);
        }
    }

    void TabReorderAnimator::_AnimateTransformTo(const TranslateTransform& transform, double targetX)
    {
        if (!transform)
        {
            return;
        }

        if (!_animationsEnabled)
        {
            transform.X(targetX);
            return;
        }

        if (std::abs(transform.X() - targetX) < 0.5)
        {
            transform.X(targetX);
            return;
        }

        const auto duration = DurationHelper::FromTimeSpan(
            TimeSpan{ std::chrono::milliseconds(AnimationDurationMs) });

        DoubleAnimation animation;
        animation.Duration(duration);
        animation.To(targetX);
        animation.EasingFunction(QuadraticEase{});
        animation.EnableDependentAnimation(true);

        Storyboard sb;
        sb.Duration(duration);
        sb.Children().Append(animation);
        sb.SetTarget(animation, transform);
        sb.SetTargetProperty(animation, L"X");

        _activeAnimations.push_back(sb);
        sb.Begin();
    }

    void TabReorderAnimator::_StopAllAnimations()
    {
        for (auto& anim : _activeAnimations)
        {
            if (anim)
            {
                anim.Stop();
            }
        }
        _activeAnimations.clear();
    }

    void TabReorderAnimator::_ResetAllTransforms(bool animated)
    {
        _StopAllAnimations();

        for (auto& transform : _transforms)
        {
            if (transform)
            {
                if (animated && _animationsEnabled)
                {
                    _AnimateTransformTo(transform, 0.0);
                }
                else
                {
                    transform.X(0.0);
                }
            }
        }
    }

    void TabReorderAnimator::_DisableBuiltInTransitions()
    {
        try
        {
            const auto childCount = VisualTreeHelper::GetChildrenCount(_tabView);
            for (int32_t i = 0; i < childCount; i++)
            {
                if (const auto listView = VisualTreeHelper::GetChild(_tabView, i).try_as<Controls::ListView>())
                {
                    if (!_transitionsSaved)
                    {
                        _savedTransitions = listView.ItemContainerTransitions();
                        _transitionsSaved = true;
                    }
                    listView.ItemContainerTransitions(nullptr);
                    break;
                }
            }
        }
        catch (...)
        {
            // Do nothing on failure - visual tree structure may vary
        }
    }

    void TabReorderAnimator::_RestoreBuiltInTransitions()
    {
        if (!_transitionsSaved)
        {
            return;
        }

        try
        {
            const auto childCount = VisualTreeHelper::GetChildrenCount(_tabView);
            for (int32_t i = 0; i < childCount; i++)
            {
                if (const auto listView = VisualTreeHelper::GetChild(_tabView, i).try_as<Controls::ListView>())
                {
                    listView.ItemContainerTransitions(_savedTransitions);
                    break;
                }
            }
        }
        catch (...)
        {
            // Do nothing on failure - visual tree structure may vary
        }

        _savedTransitions = nullptr;
        _transitionsSaved = false;
    }
}
