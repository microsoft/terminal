// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LeafPane.h"
#include "AppLogic.h"

#include <Mmsystem.h>

#include "LeafPane.g.cpp"

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
using namespace TerminalApp;

namespace winrt::TerminalApp::implementation
{

    winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_focusedBorderBrush = { nullptr };
    winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_unfocusedBorderBrush = { nullptr };

    static constexpr float Half = 0.50f;
    static const int PaneBorderSize = 2;
    static const int CombinedPaneBorderSize = 2 * PaneBorderSize;

    LeafPane::LeafPane()
    {
        InitializeComponent();
    }

    LeafPane::LeafPane(const GUID& profile, const TermControl& control, const bool lastFocused) :
        _control{ control },
        _lastActive{ lastFocused },
        _profile{ profile }
    {
        InitializeComponent();
        //GridBorder().Child(_control);

        _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &LeafPane::_ControlConnectionStateChangedHandler });
        _warningBellToken = _control.WarningBell({ this, &LeafPane::_ControlWarningBellHandler });

        // On the first Pane's creation, lookup resources we'll use to theme the
        // Pane, including the brushed to use for the focused/unfocused border
        // color.
        if (s_focusedBorderBrush == nullptr || s_unfocusedBorderBrush == nullptr)
        {
            _SetupResources();
        }

        // Use the unfocused border color as the pane background, so an actual color
        // appears behind panes as we animate them sliding in.
        Root().Background(s_unfocusedBorderBrush);

        // Register an event with the control to have it inform us when it gains focus.
        _gotFocusRevoker = control.GotFocus(winrt::auto_revoke, { this, &LeafPane::_ControlGotFocusHandler });
    }

    void LeafPane::BorderTappedHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                       winrt::Windows::UI::Xaml::Input::TappedRoutedEventArgs const& e)
    {
        SetActive();
        e.Handled(true);
    }

    IPane LeafPane::GetActivePane()
    {
        return _lastActive ? IPane{ *this } : nullptr;
    }

    IPane LeafPane::FindFirstLeaf()
    {
        return IPane{ *this };
    }

    TermControl LeafPane::GetTerminalControl()
    {
        return _control;
    }

    GUID LeafPane::GetProfile()
    {
        return _profile;
    }

    Controls::Grid LeafPane::GetRootElement()
    {
        return Root();
    }

    bool LeafPane::WasLastFocused() const noexcept
    {
        return _lastActive;
    }

    void LeafPane::UpdateVisuals()
    {
        GridBorder().BorderBrush(_lastActive ? s_focusedBorderBrush : s_unfocusedBorderBrush);
    }

    void LeafPane::ClearActive()
    {
        _lastActive = false;
        UpdateVisuals();
    }

    void LeafPane::SetActive()
    {
        _lastActive = true;
        UpdateVisuals();
    }

    void LeafPane::UpdateSettings(const TerminalApp::TerminalSettings& settings, const GUID& profile)
    {
        if (profile == _profile)
        {
            _control.UpdateSettings(settings);
        }
    }

    void LeafPane::FocusPane(uint32_t id)
    {
        if (_id == id)
        {
            _control.Focus(FocusState::Programmatic);
        }
    }

    void LeafPane::FocusFirstChild()
    {
        if (Root().ActualWidth() == 0 && Root().ActualHeight() == 0)
        {
            // When these sizes are 0, then the pane might still be in startup,
            // and doesn't yet have a real size. In that case, the control.Focus
            // event won't be handled until _after_ the startup events are all
            // processed. This will lead to the Tab not being notified that the
            // focus moved to a different Pane.
            //
            // In that scenario, trigger the event manually here, to correctly
            // inform the Tab that we're now focused.
            _GotFocusHandlers(*this);
        }

        _control.Focus(FocusState::Programmatic);
    }

    bool LeafPane::HasFocusedChild()
    {
        return _control && _lastActive;
    }

    std::pair<IPane, IPane> LeafPane::Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                            const float splitSize,
                                            const GUID& profile,
                                            const winrt::Microsoft::Terminal::TerminalControl::TermControl& control)
    {
        splitType = _convertAutomaticSplitState(splitType);
        auto newNeighbour = LeafPane(profile, control, false);

        // Update the border of this pane and set appropriate border for the new leaf pane.
        if (splitType == SplitState::Vertical)
        {
            newNeighbour.Borders(_borders | Borders2::Left);
            _borders = _borders | Borders2::Right;
        }
        else
        {
            newNeighbour.Borders(_borders | Borders2::Top);
            _borders = _borders | Borders2::Bottom;
        }

        _UpdateBorders();
        newNeighbour._UpdateBorders();

        if (WasLastFocused())
        {
            ClearActive();
            newNeighbour.SetActive();
        }

        // Parent pane has to know it's size when creating, which will just be the size of ours.
        Size actualSize{ gsl::narrow_cast<float>(Root().ActualWidth()),
                         gsl::narrow_cast<float>(Root().ActualHeight()) };

        const auto newParent = TerminalApp::ParentPane(*this, newNeighbour, splitType, 1.0f - splitSize, actualSize);

        _GotSplitHandlers(newParent);

        // Call InitializeChildren after invoking GotSplit handlers, because that is where we are detached and
        // the new parent is attached to xaml view. Only when we are detached can the new parent actually attach us.
        newParent.InitializeChildren();

        return { *this, newNeighbour };
    }

    float LeafPane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
        return dimension - lower < higher - dimension ? lower : higher;
    }

    void LeafPane::Shutdown()
    {
        _control.Close();
    }

    void LeafPane::Close()
    {
        _ClosedHandlers(nullptr, nullptr);
    }

    int LeafPane::GetLeafPaneCount() const noexcept
    {
        return 1;
    }

    uint16_t LeafPane::Id() noexcept
    {
        return _id;
    }

    void LeafPane::Id(uint16_t id) noexcept
    {
        _id = id;
    }

    Borders2 LeafPane::Borders() noexcept
    {
        return _borders;
    }

    void LeafPane::Borders(Borders2 borders) noexcept
    {
        _borders = borders;
    }

    Size LeafPane::GetMinSize() const
    {
        auto controlSize = _control.MinimumSize();
        auto newWidth = controlSize.Width;
        auto newHeight = controlSize.Height;

        newWidth += WI_IsFlagSet(_borders, Borders2::Left) ? PaneBorderSize : 0;
        newWidth += WI_IsFlagSet(_borders, Borders2::Right) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders2::Top) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders2::Bottom) ? PaneBorderSize : 0;

        return { newWidth, newHeight };
    }

    IReference<SplitState> LeafPane::PreCalculateAutoSplit(const IPane target,
                                                           const winrt::Windows::Foundation::Size availableSpace) const
    {
        if (winrt::get_self<implementation::LeafPane>(target) == this)
        {
            // Use the available space to calculate which direction to split in.
            return availableSpace.Width > availableSpace.Height ? SplitState::Vertical : SplitState::Horizontal;
        }
        else
        {
            // If this pane is _any other leaf_, then just return nullptr, to
            // indicate that the `target` Pane is not down this branch.
            return nullptr;
        }
    }

    IReference<bool> LeafPane::PreCalculateCanSplit(const IPane target,
                                                       SplitState splitType,
                                                       const float splitSize,
                                                       const winrt::Windows::Foundation::Size availableSpace) const
    {
        if (winrt::get_self<implementation::LeafPane>(target) == this)
        {
            const auto firstPrecent = 1.0f - splitSize;
            const auto secondPercent = splitSize;
            // If this pane is a leaf, and it's the pane we're looking for, use
            // the available space to calculate which direction to split in.
            const Size minSize = GetMinSize();

            if (splitType == SplitState::None)
            {
                return { false };
            }
            else if (splitType == SplitState::Vertical)
            {
                const auto widthMinusSeparator = availableSpace.Width - CombinedPaneBorderSize;
                const auto newFirstWidth = widthMinusSeparator * firstPrecent;
                const auto newSecondWidth = widthMinusSeparator * secondPercent;

                return { newFirstWidth > minSize.Width && newSecondWidth > minSize.Width };
            }
            else if (splitType == SplitState::Horizontal)
            {
                const auto heightMinusSeparator = availableSpace.Height - CombinedPaneBorderSize;
                const auto newFirstHeight = heightMinusSeparator * firstPrecent;
                const auto newSecondHeight = heightMinusSeparator * secondPercent;

                return { newFirstHeight > minSize.Height && newSecondHeight > minSize.Height };
            }

            return nullptr;
        }
        else
        {
            // If this pane is _any other leaf_, then just return nullptr, to
            // indicate that the `target` Pane is not down this branch.
            return nullptr;
        }
    }

    void LeafPane::UpdateBorderWithClosedNeighbor(TerminalApp::LeafPane closedNeighbor, const ResizeDirection& neighborDirection)
    {
        // Prepare a mask that includes the only the border which was touching our neighbour.
        Borders2 borderMask;
        switch (neighborDirection)
        {
        case ResizeDirection::Up:
            borderMask = Borders2::Top;
        case ResizeDirection::Down:
            borderMask = Borders2::Bottom;
        case ResizeDirection::Left:
            borderMask = Borders2::Left;
        case ResizeDirection::Right:
            borderMask = Borders2::Right;
        }

        // Set the border on this side to the same state that the neighbour had.
        // todo: why doesn't this call work?
        //WI_UpdateFlagsInMask(_borders, borderMask, closedNeighbor.Borders());

        _UpdateBorders();
    }

    void LeafPane::_ControlConnectionStateChangedHandler(const TermControl& /*sender*/,
                                                         const winrt::Windows::Foundation::IInspectable& /*args*/)
    {
        const auto newConnectionState = _control.ConnectionState();

        if (newConnectionState < ConnectionState::Closed)
        {
            // Pane doesn't care if the connection isn't entering a terminal state.
            return;
        }

        const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
        auto paneProfile = settings.FindProfile(_profile);
        if (paneProfile)
        {
            auto mode = paneProfile.CloseOnExit();
            if ((mode == CloseOnExitMode::Always) ||
                (mode == CloseOnExitMode::Graceful && newConnectionState == ConnectionState::Closed))
            {
                Close();
            }
        }
    }

    void LeafPane::_ControlWarningBellHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                              const winrt::Windows::Foundation::IInspectable& /*eventArgs*/)
    {
        const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
        auto paneProfile = settings.FindProfile(_profile);
        if (paneProfile)
        {
            if (WI_IsFlagSet(paneProfile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Audible))
            {
                const auto soundAlias = reinterpret_cast<LPCTSTR>(SND_ALIAS_SYSTEMHAND);
                PlaySound(soundAlias, NULL, SND_ALIAS_ID | SND_ASYNC | SND_SENTRY);
            }
            if (WI_IsFlagSet(paneProfile.BellStyle(), winrt::Microsoft::Terminal::Settings::Model::BellStyle::Visual))
            {
                // Bubble this event up to app host, starting with bubbling to the hosting tab
                // todo: figure out how to raise this event with nullptr
                // have tried - nullptr, IPane{ nullptr }, LeafPane{ nullptr }
                _PaneRaiseVisualBellHandlers(*this);
            }
        }
    }

    void LeafPane::_ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                           RoutedEventArgs const& /* args */)
    {
        _GotFocusHandlers(*this);
    }

    void LeafPane::_UpdateBorders()
    {
        double top = 0, bottom = 0, left = 0, right = 0;

        Thickness newBorders{ 0 };
        //if (_zoomed)
        //{
        //    // When the pane is zoomed, manually show all the borders around the window.
        //    top = bottom = right = left = PaneBorderSize;
        //}
        //else
        //{
            if (WI_IsFlagSet(_borders, Borders2::Top))
            {
                top = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders2::Bottom))
            {
                bottom = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders2::Left))
            {
                left = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders2::Right))
            {
                right = PaneBorderSize;
            }
        //}
        GridBorder().BorderThickness(ThicknessHelper::FromLengths(left, top, right, bottom));
    }

    void LeafPane::_SetupResources()
    {
        const auto res = Application::Current().Resources();
        const auto accentColorKey = winrt::box_value(L"SystemAccentColor");
        if (res.HasKey(accentColorKey))
        {
            const auto colorFromResources = res.Lookup(accentColorKey);
            // If SystemAccentColor is _not_ a Color for some reason, use
            // Transparent as the color, so we don't do this process again on
            // the next pane (by leaving s_focusedBorderBrush nullptr)
            auto actualColor = winrt::unbox_value_or<Color>(colorFromResources, Colors::Black());
            s_focusedBorderBrush = SolidColorBrush(actualColor);
        }
        else
        {
            // DON'T use Transparent here - if it's "Transparent", then it won't
            // be able to hittest for clicks, and then clicking on the border
            // will eat focus.
            s_focusedBorderBrush = SolidColorBrush{ Colors::Black() };
        }

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

    SnapSizeResult LeafPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        const auto minSize = GetMinSize();
        const auto minDimension = widthOrHeight ? minSize.Width : minSize.Height;

        if (dimension <= minDimension)
        {
            return { minDimension, minDimension };
        }

        float lower = _control.SnapDimensionToGrid(widthOrHeight, dimension);
        if (widthOrHeight)
        {
            lower += WI_IsFlagSet(_borders, Borders2::Left) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders2::Right) ? PaneBorderSize : 0;
        }
        else
        {
            lower += WI_IsFlagSet(_borders, Borders2::Top) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders2::Bottom) ? PaneBorderSize : 0;
        }

        if (lower == dimension)
        {
            // If we happen to be already snapped, then just return this size
            // as both lower and higher values.
            return { lower, lower };
        }
        else
        {
            const auto cellSize = _control.CharacterDimensions();
            const auto higher = lower + (widthOrHeight ? cellSize.Width : cellSize.Height);
            return { lower, higher };
        }
    }

    SplitState LeafPane::_convertAutomaticSplitState(const SplitState& splitType)
    {
        // Careful here! If the pane doesn't yet have a size, these dimensions will
        // be 0, and we'll always return Vertical.

        if (splitType == SplitState::Automatic)
        {
            // If the requested split type was "auto", determine which direction to
            // split based on our current dimensions
            const Size actualSize{ gsl::narrow_cast<float>(Root().ActualWidth()),
                                   gsl::narrow_cast<float>(Root().ActualHeight()) };
            return actualSize.Width >= actualSize.Height ? SplitState::Vertical : SplitState::Horizontal;
        }
        return splitType;
    }

    DEFINE_EVENT(LeafPane, GotFocus, _GotFocusHandlers, winrt::delegate<LeafPane>);
    DEFINE_EVENT(LeafPane, GotSplit, _GotSplitHandlers, winrt::delegate<TerminalApp::ParentPane>);
    DEFINE_EVENT(LeafPane, PaneRaiseVisualBell, _PaneRaiseVisualBellHandlers, winrt::delegate<LeafPane>);
}
