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
        GridBorder().Child(_control);

        _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &LeafPane::_ControlConnectionStateChangedHandler });
        _warningBellToken = _control.WarningBell({ this, &LeafPane::_ControlWarningBellHandler });

        // On the Pane's creation, lookup resources we'll use to theme the
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
        FocusFirstChild();
        e.Handled(true);
    }

    // Method Description:
    // - If this is the last focused pane, returns itself
    // - This Pane's control might not currently be focused, if the tab itself is
    //   not currently focused
    // Return Value:
    // - nullptr if we're unfocused, else returns this
    IPane LeafPane::GetActivePane()
    {
        return _lastActive ? IPane{ *this } : nullptr;
    }

    IPane LeafPane::FindFirstLeaf()
    {
        return IPane{ *this };
    }

    // Method Description:
    // - Gets the TermControl of this pane
    // Return Value:
    // - The TermControl of this LeafPane
    TermControl LeafPane::GetTerminalControl()
    {
        return _control;
    }

    GUID LeafPane::GetProfile()
    {
        return _profile;
    }

    // Method Description:
    // - Get the root UIElement of this pane, which in our case just contains a border
    //   with a Terminal Control in it
    // Return Value:
    // - the Grid acting as the root of this pane.
    Controls::Grid LeafPane::GetRootElement()
    {
        return Root();
    }

    // Method Description:
    // - Returns true if this pane was the last pane to be focused in a tree of panes.
    // Return Value:
    // - true iff we were the last pane focused in this tree of panes.
    bool LeafPane::WasLastFocused() const noexcept
    {
        return _lastActive;
    }

    // Method Description:
    // - Update the focus state of this pane. We'll make sure to colorize our
    //   borders depending on if we are the active pane or not.
    void LeafPane::UpdateVisuals()
    {
        GridBorder().BorderBrush(_lastActive ? s_focusedBorderBrush : s_unfocusedBorderBrush);
    }

    // Method Description:
    // - Remove the "Active" state from this Pane
    // - Updates our visuals to match our new state, including highlighting our borders
    void LeafPane::ClearActive()
    {
        _lastActive = false;
        UpdateVisuals();
    }

    // Method Description:
    // - Sets the "Active" state on this Pane. Only one Pane in a tree of Panes
    //   should be "active"
    // - Updates our visuals to match our new state, including highlighting our borders.
    void LeafPane::SetActive()
    {
        _lastActive = true;
        UpdateVisuals();
    }

    // Method Description:
    // - Updates the settings of this pane if our profile guid matches the parameter
    // Arguments:
    // - settings: The new TerminalSettings to apply to any matching controls
    // - profile: The GUID of the profile these settings should apply to.
    void LeafPane::UpdateSettings(const TerminalApp::TerminalSettings& settings, const GUID& profile)
    {
        if (profile == _profile)
        {
            _control.UpdateSettings(settings);
        }
    }

    // Method Description:
    // - Focuses this pane if the given id matches ours
    // Arguments:
    // - The ID of the pane we want to focus
    void LeafPane::FocusPane(uint32_t id)
    {
        if (_id == id)
        {
            _control.Focus(FocusState::Programmatic);
        }
    }

    // Method Description:
    // - Focuses this control
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

    // Method Description:
    // - Returns true if this pane is currently focused
    // Return Value:
    // - true if the currently focused pane is this pane
    bool LeafPane::HasFocusedChild()
    {
        return _control && _lastActive;
    }

    // Method Description:
    // - Splits this pane, creating a new leaf pane and a parent pane
    // - The parent pane holds this pane and the newly created neighbour
    // - Emits an event with the new parent, so that whoever is listening
    //   will replace us with our parent
    // Arguments:
    // - splitType: what type of split we want to create.
    // - profile: The profile GUID to associate with the newly created pane.
    // - control: A TermControl to use in the new pane.
    // Return Value:
    // - The newly created pane
    TerminalApp::LeafPane LeafPane::Split(winrt::Microsoft::Terminal::Settings::Model::SplitState splitType,
                                          const float splitSize,
                                          const GUID& profile,
                                          const winrt::Microsoft::Terminal::TerminalControl::TermControl& control)
    {
        splitType = _convertAutomaticSplitState(splitType);
        auto newNeighbour = TerminalApp::LeafPane(profile, control, false);

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

        const auto newParent = TerminalApp::ParentPane(*this, newNeighbour, splitType, 1.0f - splitSize);

        _GotSplitHandlers(newParent);

        // Call InitializeChildren after invoking GotSplit handlers, because that is where we are detached and
        // the new parent is attached to xaml view. Only when we are detached can the new parent actually attach us.
        newParent.InitializeChildren();

        return newNeighbour;
    }

    // Method Description:
    // - Adjusts given dimension (width or height) so that we align with our character grid
    //   as close as possible. Snaps to closes match (either upward or downward). Also makes
    //   sure to fit in minimal sizes of the panes.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // - dimension: a dimension (width or height) to snap
    // Return Value:
    // - A value corresponding to the next closest snap size for this Pane, either upward or downward
    float LeafPane::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        const auto [lower, higher] = _CalcSnappedDimension(widthOrHeight, dimension);
        return dimension - lower < higher - dimension ? lower : higher;
    }

    // Method Description:
    // - Closes our attached control, preparing us to be removed from the UI tree
    void LeafPane::Shutdown()
    {
        _control.Close();
    }

    // Method Description:
    // - Fire our Closed event to tell our parent that we should be removed.
    // - todo: not sure if we ever call this anymore
    void LeafPane::Close()
    {
        _ClosedHandlers(*this);
    }

    int LeafPane::GetLeafPaneCount() const noexcept
    {
        return 1;
    }

    // Method Description:
    // - Retrieves the ID of this pane
    // Return Value:
    // - The ID of this pane
    uint16_t LeafPane::Id() noexcept
    {
        return _id;
    }

    // Method Description:
    // - Sets this pane's ID
    // - Panes are given IDs upon creation by TerminalTab
    // Arguments:
    // - The number to set this pane's ID to
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

    // Method Description:
    // - If this is the pane the caller wishes to zoom, we set our zoomed flag
    //   and update our borders
    // Arguments:
    // - zoomedPane: This is the pane which we're attempting to zoom on.
    void LeafPane::Maximize(IPane paneToZoom)
    {
        _zoomed = (paneToZoom == *this);
        _UpdateBorders();
    }

    void LeafPane::Restore(IPane /*paneToUnzoom*/)
    {
        _zoomed = false;
        _UpdateBorders();
    }

    // Method Description:
    // - Get the absolute minimum size that this pane can be resized to and still
    //   have 1x1 character visible. Since we're a leaf, we'll
    //   include the space needed for borders _within_ us.
    // Return Value:
    // - The minimum size that this pane can be resized to and still have a visible
    //   character.
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
    //   * If this pane is the pane we're looking for, use the
    //     available space to calculate which direction to split in.
    //   * If this pane is _any other leaf_, then just return nullopt, to indicate
    //     that the `target` Pane is not down this branch.
    // Arguments:
    // - target: The Pane we're attempting to split.
    // - availableSpace: The theoretical space that's available for this pane to be able to split.
    // Return Value:
    // - nullopt if `target` is not this pane, otherwise the
    //   SplitState that `target` would use for an `Automatic` split given
    //   `availableSpace`
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
    // - If this pane is the pane we're looking for, use the
    //   available space to calculate which direction to split in.
    // - If this pane is _any other leaf_, then just return nullopt, to indicate
    //   that the `target` Pane is not down this branch.
    // Arguments:
    // - target: The Pane we're attempting to split.
    // - splitType: The direction we're attempting to split in.
    // - availableSpace: The theoretical space that's available for this pane to be able to split.
    // Return Value:
    // - nullopt if `target` is not this pane, otherwise
    //   true iff we could split this pane, given `availableSpace`
    // Note:
    // - This method is highly similar to Pane::PreCalculateAutoSplit
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
        // Set the border on the side we shared to the same state that the neighbour had.
        switch (neighborDirection)
        {
        case ResizeDirection::Up:
            WI_UpdateFlag(_borders, Borders2::Top, WI_IsFlagSet(closedNeighbor.Borders(), Borders2::Top));
            break;
        case ResizeDirection::Down:
            WI_UpdateFlag(_borders, Borders2::Bottom, WI_IsFlagSet(closedNeighbor.Borders(), Borders2::Bottom));
            break;
        case ResizeDirection::Left:
            WI_UpdateFlag(_borders, Borders2::Left, WI_IsFlagSet(closedNeighbor.Borders(), Borders2::Left));
            break;
        case ResizeDirection::Right:
            WI_UpdateFlag(_borders, Borders2::Right, WI_IsFlagSet(closedNeighbor.Borders(), Borders2::Right));
            break;
        }

        _UpdateBorders();
    }

    // Method Description:
    // - todo: what does this even do in the new implementation
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

    // Method Description:
    // - Plays a warning note when triggered by the BEL control character,
    //   using the sound configured for the "Critical Stop" system event.`
    //   This matches the behavior of the Windows Console host.
    // - Will also flash the taskbar if the bellStyle setting for this profile
    //   has the 'visual' flag set
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

    // Event Description:
    // - todo: do we need this anymore? the hosting tab doesn't listen to all leaves anymore (only the root)
    void LeafPane::_ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                           RoutedEventArgs const& /* args */)
    {
        _GotFocusHandlers(*this);
    }

    // Method Description:
    // - Sets the thickness of each side of our borders to match our _borders state.
    void LeafPane::_UpdateBorders()
    {
        double top = 0, bottom = 0, left = 0, right = 0;

        Thickness newBorders{ 0 };
        if (_zoomed)
        {
            // When the pane is zoomed, manually show all the borders around the window.
            top = bottom = right = left = PaneBorderSize;
        }
        else
        {
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
        }
        GridBorder().BorderThickness(ThicknessHelper::FromLengths(left, top, right, bottom));
    }

    // Function Description:
    // - Attempts to load some XAML resources that the Pane will need. This includes:
    //   * The Color we'll use for active Panes's borders - SystemAccentColor
    //   * The Brush we'll use for inactive Panes - TabViewBackground (to match the
    //     color of the titlebar)
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

    // Method Description:
    // - Adjusts given dimension (width or height) so that we align with our character grid
    //   as close as possible. Also makes sure to fit in minimal sizes of the pane
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // - dimension: a dimension (width or height) to be snapped
    // Return Value:
    // - pair of floats, where first value is the size snapped downward (not greater then
    //   requested size) and second is the size snapped upward (not lower than requested size).
    //   If requested size is already snapped, then both returned values equal this value.
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

    // Method Description:
    // - Converts an "automatic" split type into either Vertical or Horizontal,
    //   based upon the current dimensions of the Pane.
    // - If any of the other SplitState values are passed in, they're returned
    //   unmodified.
    // Arguments:
    // - splitType: The SplitState to attempt to convert
    // Return Value:
    // - None if splitType was None, otherwise one of Horizontal or Vertical
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

    DEFINE_EVENT(LeafPane, Closed, _ClosedHandlers, winrt::delegate<LeafPane>);
    DEFINE_EVENT(LeafPane, GotFocus, _GotFocusHandlers, winrt::delegate<LeafPane>);
    DEFINE_EVENT(LeafPane, GotSplit, _GotSplitHandlers, winrt::delegate<TerminalApp::ParentPane>);
    DEFINE_EVENT(LeafPane, PaneRaiseVisualBell, _PaneRaiseVisualBellHandlers, winrt::delegate<LeafPane>);
}
