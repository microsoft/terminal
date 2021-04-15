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
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::TerminalConnection;

namespace winrt::TerminalApp::implementation
{
    static const int PaneBorderSize = 2;
    static const int CombinedPaneBorderSize = 2 * PaneBorderSize;
    winrt::Windows::UI::Xaml::Media::SolidColorBrush ParentPane::s_unfocusedBorderBrush = { nullptr };

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

        _GridLayoutHelper();

        _firstLayoutRevoker = firstChild.TerminalControl().LayoutUpdated(winrt::auto_revoke, [&](auto /*s*/, auto /*e*/) {
            _ChildrenLayoutUpdatedHelper(true);
        });

        _secondLayoutRevoker = secondChild.TerminalControl().LayoutUpdated(winrt::auto_revoke, [&](auto /*s*/, auto /*e*/) {
            _ChildrenLayoutUpdatedHelper(false);
        });

        _SetupResources();
    }

    // Method Description:
    // - Get the root UIElement of this pane. In our case is a grid
    //   with 2 content presenters, one for each child
    // Return Value:
    // - the Grid acting as the root of this pane.
    Controls::Grid ParentPane::GetRootElement()
    {
        return Root();
    }

    // Method Description:
    // - Updates the settings of the children of this pane recursively
    // Arguments:
    // - settings: The new TerminalSettings to apply to any matching controls
    // - profile: The GUID of the profile these settings should apply to
    void ParentPane::UpdateSettings(const TerminalSettingsCreateResult& settings, const GUID& profile)
    {
        _firstChild.UpdateSettings(settings, profile);
        _secondChild.UpdateSettings(settings, profile);
    }

    // Method Description:
    // - Returns nullptr if no children of this pane were the last pane to be focused, or the Pane that
    //   _was_ the last pane to be focused
    // Return Value:
    // - nullptr if no children were marked `_lastActive`, else returns the last active child
    IPane ParentPane::GetActivePane() const
    {
        const auto first = _firstChild.GetActivePane();
        if (first != nullptr)
        {
            return first;
        }
        return _secondChild.GetActivePane();
    }

    // Method Description:
    // - Recalculates and reapplies sizes of all descendant panes.
    void ParentPane::Relayout()
    {
        ResizeContent(Root().ActualSize());
    }

    // Method Description:
    // - Recursive function that focuses a pane with the given ID
    // Arguments:
    // - The ID of the pane we want to focus
    void ParentPane::FocusPane(uint32_t id)
    {
        _firstChild.FocusPane(id);
        _secondChild.FocusPane(id);
    }

    // Method Description:
    // - Focuses the first leaf of our first child, recursively.
    void ParentPane::FocusFirstChild()
    {
        _firstChild.FocusFirstChild();
    }

    // Method Description:
    // - Returns true is a pane which is a child of this pane that is actively focused
    // Return Value:
    // - true if the currently focused pane is one of this pane's descendants
    bool ParentPane::HasFocusedChild()
    {
        return _firstChild.HasFocusedChild() || _secondChild.HasFocusedChild();
    }

    bool ParentPane::ContainsReadOnly()
    {
        return _firstChild.ContainsReadOnly() || _secondChild.ContainsReadOnly();
    }

    // Method Description:
    // - Adds our children to the UI tree
    // - Adds event handlers for our children
    // - Animates our children
    void ParentPane::InitializeChildren()
    {
        Root().Children().Append(_firstChild.try_as<FrameworkElement>());
        Root().Children().Append(_secondChild.try_as<FrameworkElement>());

        _SetupChildEventHandlers(true);
        _SetupChildEventHandlers(false);
    }

    // Method Description:
    // - Prepare this pane to be removed from the UI hierarchy by closing all controls
    //   and connections beneath it.
    void ParentPane::Shutdown()
    {
        _firstChild.Shutdown();
        _secondChild.Shutdown();
    }

    // Method Description:
    // - Recursively remove the "Active" state from any leaf descendants of this parent pane
    void ParentPane::ClearActive()
    {
        _firstChild.ClearActive();
        _secondChild.ClearActive();
    }

    // Method Description:
    // - Update the size of this pane. Resizes each of our columns so they have the
    //   same relative sizes, given the newSize.
    // - Because we're just manually setting the row/column sizes in pixels, we have
    //   to be told our new size, we can't just use our own OnSized event, because
    //   that _won't fire when we get smaller_.
    // Arguments:
    // - newSize: the amount of space that this pane has to fill now.
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

    // Method Description:
    // - Moves the separator between panes, as to resize each child on either size
    //   of the separator. Tries to move a separator in the given direction. The
    //   separator moved is the separator that's closest depth-wise to the
    //   currently focused pane, that's also in the correct direction to be moved.
    //   If there isn't such a separator, then this method returns false, as we
    //   couldn't handle the resize.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - true if we or a child handled this resize request.
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

    // Method Description:
    // - Attempts to move focus to one of our children. If we have a focused child,
    //   we'll try to move the focus in the direction requested.
    //   - If there isn't a pane that exists as a child of this pane in the correct
    //     direction, we'll return false. This will indicate to our parent that they
    //     should try and move the focus themselves. In this way, the focus can move
    //     up and down the tree to the correct pane.
    // - This method is _very_ similar to ResizePane. Both are trying to find the
    //   right separator to move (focus) in a direction.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - true if we or a child handled this focus move request.
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

    // Method Description:
    // - Adjusts given dimension (width or height) so that all descendant terminals
    //   align with their character grids as close as possible. Snaps to closes match
    //   (either upward or downward). Also makes sure to fit in minimal sizes of the panes.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // - dimension: a dimension (width or height) to snap
    // Return Value:
    // - A value corresponding to the next closest snap size for this Pane, either upward or downward
    float ParentPane::CalcSnappedDimensionSingle(const bool widthOrHeight, const float dimension) const
    {
        const auto [lower, higher] = CalcSnappedDimension(widthOrHeight, dimension);
        return dimension - lower < higher - dimension ? lower : higher;
    }

    uint32_t ParentPane::GetLeafPaneCount() const noexcept
    {
        return _firstChild.GetLeafPaneCount() + _secondChild.GetLeafPaneCount();
    }

    // Method Description:
    // - Get the absolute minimum size that this pane can be resized to and still
    //   have 1x1 character visible in each of its children
    // Return Value:
    // - The minimum size that this pane can be resized to and still have a visible
    //   character.
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

    // Method Description:
    // - Recursively attempt to "zoom" the given pane. When the pane is zoomed, it
    //   won't be displayed as part of the tab tree, instead it'll take up the full
    //   content of the tab. When we find the given pane, we'll need to remove it
    //   from the UI tree, so that the caller can re-add it. We'll also set some
    //   internal state, so the pane can display all of its borders.
    // Arguments:
    // - zoomedPane: This is the pane which we're attempting to zoom on.
    void ParentPane::Maximize(IPane paneToZoom)
    {
        if (paneToZoom == _firstChild || paneToZoom == _secondChild)
        {
            // When we're zooming the pane, we'll need to remove it from our UI
            // tree. Easy way: just remove both children. We'll re-attach both
            // when we un-zoom.
            Root().Children().Clear();
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update it's borders.
        _firstChild.Maximize(paneToZoom);
        _secondChild.Maximize(paneToZoom);
    }

    // Method Description:
    // - Recursively attempt to "un-zoom" the given pane. This does the opposite of
    //   ParentPane::Maximize. When we find the given pane, we should return the pane to our
    //   UI tree. We'll also clear the internal state, so the pane can display its
    //   borders correctly.
    // - The caller should make sure to have removed the zoomed pane from the UI
    //   tree _before_ calling this.
    // Arguments:
    // - zoomedPane: This is the pane which we're attempting to un-zoom.
    void ParentPane::Restore(IPane paneToUnzoom)
    {
        if (paneToUnzoom == _firstChild || paneToUnzoom == _secondChild)
        {
            // When we're un-zooming the pane, we'll need to re-add it to our UI
            // tree where it originally belonged. easy way: just re-add both.
            Root().Children().Clear();
            Root().Children().Append(_firstChild.try_as<FrameworkElement>());
            Root().Children().Append(_secondChild.try_as<FrameworkElement>());
        }

        // Always recurse into both children. If the (un)zoomed pane was one of
        // our direct children, we'll still want to update it's borders.
        _firstChild.Restore(paneToUnzoom);
        _secondChild.Restore(paneToUnzoom);
    }

    // Method Description:
    // - This is a helper to determine which direction an "Automatic" split should
    //   happen in for a given pane, but without using the ActualWidth() and
    //   ActualHeight() methods. This is used during the initialization of the
    //   Terminal, when we could be processing many "split-pane" commands _before_
    //   we've ever laid out the Terminal for the first time. When this happens, the
    //   Pane's don't have an actual size yet. However, we'd still like to figure
    //   out how to do an "auto" split when these Panes are all laid out.
    // - This method assumes that the Pane we're attempting to split is `target`,
    //   and this method should be called on the root of a tree of Panes.
    // - We'll walk down the tree attempting to find `target`. As we traverse the
    //   tree, we'll reduce the size passed to each subsequent recursive call. The
    //   size passed to this method represents how much space this Pane _will_ have
    //   to use.
    // - Since this pane is a parent, calculate how much space our children will be
    //   able to use, and recurse into them.
    // Arguments:
    // - target: The Pane we're attempting to split.
    // - availableSpace: The theoretical space that's available for this pane to be able to split.
    // Return Value:
    // - nullopt if `target` is not this pane or a child of this pane, otherwise the
    //   SplitState that `target` would use for an `Automatic` split given
    //   `availableSpace`
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

    // Method Description:
    // - This is a helper to determine if a given Pane can be split, but without
    //   using the ActualWidth() and ActualHeight() methods. This is used during
    //   processing of many "split-pane" commands, which could happen _before_ we've
    //   laid out a Pane for the first time. When this happens, the Pane's don't
    //   have an actual size yet. However, we'd still like to figure out if the pane
    //   could be split, once they're all laid out.
    // - This method assumes that the Pane we're attempting to split is `target`,
    //   and this method should be called on the root of a tree of Panes.
    // - We'll walk down the tree attempting to find `target`. As we traverse the
    //   tree, we'll reduce the size passed to each subsequent recursive call. The
    //   size passed to this method represents how much space this Pane _will_ have
    //   to use.
    // - Since this pane is a parent, calculate how much space our children will be
    //   able to use, and recurse into them.
    // Arguments:
    // - target: The Pane we're attempting to split.
    // - splitType: The direction we're attempting to split in.
    // - availableSpace: The theoretical space that's available for this pane to be able to split.
    // Return Value:
    // - nullopt if `target` is not a child of this pane, otherwise
    //   true iff we could split this pane, given `availableSpace`
    // Note:
    // - This method is highly similar to PreCalculateAutoSplit
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

    // Method Description:
    // - Sets up row/column definitions for this pane. There are three total
    //   row/cols. The middle one is for the separator. The first and third are for
    //   each of the child panes, and are given a size in pixels, based off the
    //   available space, and the percent of the space they respectively consume,
    //   which is stored in _desiredSplitPosition
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

    // Method Description:
    // - Adjust our child percentages to increase the size of one of our children
    //   and decrease the size of the other.
    // - Adjusts the separation amount by 5%
    // - Does nothing if the direction doesn't match our current split direction
    // Arguments:
    // - direction: the direction to move our separator. If it's down or right,
    //   we'll be increasing the size of the first of our children. Else, we'll be
    //   decreasing the size of our first child.
    // Return Value:
    // - false if we couldn't resize this pane in the given direction, else true.
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

    // Method Description:
    // - Attempts to handle moving focus to one of our children. If our split
    //   direction isn't appropriate for the move direction, then we'll return
    //   false, to try and let our parent handle the move. If our child we'd move
    //   focus to is already focused, we'll also return false, to again let our
    //   parent try and handle the focus movement.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - true if we handled this focus move request.
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

        return true;
    }

    // Method Description:
    // - Helper to handle when our children's layouts have updated
    // - When a child's layout updates, we revoke the revoker to only let this succeed once,
    //   and then we check if both children's layouts have been updated, if so then we go ahead
    //   and initialize them
    // Arguments:
    // - isFirstChild: which child's layout got updated
    void ParentPane::_ChildrenLayoutUpdatedHelper(const bool isFirstChild)
    {
        if (isFirstChild)
        {
            _firstLayoutUpdated = true;
            _firstLayoutRevoker.revoke();
        }
        else
        {
            _secondLayoutUpdated = true;
            _secondLayoutRevoker.revoke();
        }

        if (_firstLayoutUpdated && _secondLayoutUpdated)
        {
            // Once both children have their sizes, we can initialize them
            _SetupEntranceAnimation();
        }
    }

    // Method Description:
    // - Create a pair of animations when a new control enters this pane. This
    //   should _ONLY_ be called in InitializeChildren, AFTER the first and second child panes
    //   have been set up
    void ParentPane::_SetupEntranceAnimation()
    {
        // This will query if animations are enabled via the "Show animations in
        // Windows" setting in the OS
        winrt::Windows::UI::ViewManagement::UISettings uiSettings;
        const auto animationsEnabledInOS = uiSettings.AnimationsEnabled();
        const auto animationsEnabledInApp = Media::Animation::Timeline::AllowDependentAnimations();

        const bool splitWidth = _splitState == SplitState::Vertical;
        const auto totalSize = splitWidth ? ActualWidth() : ActualHeight();

        // There's a chance that we're in startup and so by the time we get here one of our children is
        // actually a parent, don't try to animate in that case
        if (!(_firstChild.try_as<TerminalApp::LeafPane>() && _secondChild.try_as<TerminalApp::LeafPane>()))
        {
            return;
        }

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
            auto childGrid = child.GetRootElement();
            auto control = child.try_as<TerminalApp::LeafPane>().TerminalControl();
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

    // Method Description:
    // - Closes one of our children. In doing so, emit an event containing the
    //   remaining child, so that whoever is listening (either our parent or
    //   the hosting tab if we were the root) will replace us with the remaining child
    // Arguments:
    // - closeFirst: if true, the first child should be closed, and the second
    //   should be preserved, and vice-versa for false.
    void ParentPane::_CloseChild(const bool closeFirst)
    {
        // The closed child must always be a leaf.
        const auto closedChild = (closeFirst ? _firstChild.try_as<TerminalApp::LeafPane>() : _secondChild.try_as<TerminalApp::LeafPane>());
        THROW_HR_IF_NULL(E_FAIL, closedChild);

        const auto remainingChild = closeFirst ? _secondChild : _firstChild;

        // Detach all the controls from our grid, so they can be attached later.
        Root().Children().Clear();

        const auto closedChildDir = (_splitState == SplitState::Vertical) ?
                                        (closeFirst ? ResizeDirection::Left : ResizeDirection::Right) :
                                        (closeFirst ? ResizeDirection::Up : ResizeDirection::Down);

        if (const auto remainingAsLeaf = remainingChild.try_as<TerminalApp::LeafPane>())
        {
            // If our remaining child is a leaf, tell it to update its border now that its neighbour has closed
            remainingAsLeaf.UpdateBorderWithClosedNeighbor(closedChild, closedChildDir);
        }
        else
        {
            // If our remaining child is a parent, we need to propagate the update border call to all leaves that shared
            // an edge with the closed child
            const auto remainingAsParent = remainingChild.try_as<ParentPane>();
            remainingAsParent->PropagateToLeavesOnEdge(closedChildDir, [&](TerminalApp::LeafPane paneOnEdge) {
                paneOnEdge.UpdateBorderWithClosedNeighbor(closedChild, closedChildDir);
            });
        }

        // If the closed child was last active, make sure to set a leaf in our remaining child
        // as last active before we collapse (there should always be exactly 1 active leaf)
        if (closedChild.WasLastFocused())
        {
            closedChild.ClearActive();
            auto& remainingChild = closeFirst ? _secondChild : _firstChild;
            remainingChild.FindFirstLeaf().try_as<TerminalApp::LeafPane>().SetActive();
        }

        // Make sure to only fire off this event _after_ we have set the new active pane, because this event
        // might cause the tab content to change which will fire off a property changed event which eventually
        // results in the tab trying to access the active terminal control, which requires a valid active pane
        _PaneTypeChangedHandlers(nullptr, remainingChild);
    }

    winrt::fire_and_forget ParentPane::_CloseChildRoutine(const bool closeFirst)
    {
        auto weakThis = get_weak();
        co_await winrt::resume_foreground(Root().Dispatcher());

        if (auto pane{ weakThis.get() })
        {
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
            Root().Children().Clear();
            // Add the remaining child back to the grid, in the right place.
            Root().Children().Append(remainingChild.try_as<FrameworkElement>());
            if (_splitState == SplitState::Vertical)
            {
                Controls::Grid::SetColumn(remainingChild.try_as<FrameworkElement>(), closeFirst ? 1 : 0);
            }
            else if (_splitState == SplitState::Horizontal)
            {
                Controls::Grid::SetRow(remainingChild.try_as<FrameworkElement>(), closeFirst ? 1 : 0);
            }

            // Create the dummy grid. This grid will be the one we actually animate,
            // in the place of the closed pane.
            Controls::Grid dummyGrid;

            dummyGrid.Background(s_unfocusedBorderBrush);

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

            auto strongThis{ get_strong() };
            // When the animation is completed, reparent the child's content up to
            // us, and remove the child nodes from the tree.
            animation.Completed([=](auto&&, auto&&) {
                // We don't need to manually undo any of the above trickiness.
                // We're going to re-parent the child's content into us anyways
                strongThis->_CloseChild(closeFirst);
            });
        }
    }

    // Method Description:
    // - Adds event handlers to our children
    // - For child leaves, we handle their Closed event
    // - For all children, we listen to their type changed events
    void ParentPane::_SetupChildEventHandlers(const bool isFirstChild)
    {
        auto& child = isFirstChild ? _firstChild : _secondChild;
        auto& closedToken = isFirstChild ? _firstClosedToken : _secondClosedToken;
        auto& typeChangedToken = isFirstChild ? _firstTypeChangedToken : _secondTypeChangedToken;

        if (const auto childAsLeaf = child.try_as<TerminalApp::LeafPane>())
        {
            // When our child is a leaf and got closed, we close it
            const auto childImpl = winrt::get_self<implementation::LeafPane>(child);

            closedToken = childImpl->Closed([=](auto&& /*s*/, auto&& /*a*/) {
                // Unsubscribe from events of both our children, as we ourself will also
                // get closed when our child does.
                _RemoveAllChildEventHandlers(false);
                _RemoveAllChildEventHandlers(true);
                _CloseChildRoutine(isFirstChild);
            });
        }

        // When our child is a leaf and gets split, it produces a new parent pane that contains
        // both itself and its new leaf neighbor. We then replace that child with the new parent pane.

        // When our child is a parent and one of its children got closed (and so the parent collapses),
        // we take in its remaining, orphaned child as our own.

        // Either way, the event handling is the same - update the event handlers and update the content
        // of the appropriate root
        typeChangedToken = child.PaneTypeChanged([=](auto&& /*s*/, IPane newPane) {
            _OnChildSplitOrCollapse(isFirstChild, newPane);
        });
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
        }
        child.PaneTypeChanged(typeChangedToken);
    }

    void ParentPane::_OnChildSplitOrCollapse(const bool isFirstChild, IPane newChild)
    {
        // Unsubscribe from all the events of the child.
        _RemoveAllChildEventHandlers(isFirstChild);

        // check whether we need to move focus to the newChild after
        // we are done modifying the UI tree
        bool moveFocusAfter{ false };
        const auto childToReplace = isFirstChild ? _firstChild : _secondChild;

        // we only need to move the focus if a parent pane is collapsing and
        // one of its leaves had focus
        if (childToReplace.try_as<TerminalApp::ParentPane>())
        {
            moveFocusAfter = childToReplace.HasFocusedChild();
        }

        (isFirstChild ? _firstChild : _secondChild) = newChild;

        const auto remainingChild = isFirstChild ? _secondChild : _firstChild;

        Root().Children().Clear();
        _GridLayoutHelper();
        Root().Children().Append(_firstChild.try_as<FrameworkElement>());
        Root().Children().Append(_secondChild.try_as<FrameworkElement>());

        // Setup events appropriate for the new child
        _SetupChildEventHandlers(isFirstChild);

        if (moveFocusAfter)
        {
            newChild.FocusFirstChild();
        }
    }

    void ParentPane::_GridLayoutHelper() const noexcept
    {
        Controls::Grid::SetColumn(_firstChild.try_as<FrameworkElement>(), 0);
        Controls::Grid::SetRow(_firstChild.try_as<FrameworkElement>(), 0);

        Controls::Grid::SetColumn(_secondChild.try_as<FrameworkElement>(), _splitState == SplitState::Vertical ? 1 : 0);
        Controls::Grid::SetRow(_secondChild.try_as<FrameworkElement>(), _splitState == SplitState::Horizontal ? 1 : 0);
    }

    // Method Description:
    // - Gets the size in pixels of each of our children, given the full size they
    //   should fill. Since these children own their own separators (borders), this
    //   size is their portion of our _entire_ size. If specified size is lower than
    //   required then children will be of minimum size. Snaps first child to grid
    //   but not the second.
    // Arguments:
    // - fullSize: the amount of space in pixels that should be filled by our
    //   children and their separators. Can be arbitrarily low.
    // Return Value:
    // - a pair with the size of our first child and the size of our second child,
    //   respectively.
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

    // Method Description:
    // - Gets the size in pixels of each of our children, given the full size they should
    //   fill. Each child is snapped to char grid as close as possible. If called multiple
    //   times with fullSize argument growing, then both returned sizes are guaranteed to be
    //   non-decreasing (it's a monotonically increasing function). This is important so that
    //   user doesn't get any pane shrank when they actually expand the window or parent pane.
    //   That is also required by the layout algorithm.
    // Arguments:
    // - widthOrHeight: if true, operates on width, otherwise on height.
    // - fullSize: the amount of space in pixels that should be filled by our children and
    //   their separator. Can be arbitrarily low.
    // Return Value:
    // - a structure holding the result of this calculation. The 'lower' field represents the
    //   children sizes that would fit in the fullSize, but might (and usually do) not fill it
    //   completely. The 'higher' field represents the size of the children if they slightly exceed
    //   the fullSize, but are snapped. If the children can be snapped and also exactly match
    //   the fullSize, then both this fields have the same value that represent this situation.
    ParentPane::SnapChildrenSizeResult ParentPane::_CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const
    {
        auto sizeTree = _CreateMinSizeTree(widthOrHeight);
        LayoutSizeNode lastSizeTree{ sizeTree };

        while (sizeTree.size < fullSize)
        {
            lastSizeTree = sizeTree;
            _AdvanceSnappedDimension(widthOrHeight, sizeTree);

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

    // Method Description:
    // - Adjusts given dimension (width or height) so that all descendant terminals
    //   align with their character grids as close as possible. Also makes sure to
    //   fit in minimal sizes of the panes.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // - dimension: a dimension (width or height) to be snapped
    // Return Value:
    // - pair of floats, where first value is the size snapped downward (not greater then
    //   requested size) and second is the size snapped upward (not lower than requested size).
    //   If requested size is already snapped, then both returned values equal this value.
    SnapSizeResult ParentPane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        if (_splitState == (widthOrHeight ? SplitState::Horizontal : SplitState::Vertical))
        {
            // If we're resizing along separator axis, snap to the closest possibility
            // given by our children panes.

            const auto firstSnapped = _firstChild.CalcSnappedDimension(widthOrHeight, dimension);
            const auto secondSnapped = _secondChild.CalcSnappedDimension(widthOrHeight, dimension);
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

    // Method Description:
    // - Increases size of given LayoutSizeNode to match next possible 'snap'. In case of leaf
    //   pane this means the next cell of the terminal. Otherwise it means that one of its children
    //   advances (recursively). It expects the given node and its descendants to have either
    //   already snapped or minimum size.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height.
    // - sizeNode: a layout size node that corresponds to this pane.
    void ParentPane::_AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
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
                    sizeNode.nextFirstChild->size = _firstChild.CalcSnappedDimension(widthOrHeight, sizeNode.nextFirstChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = firstChildAsLeaf.TerminalControl().CharacterDimensions();
                    sizeNode.nextFirstChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
                firstChildAsParentImpl->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
            }
        }
        if (sizeNode.nextSecondChild == nullptr)
        {
            sizeNode.nextSecondChild = std::make_unique<LayoutSizeNode>(*sizeNode.secondChild);

            if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextSecondChild->isMinimumSize)
                {
                    sizeNode.nextSecondChild->size = _secondChild.CalcSnappedDimension(widthOrHeight, sizeNode.nextSecondChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = secondChildAsLeaf.TerminalControl().CharacterDimensions();
                    sizeNode.nextSecondChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
                secondChildAsParentImpl->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
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
            if (auto firstChildAsLeaf = _firstChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextFirstChild->isMinimumSize)
                {
                    sizeNode.nextFirstChild->size = _firstChild.CalcSnappedDimension(widthOrHeight, sizeNode.nextFirstChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = firstChildAsLeaf.TerminalControl().CharacterDimensions();
                    sizeNode.nextFirstChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto firstChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_firstChild);
                firstChildAsParentImpl->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextFirstChild);
            }
        }
        else
        {
            *sizeNode.secondChild = *sizeNode.nextSecondChild;
            if (auto secondChildAsLeaf = _secondChild.try_as<TerminalApp::LeafPane>())
            {
                if (sizeNode.nextSecondChild->isMinimumSize)
                {
                    sizeNode.nextSecondChild->size = _secondChild.CalcSnappedDimension(widthOrHeight, sizeNode.nextSecondChild->size + 1).higher;
                }
                else
                {
                    const auto cellSize = secondChildAsLeaf.TerminalControl().CharacterDimensions();
                    sizeNode.nextSecondChild->size += widthOrHeight ? cellSize.Width : cellSize.Height;
                }
            }
            else
            {
                auto secondChildAsParentImpl = winrt::get_self<implementation::ParentPane>(_secondChild);
                secondChildAsParentImpl->_AdvanceSnappedDimension(widthOrHeight, *sizeNode.nextSecondChild);
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

    // Function Description:
    // - Attempts to load some XAML resources that the Pane will need. This includes:
    //   * The Color we'll use for active Panes's borders - SystemAccentColor
    //   * The Brush we'll use for inactive Panes - TabViewBackground (to match the
    //     color of the titlebar)
    void ParentPane::_SetupResources()
    {
        const auto res = Application::Current().Resources();
        const auto tabViewBackgroundKey = winrt::box_value(L"TabViewBackground");
        if (res.HasKey(tabViewBackgroundKey))
        {
            winrt::Windows::Foundation::IInspectable obj = res.Lookup(tabViewBackgroundKey);
            s_unfocusedBorderBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            // DON'T use Transparent here - if it's "Transparent", then it won't
            // be able to hittest for clicks, and then clicking on the border
            // will eat focus.
            s_unfocusedBorderBrush = SolidColorBrush{ Colors::Black() };
        }
    }

    // Method Description:
    // - Builds a tree of LayoutSizeNode that matches the tree of panes. Each node
    //   has minimum size that the corresponding pane can have.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // Return Value:
    // - Root node of built tree that matches this pane.
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

    // Method Description:
    // - Adjusts split position so that no child pane is smaller then its
    //   minimum size
    // Arguments:
    // - widthOrHeight: if true, operates on width, otherwise on height.
    // - requestedValue: split position value to be clamped
    // - totalSize: size (width or height) of the parent pane
    // Return Value:
    // - split position (value in range <0.0, 1.0>)
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
}
