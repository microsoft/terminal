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

    LeafPane* LeafPane::GetActivePane()
    {
        return _lastActive ? this : nullptr;
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

    std::pair<IPane, IPane> LeafPane::Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                            const float /*splitSize*/,
                                            const GUID& profile,
                                            const winrt::Microsoft::Terminal::TerminalControl::TermControl& control)
    {
        splitType = _convertAutomaticSplitState(splitType);
        auto newNeighbour = LeafPane(profile, control, false);

        // Update the border of this pane and set appropriate border for the new leaf pane.
        if (splitType == SplitState::Vertical)
        {
            //newNeighbour->_borders = _borders | Borders::Left;
            _borders = _borders | Borders::Right;
        }
        else
        {
            //newNeighbour->_borders = _borders | Borders::Top;
            _borders = _borders | Borders::Bottom;
        }

        _UpdateBorders();
        newNeighbour._UpdateBorders();

        if (WasLastFocused())
        {
            ClearActive();
            newNeighbour.ClearActive();
        }

        // Parent pane has to know it's size when creating, which will just be the size of ours.
        Size actualSize{ gsl::narrow_cast<float>(Root().ActualWidth()),
                         gsl::narrow_cast<float>(Root().ActualHeight()) };
        //const auto newParent = std::make_shared<ParentPane>(this, newNeighbour, splitType, Half, actualSize);

        //_SplittedHandlers(newParent);

        // Call InitializeChildren after invoking Splitted handlers, because that is were the we are detached and
        // the new parent is attached to xaml view. Only when we are detached can the new parent actually attach us.
        //newParent->InitializeChildren();

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

    Size LeafPane::GetMinSize() const
    {
        auto controlSize = _control.MinimumSize();
        auto newWidth = controlSize.Width;
        auto newHeight = controlSize.Height;

        newWidth += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
        newWidth += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
        newHeight += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;

        return { newWidth, newHeight };
    }

    LeafPane* LeafPane::FindFirstLeaf()
    {
        return this;
    }

    void LeafPane::PropagateToLeaves(std::function<void(LeafPane&)> action)
    {
        action(*this);
    }

    void LeafPane::PropagateToLeavesOnEdge(const ResizeDirection& /* edge */, std::function<void(LeafPane&)> action)
    {
        action(*this);
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
                //_PaneRaiseVisualBellHandlers(nullptr);
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
            if (WI_IsFlagSet(_borders, Borders::Top))
            {
                top = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders::Bottom))
            {
                bottom = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders::Left))
            {
                left = PaneBorderSize;
            }
            if (WI_IsFlagSet(_borders, Borders::Right))
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

    LeafPane::SnapSizeResult LeafPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
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
            lower += WI_IsFlagSet(_borders, Borders::Left) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders::Right) ? PaneBorderSize : 0;
        }
        else
        {
            lower += WI_IsFlagSet(_borders, Borders::Top) ? PaneBorderSize : 0;
            lower += WI_IsFlagSet(_borders, Borders::Bottom) ? PaneBorderSize : 0;
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
}
