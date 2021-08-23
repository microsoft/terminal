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

#include "../../cascadia/inc/cppwinrt_utils.h"
#include "TaskbarState.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
};

namespace winrt::TerminalApp::implementation
{
    struct TerminalTab;
}

enum class Borders : int
{
    None = 0x0,
    Top = 0x1,
    Bottom = 0x2,
    Left = 0x4,
    Right = 0x8
};
DEFINE_ENUM_FLAG_OPERATORS(Borders);

class Pane : public std::enable_shared_from_this<Pane>
{
public:
    Pane(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
         const winrt::Microsoft::Terminal::Control::TermControl& control,
         const bool lastFocused = false);

    std::shared_ptr<Pane> GetActivePane();
    winrt::Microsoft::Terminal::Control::TermControl GetTerminalControl();
    winrt::Microsoft::Terminal::Settings::Model::Profile GetFocusedProfile();

    // Method Description:
    // - If this is a leaf pane, return its profile.
    // - If this is a branch/root pane, return nullptr.
    winrt::Microsoft::Terminal::Settings::Model::Profile GetProfile() const
    {
        return _profile;
    }

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    bool WasLastFocused() const noexcept;
    void UpdateVisuals();
    void ClearActive();
    void SetActive();

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                        const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
    void Relayout();
    bool ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
    std::shared_ptr<Pane> NavigateDirection(const std::shared_ptr<Pane> sourcePane, const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);
    bool SwapPanes(std::shared_ptr<Pane> first, std::shared_ptr<Pane> second);

    std::shared_ptr<Pane> NextPane(const std::shared_ptr<Pane> pane);
    std::shared_ptr<Pane> PreviousPane(const std::shared_ptr<Pane> pane);

    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                  const float splitSize,
                                                                  const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
                                                                  const winrt::Microsoft::Terminal::Control::TermControl& control);
    bool ToggleSplitOrientation();
    float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
    std::optional<winrt::Microsoft::Terminal::Settings::Model::SplitState> PreCalculateAutoSplit(const std::shared_ptr<Pane> target,
                                                                                                 const winrt::Windows::Foundation::Size parentSize) const;
    std::optional<bool> PreCalculateCanSplit(const std::shared_ptr<Pane> target,
                                             winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                             const float splitSize,
                                             const winrt::Windows::Foundation::Size availableSpace) const;
    void Shutdown();
    void Close();

    std::shared_ptr<Pane> AttachPane(std::shared_ptr<Pane> pane,
                                     winrt::Microsoft::Terminal::Settings::Model::SplitState splitType);
    std::shared_ptr<Pane> DetachPane(std::shared_ptr<Pane> pane);

    int GetLeafPaneCount() const noexcept;

    void Maximize(std::shared_ptr<Pane> zoomedPane);
    void Restore(std::shared_ptr<Pane> zoomedPane);

    std::optional<uint32_t> Id() noexcept;
    void Id(uint32_t id) noexcept;
    bool FocusPane(const uint32_t id);
    bool FocusPane(const std::shared_ptr<Pane> pane);
    std::shared_ptr<Pane> FindPane(const uint32_t id);

    bool ContainsReadOnly() const;

    // Method Description:
    // - A helper method for ad-hoc recursion on a pane tree. Walks the pane
    //   tree, calling a predicate on each pane in a depth-first pattern.
    // - If the predicate returns true, recursion is stopped early.
    // Arguments:
    // - f: The function to be applied to each pane.
    // Return Value:
    // - true if the predicate returned true on any pane.
    template<typename F>
    //requires std::predicate<F, std::shared_ptr<Pane>>
    bool WalkTree(F f)
    {
        if (f(shared_from_this()))
        {
            return true;
        }

        if (!_IsLeaf())
        {
            return _firstChild->WalkTree(f) || _secondChild->WalkTree(f);
        }

        return false;
    }

    void CollectTaskbarStates(std::vector<winrt::TerminalApp::TaskbarState>& states);

    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
    DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
    DECLARE_EVENT(LostFocus, _LostFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
    DECLARE_EVENT(PaneRaiseBell, _PaneRaiseBellHandlers, winrt::Windows::Foundation::EventHandler<bool>);
    DECLARE_EVENT(Detached, _PaneDetachedHandlers, winrt::delegate<std::shared_ptr<Pane>>);

private:
    struct PanePoint;
    struct PaneNeighborSearch;
    struct SnapSizeResult;
    struct SnapChildrenSizeResult;
    struct LayoutSizeNode;

    winrt::Windows::UI::Xaml::Controls::Grid _root{};
    winrt::Windows::UI::Xaml::Controls::Border _border{};
    winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
    winrt::Microsoft::Terminal::TerminalConnection::ConnectionState _connectionState{ winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

    std::shared_ptr<Pane> _firstChild{ nullptr };
    std::shared_ptr<Pane> _secondChild{ nullptr };
    winrt::Microsoft::Terminal::Settings::Model::SplitState _splitState{ winrt::Microsoft::Terminal::Settings::Model::SplitState::None };
    float _desiredSplitPosition;

    std::optional<uint32_t> _id;

    bool _lastActive{ false };
    winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
    winrt::event_token _connectionStateChangedToken{ 0 };
    winrt::event_token _firstClosedToken{ 0 };
    winrt::event_token _secondClosedToken{ 0 };
    winrt::event_token _warningBellToken{ 0 };

    winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;
    winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;

    std::shared_mutex _createCloseLock{};

    Borders _borders{ Borders::None };

    bool _zoomed{ false };

    bool _IsLeaf() const noexcept;
    bool _HasFocusedChild() const noexcept;
    void _SetupChildCloseHandlers();

    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> _Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                   const float splitSize,
                                                                   std::shared_ptr<Pane> newPane);

    void _CreateRowColDefinitions();
    void _ApplySplitDefinitions();
    void _SetupEntranceAnimation();
    void _UpdateBorders();
    Borders _GetCommonBorders();

    bool _Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);

    std::shared_ptr<Pane> _FindParentOfPane(const std::shared_ptr<Pane> pane);
    bool _IsAdjacent(const std::shared_ptr<Pane> first, const PanePoint firstOffset, const std::shared_ptr<Pane> second, const PanePoint secondOffset, const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction) const;
    PaneNeighborSearch _FindNeighborForPane(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction,
                                            PaneNeighborSearch searchResult,
                                            const bool focusIsSecondSide,
                                            const PanePoint offset);
    PaneNeighborSearch _FindPaneAndNeighbor(const std::shared_ptr<Pane> sourcePane,
                                            const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction,
                                            const PanePoint offset);

    void _CloseChild(const bool closeFirst);
    winrt::fire_and_forget _CloseChildRoutine(const bool closeFirst);

    void _FocusFirstChild();
    void _ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
    void _ControlWarningBellHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                    winrt::Windows::Foundation::IInspectable const& e);
    void _ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
    void _ControlLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                  winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    std::pair<float, float> _CalcChildrenSizes(const float fullSize) const;
    SnapChildrenSizeResult _CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const;
    SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
    void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const;

    winrt::Windows::Foundation::Size _GetMinSize() const;
    LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;
    float _ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const;

    winrt::Microsoft::Terminal::Settings::Model::SplitState _convertAutomaticSplitState(const winrt::Microsoft::Terminal::Settings::Model::SplitState& splitType) const;

    std::optional<winrt::Microsoft::Terminal::Settings::Model::SplitState> _preCalculateAutoSplit(const std::shared_ptr<Pane> target, const winrt::Windows::Foundation::Size parentSize) const;

    // Function Description:
    // - Returns true if the given direction can be used with the given split
    //   type.
    // - This is used for pane resizing (which will need a pane separator
    //   that's perpendicular to the direction to be able to move the separator
    //   in that direction).
    // - Also used for moving focus between panes, which again happens _across_ a separator.
    // Arguments:
    // - direction: The Direction to compare
    // - splitType: The winrt::TerminalApp::SplitState to compare
    // Return Value:
    // - true iff the direction is perpendicular to the splitType. False for
    //   winrt::TerminalApp::SplitState::None.
    template<typename T>
    static constexpr bool DirectionMatchesSplit(const T& direction,
                                                const winrt::Microsoft::Terminal::Settings::Model::SplitState& splitType)
    {
        if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::None)
        {
            return false;
        }
        else if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::Horizontal)
        {
            return direction == T::Up ||
                   direction == T::Down;
        }
        else if (splitType == winrt::Microsoft::Terminal::Settings::Model::SplitState::Vertical)
        {
            return direction == T::Left ||
                   direction == T::Right;
        }
        return false;
    }

    static void _SetupResources();

    struct PanePoint
    {
        float x;
        float y;
    };

    struct PaneNeighborSearch
    {
        std::shared_ptr<Pane> source;
        std::shared_ptr<Pane> neighbor;
        PanePoint sourceOffset;
    };

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
    // to the pane tree. Used for laying out panes with snapped sizes.
    struct LayoutSizeNode
    {
        float size;
        bool isMinimumSize;
        std::unique_ptr<LayoutSizeNode> firstChild;
        std::unique_ptr<LayoutSizeNode> secondChild;

        // These two fields hold next possible snapped values of firstChild and
        // secondChild. Although that could be calculated from these fields themselves,
        // it would be wasteful as we have to know these values more often than for
        // simple increment. Hence we cache that here.
        std::unique_ptr<LayoutSizeNode> nextFirstChild;
        std::unique_ptr<LayoutSizeNode> nextSecondChild;

        explicit LayoutSizeNode(const float minSize);
        LayoutSizeNode(const LayoutSizeNode& other);

        LayoutSizeNode& operator=(const LayoutSizeNode& other);

    private:
        void _AssignChildNode(std::unique_ptr<LayoutSizeNode>& nodeField, const LayoutSizeNode* const newNode);
    };

    friend struct winrt::TerminalApp::implementation::TerminalTab;
    friend class ::TerminalAppLocalTests::TabTests;
};
