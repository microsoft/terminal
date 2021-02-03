// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ParentPane.h"

#include "ParentPane.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::TerminalApp;

namespace winrt::TerminalApp::implementation
{
    static const int PaneBorderSize = 2;
    static const int CombinedPaneBorderSize = 2 * PaneBorderSize;

    static const int AnimationDurationInMilliseconds = 200;
    static const Duration AnimationDuration = DurationHelper::FromTimeSpan(winrt::Windows::Foundation::TimeSpan(std::chrono::milliseconds(AnimationDurationInMilliseconds)));

    ParentPane::ParentPane(TerminalApp::LeafPane firstChild, TerminalApp::LeafPane secondChild, SplitState splitState, float splitPosition) :
        _firstChild(firstChild),
        _secondChild(secondChild),
        _splitState(splitState),
        _desiredSplitPosition(splitPosition)

    {
        InitializeComponent();

        _CreateRowColDefinitions();

        _GetGridSetColOrRowFunc()(FirstChild_Root(), 0);
        _GetGridSetColOrRowFunc()(SecondChild_Root(), 1);
    }

    Controls::Grid ParentPane::GetRootElement()
    {
        return Root();
    }

    void ParentPane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
    {
        _firstChild.UpdateSettings(settings, profile);
        _secondChild.UpdateSettings(settings, profile);
    }

    IPane ParentPane::GetActivePane()
    {
        auto first = _firstChild.GetActivePane();
        if (first != nullptr)
        {
            return first;
        }
        auto second = _secondChild.GetActivePane();
        return second;
    }

    void ParentPane::Relayout()
    {
        ResizeContent(Root().ActualSize());
    }

    void ParentPane::FocusPane(uint32_t id)
    {
        _firstChild.FocusPane(id);
        _secondChild.FocusPane(id);
    }

    void ParentPane::FocusFirstChild()
    {
        _firstChild.FocusFirstChild();
    }

    bool ParentPane::HasFocusedChild()
    {
        return _firstChild.HasFocusedChild() || _secondChild.HasFocusedChild();
    }

    void ParentPane::InitializeChildren()
    {
        FirstChild_Root().Content(_firstChild);
        SecondChild_Root().Content(_secondChild);

        _SetupChildEventHandlers(true);
        _SetupChildEventHandlers(false);

        _SetupEntranceAnimation();
    }

    void ParentPane::Shutdown()
    {
        _firstChild.Shutdown();
        _secondChild.Shutdown();
    }

    void ParentPane::ClearActive()
    {
        _firstChild.ClearActive();
        _secondChild.ClearActive();
    }

    void ParentPane::ResizeContent(const Size& newSize)
    {
        const auto width = newSize.Width;
        const auto height = newSize.Height;

        _CreateRowColDefinitions();

        if (_splitState == SplitState::Vertical)
        {
            const auto paneSizes = _CalcChildrenSizes(width);

            const Size firstSize{ paneSizes.first, height };
            const Size secondSize{ paneSizes.second, height };
            _firstChild.ResizeContent(firstSize);
            _secondChild.ResizeContent(secondSize);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            const auto paneSizes = _CalcChildrenSizes(height);

            const Size firstSize{ width, paneSizes.first };
            const Size secondSize{ width, paneSizes.second };
            _firstChild.ResizeContent(firstSize);
            _secondChild.ResizeContent(secondSize);
        }
    }

    bool ParentPane::ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction)
    {
        // Check if either our first or second child is the currently focused leaf.
        // If it is, and the requested resize direction matches our separator, then
        // we're the pane that needs to adjust its separator.
        // If our separator is the wrong direction, then we can't handle it.
        const auto firstIsLeaf = _firstChild.try_as<TerminalApp::LeafPane>();
        const auto secondIsLeaf = _secondChild.try_as<TerminalApp::LeafPane>();
        const bool firstIsFocused = firstIsLeaf && firstIsLeaf.WasLastFocused();
        const bool secondIsFocused = secondIsLeaf && secondIsLeaf.WasLastFocused();
        if (firstIsFocused || secondIsFocused)
        {
            return _Resize(direction);
        }

        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the resize.
        // For each child, if it has a focused descendant, try having that child
        // handle the resize.
        // If the child wasn't able to handle the resize, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to resize. Otherwise, just return false, as we couldn't handle it
        // either.
        const auto firstIsParent = _firstChild.try_as<ParentPane>();
        if (firstIsParent)
        {
            if (firstIsParent->GetActivePane())
            {
                return firstIsParent->ResizePane(direction) || _Resize(direction);
            }
        }

        const auto secondIsParent = _secondChild.try_as<ParentPane>();
        if (secondIsParent)
        {
            if (secondIsParent->GetActivePane())
            {
                return secondIsParent->ResizePane(direction) || _Resize(direction);
            }
        }

        return false;
    }

    bool ParentPane::NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction)
    {
        // Check if either our first or second child is the currently focused leaf.
        // If it is, and the requested move direction matches our separator, then
        // we're the pane that needs to handle this focus move.
        const auto firstIsLeaf = _firstChild.try_as<TerminalApp::LeafPane>();
        const auto secondIsLeaf = _secondChild.try_as<TerminalApp::LeafPane>();
        const bool firstIsFocused = firstIsLeaf && firstIsLeaf.WasLastFocused();
        const bool secondIsFocused = secondIsLeaf && secondIsLeaf.WasLastFocused();
        if (firstIsFocused || secondIsFocused)
        {
            return _NavigateFocus(direction);
        }

        // If neither of our children were the focused leaf, then recurse into
        // our children and see if they can handle the focus move.
        // For each child, if it has a focused descendant, try having that child
        // handle the focus move.
        // If the child wasn't able to handle the focus move, it's possible that
        // there were no descendants with a separator the correct direction. If
        // our separator _is_ the correct direction, then we should be the pane
        // to move focus into our other child. Otherwise, just return false, as
        // we couldn't handle it either.
        const auto firstIsParent = _firstChild.try_as<ParentPane>();
        if (firstIsParent)
        {
            if (firstIsParent->GetActivePane())
            {
                return firstIsParent->NavigateFocus(direction) || _NavigateFocus(direction);
            }
        }

        const auto secondIsParent = _secondChild.try_as<ParentPane>();
        if (secondIsParent)
        {
            if (secondIsParent->GetActivePane())
            {
                return secondIsParent->NavigateFocus(direction) || _NavigateFocus(direction);
            }
        }

        return false;
    }

    float ParentPane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
        return dimension - lower < higher - dimension ? lower : higher;
    }

    int ParentPane::GetLeafPaneCount() const noexcept
    {
        return _firstChild.GetLeafPaneCount() + _secondChild.GetLeafPaneCount();
    }

    Size ParentPane::GetMinSize() const
    {
        const auto firstSize = _firstChild.GetMinSize();
        const auto secondSize = _secondChild.GetMinSize();

        const auto minWidth = _splitState == SplitState::Vertical ?
                                  firstSize.Width + secondSize.Width :
                                  std::max(firstSize.Width, secondSize.Width);
        const auto minHeight = _splitState == SplitState::Horizontal ?
                                   firstSize.Height + secondSize.Height :
                                   std::max(firstSize.Height, secondSize.Height);

        return { minWidth, minHeight };
    }

    IReference<SplitState> ParentPane::PreCalculateAutoSplit(const IPane target,
                                                             const winrt::Windows::Foundation::Size availableSpace) const
    {
        const bool isVerticalSplit = _splitState == SplitState::Vertical;
        const float firstWidth = isVerticalSplit ? (availableSpace.Width * _desiredSplitPosition) : availableSpace.Width;
        const float secondWidth = isVerticalSplit ? (availableSpace.Width - firstWidth) : availableSpace.Width;
        const float firstHeight = !isVerticalSplit ? (availableSpace.Height * _desiredSplitPosition) : availableSpace.Height;
        const float secondHeight = !isVerticalSplit ? (availableSpace.Height - firstHeight) : availableSpace.Height;

        const auto firstResult = _firstChild.PreCalculateAutoSplit(target, { firstWidth, firstHeight });
        return firstResult ? firstResult : _secondChild.PreCalculateAutoSplit(target, { secondWidth, secondHeight });
    }

    IReference<bool> ParentPane::PreCalculateCanSplit(const IPane target,
                                                      SplitState splitType,
                                                      const float splitSize,
                                                      const winrt::Windows::Foundation::Size availableSpace) const
    {
        const bool isVerticalSplit = _splitState == SplitState::Vertical;
        const float firstWidth = isVerticalSplit ?
                                     (availableSpace.Width * _desiredSplitPosition) - PaneBorderSize :
                                     availableSpace.Width;
        const float secondWidth = isVerticalSplit ?
                                      (availableSpace.Width - firstWidth) - PaneBorderSize :
                                      availableSpace.Width;
        const float firstHeight = !isVerticalSplit ?
                                      (availableSpace.Height * _desiredSplitPosition) - PaneBorderSize :
                                      availableSpace.Height;
        const float secondHeight = !isVerticalSplit ?
                                       (availableSpace.Height - firstHeight) - PaneBorderSize :
                                       availableSpace.Height;

        const auto firstResult = _firstChild.PreCalculateCanSplit(target, splitType, splitSize, { firstWidth, firstHeight });
        return firstResult ? firstResult : _secondChild.PreCalculateCanSplit(target, splitType, splitSize, { secondWidth, secondHeight });
    }

    IPane ParentPane::FindFirstLeaf()
    {
        return _firstChild.FindFirstLeaf();
    }

    void ParentPane::PropagateToLeavesOnEdge(const ResizeDirection& edge, std::function<void(TerminalApp::LeafPane)> action)
    {
        if (DirectionMatchesSplit(edge, _splitState))
        {
            const auto& adjacentChild = (_splitState == SplitState::Vertical && edge == ResizeDirection::Left ||
                                         _splitState == SplitState::Horizontal && edge == ResizeDirection::Up) ?
                                           _firstChild :
                                           _secondChild;
            if (auto adjChildAsLeaf = adjacentChild.try_as<TerminalApp::LeafPane>())
            {
                action(adjChildAsLeaf);
            }
            else
            {
                auto adjChildAsParentImpl = winrt::get_self<implementation::ParentPane>(adjacentChild);
                adjChildAsParentImpl->PropagateToLeavesOnEdge(edge, action);
            }
        }
        else
        {
            if (auto firstChildAsLeaf = _firstChild.try_as<TerminalApp::LeafPane>())
            {
                action(firstChildAsLeaf);
            }
            else
            {
                auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
                firstChildAsParentImpl->PropagateToLeavesOnEdge(edge, action);
            }
            if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
            {
                action(secondChildAsLeaf);
            }
            else
            {
                auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
                secondChildAsParentImpl->PropagateToLeavesOnEdge(edge, action);
            }
        }
    }

    void ParentPane::_CreateRowColDefinitions()
    {
        const auto first = _desiredSplitPosition * 100.0f;
        const auto second = 100.0f - first;
        if (_splitState == SplitState::Vertical)
        {
            Root().ColumnDefinitions().Clear();

            // Create two columns in this grid: one for each pane

            auto firstColDef = Controls::ColumnDefinition();
            firstColDef.Width(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

            auto secondColDef = Controls::ColumnDefinition();
            secondColDef.Width(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

            Root().ColumnDefinitions().Append(firstColDef);
            Root().ColumnDefinitions().Append(secondColDef);
        }
        else if (_splitState == SplitState::Horizontal)
        {
            Root().RowDefinitions().Clear();

            // Create two rows in this grid: one for each pane

            auto firstRowDef = Controls::RowDefinition();
            firstRowDef.Height(GridLengthHelper::FromValueAndType(first, GridUnitType::Star));

            auto secondRowDef = Controls::RowDefinition();
            secondRowDef.Height(GridLengthHelper::FromValueAndType(second, GridUnitType::Star));

            Root().RowDefinitions().Append(firstRowDef);
            Root().RowDefinitions().Append(secondRowDef);
        }
    }

    bool ParentPane::_Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction)
    {
        if (!DirectionMatchesSplit(direction, _splitState))
        {
            return false;
        }

        float amount = .05f;
        if (direction == ResizeDirection::Right || direction == ResizeDirection::Down)
        {
            amount = -amount;
        }

        // Make sure we're not making a pane explode here by resizing it to 0 characters.
        const bool changeWidth = _splitState == SplitState::Vertical;

        const Size actualSize{ gsl::narrow_cast<float>(Root().ActualWidth()),
                               gsl::narrow_cast<float>(Root().ActualHeight()) };
        // actualDimension is the size in DIPs of this pane in the direction we're
        // resizing.
        const auto actualDimension = changeWidth ? actualSize.Width : actualSize.Height;

        _desiredSplitPosition = _ClampSplitPosition(changeWidth, _desiredSplitPosition - amount, actualDimension);

        // Resize our columns to match the new percentages.
        ResizeContent(actualSize);

        return true;
    }

    bool ParentPane::_NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction)
    {
        if (!DirectionMatchesSplit(direction, _splitState))
        {
            return false;
        }

        const bool focusSecond = (direction == FocusDirection::Right) || (direction == FocusDirection::Down);

        const auto newlyFocusedChild = focusSecond ? _secondChild : _firstChild;

        // If the child we want to move focus to is _already_ focused, return false,
        // to try and let our parent figure it out.
        if (newlyFocusedChild.HasFocusedChild())
        {
            return false;
        }

        // Transfer focus to our child.
        newlyFocusedChild.FocusFirstChild();

        //UpdateVisuals();
        // TODO: do we need a replacement for update visuals here?

        return true;
    }

    // todo: can the dummy grids needed for the animations (both entrance and close) be put in the xaml file?
    void ParentPane::_SetupEntranceAnimation()
    {
        // This will query if animations are enabled via the "Show animations in
        // Windows" setting in the OS
        winrt::Windows::UI::ViewManagement::UISettings uiSettings;
        const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
        const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

        const bool splitWidth = _splitState == SplitState::Vertical;
        const auto totalSize = splitWidth ? Root().ActualWidth() : Root().ActualHeight();
        // If we don't have a size yet, it's likely that we're in startup, or we're
        // being executed as a sequence of actions. In that case, just skip the
        // animation.
        if (totalSize <= 0 || !animationsEnabledInOS || !animationsEnabledInApp)
        {
            return;
        }

        const auto [firstSize, secondSize] = _CalcChildrenSizes(::base::saturated_cast<float>(totalSize));

        // This is safe to capture this, because it's only being called in the
        // context of this method (not on another thread)
        auto setupAnimation = [=](const auto& size, const bool isFirstChild) {
            auto child = isFirstChild ? _firstChild : _secondChild;
            // todo: do we need to call child's get root element or can we just use child itself?
            auto childGrid = child.GetRootElement();
            auto control = child.try_as<TerminalApp::LeafPane>().GetTerminalControl();
            // Build up our animation:
            // * it'll take as long as our duration (200ms)
            // * it'll change the value of our property from 0 to secondSize
            // * it'll animate that value using a quadratic function (like f(t) = t^2)
            // * IMPORTANT! We'll manually tell the animation that "yes we know what
            //   we're doing, we want an animation here."
            Media::Animation::DoubleAnimation animation{};
            animation.Duration(AnimationDuration);
            if (isFirstChild)
            {
                // If we're animating the first pane, the size should decrease, from
                // the full size down to the given size.
                animation.From(totalSize);
                animation.To(size);
            }
            else
            {
                // Otherwise, we want to show the pane getting larger, so animate
                // from 0 to the requested size.
                animation.From(0.0);
                animation.To(size);
            }
            animation.EasingFunction(Media::Animation::QuadraticEase{});
            animation.EnableDependentAnimation(true);

            // Now we're going to set up the Storyboard. This is a unit that uses the
            // Animation from above, and actually applies it to a property.
            // * we'll set it up for the same duration as the animation we have
            // * Apply the animation to the grid of the new pane we're adding to the tree.
            // * apply the animation to the Width or Height property.
            Media::Animation::Storyboard s;
            s.Duration(AnimationDuration);
            s.Children().Append(animation);
            s.SetTarget(animation, childGrid);
            s.SetTargetProperty(animation, splitWidth ? L"Width" : L"Height");

            // BE TRICKY:
            // We're animating the width or height of our child pane's grid.
            //
            // We DON'T want to change the size of the control itself, because the
            // terminal has to reflow the buffer every time the control changes size. So
            // what we're going to do there is manually set the control's size to how
            // big we _actually know_ the control will be.
            //
            // We're also going to be changing alignment of our child pane and the
            // control. This way, we'll be able to have the control stick to the inside
            // of the child pane's grid (the side that's moving), while we also have the
            // pane's grid stick to "outside" of the grid (the side that's not moving)
            if (splitWidth)
            {
                // If we're animating the first child, then stick to the top/left of
                // the parent pane, otherwise use the bottom/right. This is always
                // the "outside" of the parent pane.
                childGrid.HorizontalAlignment(isFirstChild ? HorizontalAlignment::Left : HorizontalAlignment::Right);
                control.HorizontalAlignment(HorizontalAlignment::Left);
                control.Width(isFirstChild ? totalSize : size);

                // When the animation is completed, undo the trickiness from before, to
                // restore the controls to the behavior they'd usually have.
                animation.Completed([childGrid, control](auto&&, auto&&) {
                    control.Width(NAN);
                    childGrid.Width(NAN);
                    childGrid.HorizontalAlignment(HorizontalAlignment::Stretch);
                    control.HorizontalAlignment(HorizontalAlignment::Stretch);
                });
            }
            else
            {
                // If we're animating the first child, then stick to the top/left of
                // the parent pane, otherwise use the bottom/right. This is always
                // the "outside" of the parent pane.
                childGrid.VerticalAlignment(isFirstChild ? VerticalAlignment::Top : VerticalAlignment::Bottom);
                control.VerticalAlignment(VerticalAlignment::Top);
                control.Height(isFirstChild ? totalSize : size);

                // When the animation is completed, undo the trickiness from before, to
                // restore the controls to the behavior they'd usually have.
                animation.Completed([childGrid, control](auto&&, auto&&) {
                    control.Height(NAN);
                    childGrid.Height(NAN);
                    childGrid.VerticalAlignment(VerticalAlignment::Stretch);
                    control.VerticalAlignment(VerticalAlignment::Stretch);
                });
            }

            // Start the animation.
            s.Begin();
        };

        // TODO: GH#7365 - animating the first child right now doesn't _really_ do
        // anything. We could do better though.
        setupAnimation(firstSize, true);
        setupAnimation(secondSize, false);
    }

    void ParentPane::_CloseChild(const bool closeFirst)
    {
        // The closed child must always be a leaf.
        const auto closedChild = (closeFirst ? _firstChild.try_as<TerminalApp::LeafPane>() : _secondChild.try_as<TerminalApp::LeafPane>());
        THROW_HR_IF_NULL(E_FAIL, closedChild);

        const auto remainingChild = closeFirst ? _secondChild : _firstChild;

        // Detach all the controls from our grid, so they can be attached later.
        // todo: ContentPresenter doesn't seem to have a 'Clear' method, so just as placeholder for now
        //       we 'detach' the control by replacing it with the empty string
        FirstChild_Root().Content(winrt::box_value(L""));
        SecondChild_Root().Content(winrt::box_value(L""));

        const auto closedChildDir = (_splitState == SplitState::Vertical) ?
                                        (closeFirst ? ResizeDirection::Left : ResizeDirection::Right) :
                                        (closeFirst ? ResizeDirection::Up : ResizeDirection::Down);

        // On all the leaf descendants that were adjacent to the closed child, update its
        // border, so that it matches the border of the closed child.
        // todo: update border with close neighbour
        PropagateToLeavesOnEdge(closedChildDir, [&](TerminalApp::LeafPane paneOnEdge) {
            paneOnEdge.UpdateBorderWithClosedNeighbor(closedChild, closedChildDir);
        });

        // If any children of closed pane was previously active, we move the focus to the remaining child
        if (closedChild.GetActivePane())
        {
            remainingChild.FindFirstLeaf().try_as<TerminalApp::LeafPane>().SetActive();
        }

        // Make sure to only fire off this event _after_ we have set the new active pane, because this event
        // might cause the tab content to change which will fire off a property changed event which eventually
        // results in the tab trying to access the active terminal control, which requires a valid active pane
        _ChildClosedHandlers(remainingChild);
    }

    winrt::fire_and_forget ParentPane::_CloseChildRoutine(const bool closeFirst)
    {
        co_await winrt::resume_foreground(Root().Dispatcher());

        // todo: do we need this weakthis??
        //if (auto pane{ weakThis.get() })
        //{
            // This will query if animations are enabled via the "Show animations in
            // Windows" setting in the OS
            winrt::Windows::UI::ViewManagement::UISettings uiSettings;
            const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
            const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

            // GH#7252: If either child is zoomed, just skip the animation. It won't work.
            const bool eitherChildZoomed = false;
            //const bool eitherChildZoomed = pane->_firstChild->_zoomed || pane->_secondChild->_zoomed;
            // If animations are disabled, just skip this and go straight to
            // _CloseChild. Curiously, the pane opening animation doesn't need this,
            // and will skip straight to Completed when animations are disabled, but
            // this one doesn't seem to.
            if (!animationsEnabledInOS || !animationsEnabledInApp || eitherChildZoomed)
            {
                _CloseChild(closeFirst);
                co_return;
            }

            // Setup the animation

            auto removedChild = closeFirst ? _firstChild : _secondChild;
            auto remainingChild = closeFirst ? _secondChild : _firstChild;
            const bool splitWidth = _splitState == SplitState::Vertical;
            const auto totalSize = splitWidth ? Root().ActualWidth() : Root().ActualHeight();

            Size removedOriginalSize;
            Size remainingOriginalSize;

            if (const auto removedAsLeaf = removedChild.try_as<TerminalApp::LeafPane>())
            {
                removedOriginalSize = Size{ ::base::saturated_cast<float>(removedAsLeaf.ActualWidth()),
                                            ::base::saturated_cast<float>(removedAsLeaf.ActualHeight()) };
            }
            else
            {
                const auto removedAsParent = removedChild.try_as<TerminalApp::ParentPane>();
                removedOriginalSize = Size{ ::base::saturated_cast<float>(removedAsParent.ActualWidth()),
                                            ::base::saturated_cast<float>(removedAsParent.ActualHeight()) };
            }

            if (const auto remainingAsLeaf = remainingChild.try_as<TerminalApp::LeafPane>())
            {
                remainingOriginalSize = Size{ ::base::saturated_cast<float>(remainingAsLeaf.ActualWidth()),
                                              ::base::saturated_cast<float>(remainingAsLeaf.ActualHeight()) };
            }
            else
            {
                const auto remainingAsParent = remainingChild.try_as<TerminalApp::ParentPane>();
                remainingOriginalSize = Size{ ::base::saturated_cast<float>(remainingAsParent.ActualWidth()),
                                              ::base::saturated_cast<float>(remainingAsParent.ActualHeight()) };
            }

            // Remove both children from the grid
            //_root.Children().Clear();
            // todo: ContentPresenter doesn't seem to have a 'Clear' method, so just as placeholder for now
            //       we 'detach' the control by replacing it with the empty string
            FirstChild_Root().Content(winrt::box_value(L""));
            SecondChild_Root().Content(winrt::box_value(L""));
            // Add the remaining child back to the grid, in the right place.
            if (const auto remainingAsLeaf = remainingChild.try_as<TerminalApp::LeafPane>())
            {
                closeFirst ? SecondChild_Root().Content(remainingAsLeaf) : FirstChild_Root().Content(remainingAsLeaf);
            }
            else
            {
                const auto remainingAsParent = remainingChild.try_as<TerminalApp::ParentPane>();
                closeFirst ? SecondChild_Root().Content(remainingAsParent) : FirstChild_Root().Content(remainingAsParent);
            }
            //_root.Children().Append(remainingChild->GetRootElement());
            //if (_splitState == SplitState::Vertical)
            //{
            //    Controls::Grid::SetColumn(remainingChild->GetRootElement(), closeFirst ? 1 : 0);
            //}
            //else if (_splitState == SplitState::Horizontal)
            //{
            //    Controls::Grid::SetRow(remainingChild->GetRootElement(), closeFirst ? 1 : 0);
            //}

            // Create the dummy grid. This grid will be the one we actually animate,
            // in the place of the closed pane.
            Controls::Grid dummyGrid;
            //dummyGrid.Background(s_unfocusedBorderBrush);
            //dummyGrid.Background();
            // It should be the size of the closed pane.
            dummyGrid.Width(removedOriginalSize.Width);
            dummyGrid.Height(removedOriginalSize.Height);
            // Put it where the removed child is
            if (_splitState == SplitState::Vertical)
            {
                Controls::Grid::SetColumn(dummyGrid, closeFirst ? 0 : 1);
            }
            else if (_splitState == SplitState::Horizontal)
            {
                Controls::Grid::SetRow(dummyGrid, closeFirst ? 0 : 1);
            }
            // Add it to the tree
            Root().Children().Append(dummyGrid);

            // Set up the rows/cols as auto/auto, so they'll only use the size of
            // the elements in the grid.
            //
            // * For the closed pane, we want to make that row/col "auto" sized, so
            //   it takes up as much space as is available.
            // * For the remaining pane, we'll make that row/col "*" sized, so it
            //   takes all the remaining space. As the dummy grid is resized down,
            //   the remaining pane will expand to take the rest of the space.
            Root().ColumnDefinitions().Clear();
            Root().RowDefinitions().Clear();
            if (_splitState == SplitState::Vertical)
            {
                auto firstColDef = Controls::ColumnDefinition();
                auto secondColDef = Controls::ColumnDefinition();
                firstColDef.Width(!closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
                secondColDef.Width(closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
                Root().ColumnDefinitions().Append(firstColDef);
                Root().ColumnDefinitions().Append(secondColDef);
            }
            else if (_splitState == SplitState::Horizontal)
            {
                auto firstRowDef = Controls::RowDefinition();
                auto secondRowDef = Controls::RowDefinition();
                firstRowDef.Height(!closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
                secondRowDef.Height(closeFirst ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
                Root().RowDefinitions().Append(firstRowDef);
                Root().RowDefinitions().Append(secondRowDef);
            }

            // Animate the dummy grid from its current size down to 0
            Media::Animation::DoubleAnimation animation{};
            animation.Duration(AnimationDuration);
            animation.From(splitWidth ? removedOriginalSize.Width : removedOriginalSize.Height);
            animation.To(0.0);
            // This easing is the same as the entrance animation.
            animation.EasingFunction(Media::Animation::QuadraticEase{});
            animation.EnableDependentAnimation(true);

            Media::Animation::Storyboard s;
            s.Duration(AnimationDuration);
            s.Children().Append(animation);
            s.SetTarget(animation, dummyGrid);
            s.SetTargetProperty(animation, splitWidth ? L"Width" : L"Height");

            // Start the animation.
            s.Begin();

            //std::weak_ptr<Pane> weakThis{ shared_from_this() };

            // When the animation is completed, reparent the child's content up to
            // us, and remove the child nodes from the tree.
            animation.Completed([=](auto&&, auto&&) {
                //if (auto pane{ weakThis.lock() })
                //{
                    // We don't need to manually undo any of the above trickiness.
                    // We're going to re-parent the child's content into us anyways
                    //pane->_CloseChild(closeFirst);
                _CloseChild(closeFirst);
                //}
            });
        //}
    }

    void ParentPane::_SetupChildEventHandlers(const bool isFirstChild)
    {
        auto& child = isFirstChild ? _firstChild : _secondChild;
        auto& closedToken = isFirstChild ? _firstClosedToken : _secondClosedToken;
        auto& typeChangedToken = isFirstChild ? _firstTypeChangedToken : _secondTypeChangedToken;

        if (const auto childAsLeaf = child.try_as<LeafPane>())
        {
            // When our child is a leaf and got closed, we close it
            const auto childImpl = winrt::get_self<implementation::LeafPane>(child);

            // todo: check if we need to do a 'get_weak()' here or something?
            closedToken = childImpl->Closed([=](auto&& /*s*/, auto&& /*e*/) {
                // Unsubscribe from events of both our children, as we ourself will also
                // get closed when our child does.
                _RemoveAllChildEventHandlers(false);
                _RemoveAllChildEventHandlers(true);
                _CloseChildRoutine(isFirstChild);
            });

            // When our child is a leaf and gets split, it produces a new parent pane that contains
            // both itself and its new leaf neighbor. We then replace that child with the new parent pane.
            typeChangedToken = childImpl->GotSplit([=](TerminalApp::ParentPane newParent) {
                _OnChildSplitOrCollapse(isFirstChild, newParent);
            });
        }
        else if (const auto childAsParent = child.try_as<ParentPane>())
        {
            // When our child is a parent and one of its children got closed (and so the parent collapses),
            // we take in its remaining, orphaned child as our own.
            typeChangedToken = childAsParent->ChildClosed([=](TerminalApp::IPane remainingChild) {
                _OnChildSplitOrCollapse(isFirstChild, remainingChild);
            });
        }
    }

    void ParentPane::_RemoveAllChildEventHandlers(const bool isFirstChild)
    {
        const auto child = isFirstChild ? _firstChild : _secondChild;
        const auto closedToken = isFirstChild ? _firstClosedToken : _secondClosedToken;
        const auto typeChangedToken = isFirstChild ? _firstTypeChangedToken : _secondTypeChangedToken;

        if (const auto childAsLeaf = child.try_as<TerminalApp::LeafPane>())
        {
            const auto childImpl = winrt::get_self<implementation::LeafPane>(child);
            childImpl->Closed(closedToken);
            childImpl->GotSplit(typeChangedToken);
        }
        else if (const auto childAsParent = child.try_as<ParentPane>())
        {
            childAsParent->ChildClosed(typeChangedToken);
        }
    }

    void ParentPane::_OnChildSplitOrCollapse(const bool isFirstChild, IPane newChild)
    {
        // Unsub from all the events of the parent child.
        _RemoveAllChildEventHandlers(isFirstChild);

        (isFirstChild ? _firstChild : _secondChild) = newChild;

        if (const auto newChildAsLeaf = newChild.try_as<TerminalApp::LeafPane>())
        {
            isFirstChild ? FirstChild_Root().Content(newChildAsLeaf) : SecondChild_Root().Content(newChildAsLeaf);
            // todo: do we need this getgridsetcolorrow call?
            //_GetGridSetColOrRowFunc()(isFirstChild ? FirstChild_Root() : SecondChild_Root(), isFirstChild ? 0 : 1);
        }
        else
        {
            const auto newChildAsParent = newChild.try_as<TerminalApp::ParentPane>();
            isFirstChild ? FirstChild_Root().Content(newChildAsParent) : SecondChild_Root().Content(newChildAsParent);
            //_GetGridSetColOrRowFunc()(isFirstChild ? FirstChild_Root() : SecondChild_Root(), isFirstChild ? 0 : 1);
        }

        // Setup events appropriate for the new child
        _SetupChildEventHandlers(isFirstChild);
    }

    std::function<void(winrt::Windows::UI::Xaml::FrameworkElement const&, int32_t)> ParentPane::_GetGridSetColOrRowFunc() const noexcept
    {
        if (_splitState == SplitState::Vertical)
        {
            return Controls::Grid::SetColumn;
        }
        else
        {
            return Controls::Grid::SetRow;
        }
    }

    std::pair<float, float> ParentPane::_CalcChildrenSizes(const float fullSize) const
    {
        const auto widthOrHeight = _splitState == SplitState::Vertical;
        const auto snappedSizes = _CalcSnappedChildrenSizes(widthOrHeight, fullSize).lower;

        // Keep the first pane snapped and give the second pane all remaining size
        return {
            snappedSizes.first,
            fullSize - snappedSizes.first
        };
    }

    ParentPane::SnapChildrenSizeResult ParentPane::_CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const
    {
        auto sizeTree = _CreateMinSizeTree(widthOrHeight);
        LayoutSizeNode lastSizeTree{ sizeTree };

        while (sizeTree.size < fullSize)
        {
            lastSizeTree = sizeTree;
            AdvanceSnappedDimension(widthOrHeight, sizeTree);

            if (sizeTree.size == fullSize)
            {
                // If we just hit exactly the requested value, then just return the
                // current state of children.
                return { { sizeTree.firstChild->size, sizeTree.secondChild->size },
                         { sizeTree.firstChild->size, sizeTree.secondChild->size } };
            }
        }

        // We exceeded the requested size in the loop above, so lastSizeTree will have
        // the last good sizes (so that children fit in) and sizeTree has the next possible
        // snapped sizes. Return them as lower and higher snap possibilities.
        return { { lastSizeTree.firstChild->size, lastSizeTree.secondChild->size },
                 { sizeTree.firstChild->size, sizeTree.secondChild->size } };
    }

    SnapSizeResult ParentPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        {
            // If we're resizing along separator axis, snap to the closest possibility
            // given by our children panes.

            const auto firstSnapped = _firstChild._CalcSnappedDimension(widthOrHeight, dimension);
            const auto secondSnapped = _secondChild._CalcSnappedDimension(widthOrHeight, dimension);
            return {
                std::max(firstSnapped.lower, secondSnapped.lower),
                std::min(firstSnapped.higher, secondSnapped.higher)
            };
        }
        else
        {
            // If we're resizing perpendicularly to separator axis, calculate the sizes
            // of child panes that would fit the given size. We use same algorithm that
            // is used for real resize routine, but exclude the remaining empty space that
            // would appear after the second pane. This will be the 'downward' snap possibility,
            // while the 'upward' will be given as a side product of the layout function.

            const auto childSizes = _CalcSnappedChildrenSizes(widthOrHeight, dimension);
            return {
                childSizes.lower.first + childSizes.lower.second,
                childSizes.higher.first + childSizes.higher.second
            };
        }
    }

    void ParentPane::AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
    {
        // We're a parent pane, so we have to advance dimension of our children panes. In
        // fact, we advance only one child (chosen later) to keep the growth fine-grained.

        // To choose which child pane to advance, we actually need to know their advanced sizes
        // in advance (oh), to see which one would 'fit' better. Often, this is already cached
        // by the previous invocation of this function in nextFirstChild and nextSecondChild
        // fields of given node. If not, we need to calculate them now.
        if (sizeNode.nextFirstChild == nullptr)
        {
            sizeNode.nextFirstChild = std::make_unique<LayoutSizeNode>(*sizeNode.firstChild);

            if (auto firstChildAsLeaf = _firstChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextFirstChild->isMinimumSize)
                {
                    sizeNode.nextFirstChild->size = _firstChild._CalcSnappedDimension(widthOrHeight, sizeNode.nextFirstChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = firstChildAsLeaf.GetTerminalControl().CharacterDimensions();
                    sizeNode.nextFirstChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
                firstChildAsParentImpl->AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
            }
        }
        if (sizeNode.nextSecondChild == nullptr)
        {
            sizeNode.nextSecondChild = std::make_unique<LayoutSizeNode>(*sizeNode.secondChild);

            if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextSecondChild->isMinimumSize)
                {
                    sizeNode.nextSecondChild->size = _secondChild._CalcSnappedDimension(widthOrHeight, sizeNode.nextSecondChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = secondChildAsLeaf.GetTerminalControl().CharacterDimensions();
                    sizeNode.nextSecondChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
                secondChildAsParentImpl->AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
            }
        }

        const auto nextFirstSize = sizeNode.nextFirstChild->size;
        const auto nextSecondSize = sizeNode.nextSecondChild->size;

        // Choose which child pane to advance.
        bool advanceFirstOrSecond;
        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        {
            // If we're growing along separator axis, choose the child that
            // wants to be smaller than the other, so that the resulting size
            // will be the smallest.
            advanceFirstOrSecond = nextFirstSize < nextSecondSize;
        }
        else
        {
            // If we're growing perpendicularly to separator axis, choose a
            // child so that their size ratio is closer to that we're trying
            // to maintain (this is, the relative separator position is closer
            // to the _desiredSplitPosition field).

            const auto firstSize = sizeNode.firstChild->size;
            const auto secondSize = sizeNode.secondChild->size;

            // Because we rely on equality check, these calculations have to be
            // immune to floating point errors. In common situation where both panes
            // have the same character sizes and _desiredSplitPosition is 0.5 (or
            // some simple fraction) both ratios will often be the same, and if so
            // we always take the left child. It could be right as well, but it's
            // important that it's consistent: that it would always go
            // 1 -> 2 -> 1 -> 2 -> 1 -> 2 and not like 1 -> 1 -> 2 -> 2 -> 2 -> 1
            // which would look silly to the user but which occur if there was
            // a non-floating-point-safe math.
            const auto deviation1 = nextFirstSize - (nextFirstSize + secondSize) * _desiredSplitPosition;
            const auto deviation2 = -1 * (firstSize - (firstSize + nextSecondSize) * _desiredSplitPosition);
            advanceFirstOrSecond = deviation1 <= deviation2;
        }

        // Here we advance one of our children. Because we already know the appropriate
        // (advanced) size that given child would need to have, we simply assign that size
        // to it. We then advance its 'next*' size (nextFirstChild or nextSecondChild) so
        // the invariant holds (as it will likely be used by the next invocation of this
        // function). The other child's next* size remains unchanged because its size
        // haven't changed either.
        if (advanceFirstOrSecond)
        {
            *sizeNode.firstChild = *sizeNode.nextFirstChild;
            // todo: there has to be a better way to do these recursive calls
            //_firstChild.AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
            if (auto firstChildAsLeaf = _firstChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextFirstChild->isMinimumSize)
                {
                    sizeNode.nextFirstChild->size = _firstChild._CalcSnappedDimension(widthOrHeight, sizeNode.nextFirstChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = firstChildAsLeaf.GetTerminalControl().CharacterDimensions();
                    sizeNode.nextFirstChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
                firstChildAsParentImpl->AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
            }
        }
        else
        {
            *sizeNode.secondChild = *sizeNode.nextSecondChild;
            //_secondChild.AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
            if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextSecondChild->isMinimumSize)
                {
                    sizeNode.nextSecondChild->size = _secondChild._CalcSnappedDimension(widthOrHeight, sizeNode.nextSecondChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = secondChildAsLeaf.GetTerminalControl().CharacterDimensions();
                    sizeNode.nextSecondChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
                secondChildAsParentImpl->AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
            }
        }

        // Since the size of one of our children has changed we need to update our size as well.
        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        {
            sizeNode.size = std::max(sizeNode.firstChild->size, sizeNode.secondChild->size);
        }
        else
        {
            sizeNode.size = sizeNode.firstChild->size + sizeNode.secondChild->size;
        }

        // Because we have grown, we're certainly no longer of our
        // minimal size (if we've ever been).
        sizeNode.isMinimumSize = false;
    }

    ParentPane::LayoutSizeNode ParentPane::_CreateMinSizeTree(const bool widthOrHeight) const
    {
        const auto size = GetMinSize();
        LayoutSizeNode node(widthOrHeight ? size.Width : size.Height);
        if (auto firstChildAsLeaf = _firstChild.try_as<TerminalApp::LeafPane>())
        {
            const auto firstSize = firstChildAsLeaf.GetMinSize();
            node.firstChild = std::make_unique<LayoutSizeNode>(widthOrHeight ? firstSize.Width : firstSize.Height);
        }
        else
        {
            auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
            node.firstChild = std::make_unique<LayoutSizeNode>(firstChildAsParentImpl->_CreateMinSizeTree(widthOrHeight));
        }
        if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
        {
            const auto secondSize = secondChildAsLeaf.GetMinSize();
            node.secondChild = std::make_unique<LayoutSizeNode>(widthOrHeight ? secondSize.Width : secondSize.Height);
        }
        else
        {
            auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
            node.secondChild = std::make_unique<LayoutSizeNode>(secondChildAsParentImpl->_CreateMinSizeTree(widthOrHeight));
        }

        return node;
    }

    float ParentPane::_ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const
    {
        const auto firstMinSize = _firstChild.GetMinSize();
        const auto secondMinSize = _secondChild.GetMinSize();

        const auto firstMinDimension = widthOrHeight ? firstMinSize.Width : firstMinSize.Height;
        const auto secondMinDimension = widthOrHeight ? secondMinSize.Width : secondMinSize.Height;

        const auto minSplitPosition = firstMinDimension / totalSize;
        const auto maxSplitPosition = 1.0f - (secondMinDimension / totalSize);

        return std::clamp(requestedValue, minSplitPosition, maxSplitPosition);
    }

    DEFINE_EVENT(ParentPane, ChildClosed, _ChildClosedHandlers, winrt::delegate<IPane>);

    // TODO: Move all this layout size node stuff elsewhere or put it in LayoutSizeNode.cpp
    ParentPane::LayoutSizeNode::LayoutSizeNode(const float minSize) :
        size{ minSize },
        isMinimumSize{ true },
        firstChild{ nullptr },
        secondChild{ nullptr },
        nextFirstChild{ nullptr },
        nextSecondChild{ nullptr }
    {
    }

    ParentPane::LayoutSizeNode::LayoutSizeNode(const LayoutSizeNode& other) :
        size{ other.size },
        isMinimumSize{ other.isMinimumSize },
        firstChild{ other.firstChild ? std::make_unique<LayoutSizeNode>(*other.firstChild) : nullptr },
        secondChild{ other.secondChild ? std::make_unique<LayoutSizeNode>(*other.secondChild) : nullptr },
        nextFirstChild{ other.nextFirstChild ? std::make_unique<LayoutSizeNode>(*other.nextFirstChild) : nullptr },
        nextSecondChild{ other.nextSecondChild ? std::make_unique<LayoutSizeNode>(*other.nextSecondChild) : nullptr }
    {
    }

    // Method Description:
    // - Makes sure that this node and all its descendants equal the supplied node.
    //   This may be more efficient that copy construction since it will reuse its
    //   allocated children.
    // Arguments:
    // - other: Node to take the values from.
    // Return Value:
    // - itself
    ParentPane::LayoutSizeNode& ParentPane::LayoutSizeNode::operator=(const LayoutSizeNode& other)
    {
        size = other.size;
        isMinimumSize = other.isMinimumSize;

        _AssignChildNode(firstChild, other.firstChild.get());
        _AssignChildNode(secondChild, other.secondChild.get());
        _AssignChildNode(nextFirstChild, other.nextFirstChild.get());
        _AssignChildNode(nextSecondChild, other.nextSecondChild.get());

        return *this;
    }

    // Method Description:
    // - Performs assignment operation on a single child node reusing
    // - current one if present.
    // Arguments:
    // - nodeField: Reference to our field holding concerned node.
    // - other: Node to take the values from.
    // Return Value:
    // - <none>
    void ParentPane::LayoutSizeNode::_AssignChildNode(std::unique_ptr<LayoutSizeNode>& nodeField, const LayoutSizeNode* const newNode)
    {
        if (newNode)
        {
            if (nodeField)
            {
                *nodeField = *newNode;
            }
            else
            {
                nodeField = std::make_unique<LayoutSizeNode>(*newNode);
            }
        }
        else
        {
            nodeField.release();
        }
    }

}
