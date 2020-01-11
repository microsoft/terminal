// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "Pane.h"
#include "LeafPane.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

class ParentPane : public Pane, public std::enable_shared_from_this<ParentPane>
{
public:
    ParentPane(std::shared_ptr<LeafPane> firstChild, std::shared_ptr<LeafPane> secondChild, winrt::TerminalApp::SplitState splitState, float splitPosition, winrt::Windows::Foundation::Size currentSize);
    ~ParentPane() override = default;
    ParentPane(const ParentPane&) = delete;
    ParentPane(ParentPane&&) = delete;
    ParentPane& operator=(const ParentPane&) = delete;
    ParentPane& operator=(ParentPane&&) = delete;

    void InitializeChildren();

    std::shared_ptr<LeafPane> FindActivePane() override;
    void PropagateToLeaves(std::function<void(LeafPane&)> action) override;
    void PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& edge,
                                 std::function<void(LeafPane&)> action) override;

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::TerminalSettings& settings,
                        const GUID& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& newSize) override;
    void Relayout() override;

    bool ResizeChild(const winrt::TerminalApp::Direction& direction);
    bool NavigateFocus(const winrt::TerminalApp::Direction& direction);

    DECLARE_EVENT(ChildClosed, _ChildClosedHandlers, winrt::delegate<std::shared_ptr<Pane>>);

private:
    std::shared_ptr<Pane> _firstChild;
    std::shared_ptr<Pane> _secondChild;
    winrt::TerminalApp::SplitState _splitState;
    float _desiredSplitPosition;

    winrt::event_token _firstClosedToken{ 0 };
    winrt::event_token _firstTypeChangedToken{ 0 };
    winrt::event_token _secondClosedToken{ 0 };
    winrt::event_token _secondTypeChangedToken{ 0 };

    void _SetupChildEventHandlers(bool firstChild);
    void _RemoveAllChildEventHandlers(bool firstChild);
    void _CreateRowColDefinitions(const winrt::Windows::Foundation::Size& rootSize);
    std::function<void(winrt::Windows::UI::Xaml::FrameworkElement const&, int32_t)> _GetGridSetColOrRowFunc() const noexcept;

    std::shared_ptr<LeafPane> _FindFirstLeaf() override;
    bool _ResizeChild(const winrt::TerminalApp::Direction& direction);
    bool _NavigateFocus(const winrt::TerminalApp::Direction& direction);
    void _CloseChild(const bool closeFirst);

    std::pair<float, float> _CalcChildrenSizes(const float fullSize) const;
    SnapChildrenSizeResult _CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const;
    SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const override;
    void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const override;
    winrt::Windows::Foundation::Size _GetMinSize() const override;
    LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const override;
    float _ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const;

    // Function Description:
    // - Returns true if the given direction can be used with the given split
    //   type.
    // - This is used for pane resizing (which will need a pane separator
    //   that's perpendicular to the direction to be able to move the separator
    //   in that direction).
    // - Additionally, it will be used for moving focus between panes, which
    //   again happens _across_ a separator.
    // Arguments:
    // - direction: The Direction to compare
    // - splitType: The winrt::TerminalApp::SplitState to compare
    // Return Value:
    // - true iff the direction is perpendicular to the splitType.
    static constexpr bool DirectionMatchesSplit(const winrt::TerminalApp::Direction& direction,
                                                const winrt::TerminalApp::SplitState& splitType)
    {
        if (splitType == winrt::TerminalApp::SplitState::Horizontal)
        {
            return direction == winrt::TerminalApp::Direction::Up ||
                   direction == winrt::TerminalApp::Direction::Down;
        }
        else
        {
            return direction == winrt::TerminalApp::Direction::Left ||
                   direction == winrt::TerminalApp::Direction::Right;
        }
        return false;
    }
};
