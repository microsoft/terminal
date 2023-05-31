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
    Right = 0x8,
    All = 0xF
};
DEFINE_ENUM_FLAG_OPERATORS(Borders);

enum class SplitState : int
{
    None = 0,
    Horizontal = 1,
    Vertical = 2
};

struct PaneResources
{
    winrt::Windows::UI::Xaml::Media::SolidColorBrush focusedBorderBrush{ nullptr };
    winrt::Windows::UI::Xaml::Media::SolidColorBrush unfocusedBorderBrush{ nullptr };
    winrt::Windows::UI::Xaml::Media::SolidColorBrush broadcastBorderBrush{ nullptr };
};

class Pane : public std::enable_shared_from_this<Pane>
{
public:
    Pane(const winrt::Microsoft::Terminal::Settings::Model::Profile& profile,
         const winrt::Microsoft::Terminal::Control::TermControl& control,
         const bool lastFocused = false);

    Pane(std::shared_ptr<Pane> first,
         std::shared_ptr<Pane> second,
         const SplitState splitType,
         const float splitPosition,
         const bool lastFocused = false);

    std::shared_ptr<Pane> GetActivePane();
    winrt::Microsoft::Terminal::Control::TermControl GetLastFocusedTerminalControl();
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

    struct BuildStartupState
    {
        std::vector<winrt::Microsoft::Terminal::Settings::Model::ActionAndArgs> args;
        std::shared_ptr<Pane> firstPane;
        std::optional<uint32_t> focusedPaneId;
        uint32_t panesCreated;
    };
    BuildStartupState BuildStartupActions(uint32_t currentId, uint32_t nextId, const bool asContent = false, const bool asMovePane = false);
    winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetTerminalArgsForPane(const bool asContent = false) const;

    void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::TerminalSettingsCreateResult& settings,
                        const winrt::Microsoft::Terminal::Settings::Model::Profile& profile);
    bool ResizePane(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);
    std::shared_ptr<Pane> NavigateDirection(const std::shared_ptr<Pane> sourcePane,
                                            const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction,
                                            const std::vector<uint32_t>& mruPanes);
    bool SwapPanes(std::shared_ptr<Pane> first, std::shared_ptr<Pane> second);

    std::shared_ptr<Pane> NextPane(const std::shared_ptr<Pane> pane);
    std::shared_ptr<Pane> PreviousPane(const std::shared_ptr<Pane> pane);

    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> Split(winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType,
                                                                  const float splitSize,
                                                                  std::shared_ptr<Pane> pane);
    bool ToggleSplitOrientation();
    float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
    std::optional<std::optional<winrt::Microsoft::Terminal::Settings::Model::SplitDirection>> PreCalculateCanSplit(const std::shared_ptr<Pane> target,
                                                                                                                   winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType,
                                                                                                                   const float splitSize,
                                                                                                                   const winrt::Windows::Foundation::Size availableSpace) const;
    void Shutdown();
    void Close();

    std::shared_ptr<Pane> AttachPane(std::shared_ptr<Pane> pane,
                                     winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType);
    std::shared_ptr<Pane> DetachPane(std::shared_ptr<Pane> pane);

    int GetLeafPaneCount() const noexcept;

    void Maximize(std::shared_ptr<Pane> zoomedPane);
    void Restore(std::shared_ptr<Pane> zoomedPane);

    std::optional<uint32_t> Id() noexcept;
    void Id(uint32_t id) noexcept;
    bool FocusPane(const uint32_t id);
    bool FocusPane(const std::shared_ptr<Pane> pane);
    std::shared_ptr<Pane> FindPane(const uint32_t id);

    void FinalizeConfigurationGivenDefault();

    bool ContainsReadOnly() const;

    void EnableBroadcast(bool enabled);
    void BroadcastKey(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl, const WORD vkey, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown);
    void BroadcastChar(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl, const wchar_t vkey, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers);
    void BroadcastString(const winrt::Microsoft::Terminal::Control::TermControl& sourceControl, const winrt::hstring& text);

    void UpdateResources(const PaneResources& resources);

    // Method Description:
    // - A helper method for ad-hoc recursion on a pane tree. Walks the pane
    //   tree, calling a function on each pane in a depth-first pattern.
    // - If that function returns void, then it will be called on every pane.
    // - Otherwise, iteration will continue until a value with operator bool
    //   returns true.
    // Arguments:
    // - f: The function to be applied to each pane.
    // Return Value:
    // - The value of the function applied on a Pane
    template<typename F>
    auto WalkTree(F f) -> decltype(f(shared_from_this()))
    {
        using R = std::invoke_result_t<F, std::shared_ptr<Pane>>;
        static constexpr auto IsVoid = std::is_void_v<R>;

        if constexpr (IsVoid)
        {
            f(shared_from_this());
            if (!_IsLeaf())
            {
                _firstChild->WalkTree(f);
                _secondChild->WalkTree(f);
            }
        }
        else
        {
            if (const auto res = f(shared_from_this()))
            {
                return res;
            }

            if (!_IsLeaf())
            {
                if (const auto res = _firstChild->WalkTree(f))
                {
                    return res;
                }
                return _secondChild->WalkTree(f);
            }

            return R{};
        }
    }

    template<typename F>
    std::shared_ptr<Pane> _FindPane(F f)
    {
        return WalkTree([f](const auto& pane) -> std::shared_ptr<Pane> {
            if (f(pane))
            {
                return pane;
            }
            return nullptr;
        });
    }

    void CollectTaskbarStates(std::vector<winrt::TerminalApp::TaskbarState>& states);

    WINRT_CALLBACK(ClosedByParent, winrt::delegate<>);
    WINRT_CALLBACK(Closed, winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>);

    using gotFocusArgs = winrt::delegate<std::shared_ptr<Pane>, winrt::Windows::UI::Xaml::FocusState>;

    WINRT_CALLBACK(GotFocus, gotFocusArgs);
    WINRT_CALLBACK(LostFocus, winrt::delegate<std::shared_ptr<Pane>>);
    WINRT_CALLBACK(PaneRaiseBell, winrt::Windows::Foundation::EventHandler<bool>);
    WINRT_CALLBACK(Detached, winrt::delegate<std::shared_ptr<Pane>>);
    WINRT_CALLBACK(RestartTerminalRequested, winrt::delegate<std::shared_ptr<Pane>>);

private:
    struct PanePoint;
    struct PaneNeighborSearch;
    struct SnapSizeResult;
    struct SnapChildrenSizeResult;
    struct LayoutSizeNode;

    winrt::Windows::UI::Xaml::Controls::Grid _root{};
    winrt::Windows::UI::Xaml::Controls::Border _borderFirst{};
    winrt::Windows::UI::Xaml::Controls::Border _borderSecond{};

    PaneResources _themeResources;

#pragma region Properties that need to be transferred between child / parent panes upon splitting / closing
    std::shared_ptr<Pane> _firstChild{ nullptr };
    std::shared_ptr<Pane> _secondChild{ nullptr };
    SplitState _splitState{ SplitState::None };
    float _desiredSplitPosition;
    winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
    winrt::Microsoft::Terminal::TerminalConnection::ConnectionState _connectionState{ winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::NotConnected };
    winrt::Microsoft::Terminal::Settings::Model::Profile _profile{ nullptr };
    bool _isDefTermSession{ false };
#pragma endregion

    std::optional<uint32_t> _id;
    std::weak_ptr<Pane> _parentChildPath{};

    bool _lastActive{ false };
    winrt::event_token _firstClosedToken{ 0 };
    winrt::event_token _secondClosedToken{ 0 };

    struct ControlEventTokens
    {
        winrt::Microsoft::Terminal::Control::TermControl::ConnectionStateChanged_revoker _ConnectionStateChanged;
        winrt::Microsoft::Terminal::Control::TermControl::WarningBell_revoker _WarningBell;
        winrt::Microsoft::Terminal::Control::TermControl::CloseTerminalRequested_revoker _CloseTerminalRequested;
        winrt::Microsoft::Terminal::Control::TermControl::RestartTerminalRequested_revoker _RestartTerminalRequested;
        winrt::Microsoft::Terminal::Control::TermControl::ReadOnlyChanged_revoker _ReadOnlyChanged;
    } _controlEvents;
    void _setupControlEvents();
    void _removeControlEvents();

    winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;
    winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;

    std::shared_mutex _createCloseLock{};

    Borders _borders{ Borders::None };

    bool _zoomed{ false };
    bool _broadcastEnabled{ false };

    winrt::Windows::Media::Playback::MediaPlayer _bellPlayer{ nullptr };
    bool _bellPlayerCreated{ false };

    bool _IsLeaf() const noexcept;
    bool _HasFocusedChild() const noexcept;
    void _SetupChildCloseHandlers();
    bool _HasChild(const std::shared_ptr<Pane> child);

    std::pair<std::shared_ptr<Pane>, std::shared_ptr<Pane>> _Split(winrt::Microsoft::Terminal::Settings::Model::SplitDirection splitType,
                                                                   const float splitSize,
                                                                   std::shared_ptr<Pane> newPane);

    void _CreateRowColDefinitions();
    void _ApplySplitDefinitions();
    void _SetupEntranceAnimation();
    void _UpdateBorders();
    Borders _GetCommonBorders();
    winrt::Windows::UI::Xaml::Media::SolidColorBrush _ComputeBorderColor();

    bool _Resize(const winrt::Microsoft::Terminal::Settings::Model::ResizeDirection& direction);

    std::shared_ptr<Pane> _FindParentOfPane(const std::shared_ptr<Pane> pane);
    std::pair<PanePoint, PanePoint> _GetOffsetsForPane(const PanePoint parentOffset) const;
    bool _IsAdjacent(const PanePoint firstOffset, const PanePoint secondOffset, const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction) const;
    PaneNeighborSearch _FindNeighborForPane(const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction,
                                            PaneNeighborSearch searchResult,
                                            const bool focusIsSecondSide,
                                            const PanePoint offset);
    PaneNeighborSearch _FindPaneAndNeighbor(const std::shared_ptr<Pane> sourcePane,
                                            const winrt::Microsoft::Terminal::Settings::Model::FocusDirection& direction,
                                            const PanePoint offset);

    void _CloseChild(const bool closeFirst, const bool isDetaching);
    winrt::fire_and_forget _CloseChildRoutine(const bool closeFirst);

    void _Focus();
    void _FocusFirstChild();
    void _ControlConnectionStateChangedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
    void _ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                    const winrt::Windows::Foundation::IInspectable& e);
    void _ControlGotFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                 const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
    void _ControlLostFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                  const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

    void _ControlReadOnlyChangedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& e);

    void _CloseTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);
    void _RestartTerminalRequestedHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::Foundation::IInspectable& /*args*/);

    std::pair<float, float> _CalcChildrenSizes(const float fullSize) const;
    SnapChildrenSizeResult _CalcSnappedChildrenSizes(const bool widthOrHeight, const float fullSize) const;
    SnapSizeResult _CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;
    void _AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const;
    winrt::Windows::Foundation::Size _GetMinSize() const;
    LayoutSizeNode _CreateMinSizeTree(const bool widthOrHeight) const;
    float _ClampSplitPosition(const bool widthOrHeight, const float requestedValue, const float totalSize) const;

    SplitState _convertAutomaticOrDirectionalSplitState(const winrt::Microsoft::Terminal::Settings::Model::SplitDirection& splitType) const;

    winrt::fire_and_forget _playBellSound(winrt::Windows::Foundation::Uri uri);

    // Function Description:
    // - Returns true if the given direction can be used with the given split
    //   type.
    // - This is used for pane resizing (which will need a pane separator
    //   that's perpendicular to the direction to be able to move the separator
    //   in that direction).
    // - Also used for moving focus between panes, which again happens _across_ a separator.
    // Arguments:
    // - direction: The Direction to compare
    // - splitType: The SplitState to compare
    // Return Value:
    // - true iff the direction is perpendicular to the splitType. False for
    //   SplitState::None.
    template<typename T>
    static constexpr bool DirectionMatchesSplit(const T& direction,
                                                const SplitState& splitType)
    {
        if (splitType == SplitState::None)
        {
            return false;
        }
        else if (splitType == SplitState::Horizontal)
        {
            return direction == T::Up ||
                   direction == T::Down;
        }
        else if (splitType == SplitState::Vertical)
        {
            return direction == T::Left ||
                   direction == T::Right;
        }
        return false;
    }

    struct PanePoint
    {
        float x;
        float y;
        float scaleX;
        float scaleY;
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
    };

    friend struct winrt::TerminalApp::implementation::TerminalTab;
    friend class ::TerminalAppLocalTests::TabTests;
};
