// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - Pane.h
//
// Abstract:
// - Panes are an abstraction by which the terminal can display multiple terminal
//   instances simultaneously in a single terminal window. While tabs allow for
//   a single terminal window to have many terminal sessions running
//   simultaneously within a single window, only one tab can be visible at a
//   time. Panes, on the other hand, allow a user to have many different
//   terminal sessions visible to the user within the context of a single window
//   at the same time. This can enable greater productivity from the user, as
//   they can see the output of one terminal window while working in another.
// - See doc/cascadia/Panes.md for a detailed description.
//
// Author:
// - Mike Griese (zadjii-msft) 16-May-2019

#pragma once
#include <functional>
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

class LeafPane;
class ParentPane;

class Pane
{
public:
    virtual ~Pane() = default;

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement() const;

    // Method Description:
    // - Searches for last focused pane in the pane tree and returns it. Returns
    //   nullptr, if the pane is not last focused and doesn't have any child
    //   that is.
    // - This Pane's control might not currently be focused, if the tab itself is
    //   not currently focused.
    // Return Value:
    // - nullptr if we're a leaf and unfocused, or no children were marked
    //   `_lastActive`, else returns this
    virtual std::shared_ptr<LeafPane> FindActivePane() = 0;

    // Method Description:
    // - Invokes the given action on each descendant leaf pane, which may be just this pane if 
    //   it is a leaf.
    // Arguments:
    // - action: function to invoke on leaves. Parameters:
    //      * pane: the leaf pane on which the action is invoked
    // Return Value:
    // - <none>
    virtual void PropagateToLeaves(std::function<void(LeafPane&)> action) = 0;

    // Method Description:
    // - Invokes the given action on each descendant leaf pane that touches the given edge of this
    //   pane. In case this is a leaf pane, it is assumed that it touches every edge of itself.
    // Arguments:
    // - action: function to invoke on leaves. Parameters:
    //      * pane: the leaf pane on which the action is invoked
    // Return Value:
    // - <none>
    virtual void PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& edge,
                                         std::function<void(LeafPane&)> action) = 0;

    // Method Description:
    // - Attempts to update the settings of this pane or any children of this pane.
    //   * If this pane is a leaf, and our profile guid matches the parameter, then
    //     we'll apply the new settings to our control.
    //   * If we're not a leaf, we'll recurse on our children.
    // Arguments:
    // - settings: The new TerminalSettings to apply to any matching controls
    // - profile: The GUID of the profile these settings should apply to.
    // Return Value:
    // - <none>
    virtual void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings,
                                const GUID& profile) = 0;

    virtual void ResizeContent(const winrt::Windows::Foundation::Size& newSize) = 0;
    virtual void Relayout() = 0;
    float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

protected:
    struct SnapSizeResult;
    struct SnapChildrenSizeResult;
    struct LayoutSizeNode;

    winrt::Windows::UI::Xaml::Controls::Grid _root{};

    Pane();

    // Method Description:
    // - Returns first leaf pane found in the tree.
    // Return Value:
    // - Itself if it's a leaf, or first of its children if it's a parent.
    virtual std::shared_ptr<LeafPane> _FindFirstLeaf() = 0;

    // Method Description:
    // - Get the absolute minimum size that this pane can be resized to and still
    //   have 1x1 character visible, in each of its children. If we're a leaf, we'll
    //   include the space needed for borders _within_ us.
    // Arguments:
    // - <none>
    // Return Value:
    // - The minimum size that this pane can be resized to and still have a visible
    //   character.
    virtual winrt::Windows::Foundation::Size _GetMinSize() const = 0;

    // Method Description:
    // - Builds a tree of LayoutSizeNode that matches the tree of panes. Each node
    //   has minimum size that the corresponding pane can have.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // Return Value:
    // - Root node of built tree, which matches this pane.
    virtual LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;

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
    virtual SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const = 0;

    // Method Description:
    // - Increases size of given LayoutSizeNode to match next possible 'snap'. In case of leaf
    //   pane this means the next cell of the terminal. Otherwise it means that one of its children
    //   advances (recursively). It expects the given node and its descendants to have either
    //   already snapped or minimum size.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height.
    // - sizeNode: a layouting node that corresponds to this pane.
    // Return Value:
    // - <none>
    virtual void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const = 0;

    // Make derived classes friends, so they can access protected members of Pane. C++ for some
    // reason doesn't allow that by default - e.g. LeafPane can access LeafPane::_ProtectedMember,
    // but not Pane::_ProtectedMember.
    friend class LeafPane;
    friend class ParentPane;

    struct SnapSizeResult
    {
        float lower;
        float higher;
    };

    struct SnapChildrenSizeResult
    {
        std::pair<float, float> lower;
        std::pair<float, float> higher;
    };

    // Helper structure that builds a (roughly) binary tree corresponding
    // to the pane tree. Used for layouting panes with snapped sizes.
    struct LayoutSizeNode
    {
        float size;
        bool isMinimumSize;
        std::unique_ptr<LayoutSizeNode> firstChild;
        std::unique_ptr<LayoutSizeNode> secondChild;

        // These two fields hold next possible snapped values of firstChild and
        // secondChild. Although that could be calculated from these fields themself,
        // it would be wasteful as we have to know these values more often than for
        // simple increment. Hence we cache that here.
        std::unique_ptr<LayoutSizeNode> nextFirstChild;
        std::unique_ptr<LayoutSizeNode> nextSecondChild;

        LayoutSizeNode(const float minSize);
        LayoutSizeNode(const LayoutSizeNode& other);

        LayoutSizeNode& operator=(const LayoutSizeNode& other);

    private:
        void _AssignChildNode(std::unique_ptr<LayoutSizeNode>& nodeField, const LayoutSizeNode* const newNode);
    };
};
