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
    Pane(const GUID& profile,
         const winrt::Microsoft::Terminal::Control::TermControl& control,
         const bool lastFocused = false);

    std::shared_ptr<Pane> GetActivePane();
    winrt::Microsoft::Terminal::Control::TermControl GetTerminalControl();
    std::optional<GUID> GetFocusedProfile();

    winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

    bool WasLastFocused() const noexcept;
    void UpdateVisuals();
    void ClearActive();
    void SetActive();

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                        const GUID& profile);
    void ResizeContent(const winrt::Windows::Foundation::Size& newSize);
    void Relayout();
    bool ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
    bool NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);

    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                                                  const float splitSize,
                                                                  const GUID& profile,
                                                                  const winrt::Microsoft::Terminal::Control::TermControl& control);
    float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
    std::optional<winrt::Microsoft::Terminal::Settings::Model::SplitState> PreCalculateAutoSplit(const std::shared_ptr<Pane> target,
                                                                                                 const winrt::Windows::Foundation::Size parentSize) const;
    std::optional<bool> PreCalculateCanSplit(const std::shared_ptr<Pane> target,
                                             winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                             const float splitSize,
                                             const winrt::Windows::Foundation::Size availableSpace) const;
    void Shutdown();
    void Close();

    int GetLeafPaneCount() const noexcept;

    void Maximize(std::shared_ptr<Pane> zoomedPane);
    void Restore(std::shared_ptr<Pane> zoomedPane);

    std::optional<uint16_t> Id() noexcept;
    void Id(uint16_t id) noexcept;
    void FocusPane(const uint16_t id);

    bool ContainsReadOnly() const;

    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);
    DECLARE_EVENT(GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
    DECLARE_EVENT(LostFocus, _LostFocusHandlers, winrt::delegate<std::shared_ptr<Pane>>);
    DECLARE_EVENT(PaneRaiseBell, _PaneRaiseBellHandlers, winrt::Windows::Foundation::EventHandler<bool>);

private:
    struct SnapSizeResult;
    struct SnapChildrenSizeResult;
    struct LayoutSizeNode;

    winrt::Windows::UI::Xaml::Controls::Grid _root{};
    winrt::Windows::UI::Xaml::Controls::Border _border{};
    winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_focusedBorderBrush;
    static winrt::Windows::UI::Xaml::Media::SolidColorBrush s_unfocusedBorderBrush;

    std::shared_ptr<Pane> _firstChild{ nullptr };
    std::shared_ptr<Pane> _secondChild{ nullptr };
    winrt::Microsoft::Terminal::Settings::Model::SplitState _splitState{ winrt::Microsoft::Terminal::Settings::Model::SplitState::None };
    float _desiredSplitPosition;

    std::optional<uint16_t> _id;

    bool _lastActive{ false };
    std::optional<GUID> _profile{ std::nullopt };
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
                                                                   const GUID& profile,
                                                                   const winrt::Microsoft::Terminal::Control::TermControl& control);

    void _CreateRowColDefinitions();
    void _ApplySplitDefinitions();
    void _SetupEntranceAnimation();
    void _UpdateBorders();

    bool _Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
    bool _NavigateFocus(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction);

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
};
