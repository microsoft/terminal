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

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    virtual std::shared_ptr<LeafPane> FindActivePane() = 0;
    virtual void PropagateToLeaves(std::function<void(LeafPane&)> action) = 0;
    virtual void PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& edge,
        std::function<void(LeafPane&)> action) = 0;

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

    virtual std::shared_ptr<LeafPane> _FindFirstLeaf() = 0;

    virtual winrt::Windows::Foundation::Size _GetMinSize() const = 0;
    virtual LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;
    virtual SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const = 0;
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
