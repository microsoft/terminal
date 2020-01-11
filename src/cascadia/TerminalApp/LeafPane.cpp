// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "LeafPane.h"
#include "Profile.h"
#include "CascadiaSettings.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::TerminalApp;
using namespace TerminalApp;

static constexpr int PaneBorderSize = 2;
static constexpr int CombinedPaneBorderSize = 2 * PaneBorderSize;
static constexpr float Half = 0.50f;
winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_focusedBorderBrush = { nullptr };
winrt::Windows::UI::Xaml::Media::SolidColorBrush LeafPane::s_unfocusedBorderBrush = { nullptr };

LeafPane::LeafPane(const GUID& profile, const TermControl& control, const bool lastFocused) :
    _control{ control },
    _lastActive{ lastFocused },
    _profile{ profile }
{
    _root.Children().Append(_border);
    _border.Child(_control);

    _connectionStateChangedToken = _control.ConnectionStateChanged({ this, &LeafPane::_ControlConnectionStateChangedHandler });

    // On the first Pane's creation, lookup resources we'll use to theme the
    // LeafPane, including the brushed to use for the focused/unfocused border
    // color.
    if (s_focusedBorderBrush == nullptr || s_unfocusedBorderBrush == nullptr)
    {
        _SetupResources();
    }

    // Register an event with the control to have it inform us when it gains focus.
    _gotFocusRevoker = control.GotFocus(winrt::auto_revoke, { this, &LeafPane::_ControlGotFocusHandler });

    // When our border is tapped, make sure to transfer focus to our control.
    // LOAD-BEARING: This will NOT work if the border's BorderBrush is set to
    // Colors::Transparent! The border won't get Tapped events, and they'll fall
    // through to something else.
    _border.Tapped([this](auto&, auto& e) {
        SetActive(true);
        e.Handled(true);
    });
}

// Function Description:
// - Attempts to load some XAML resources that the pane will need. This includes:
//   * The Color we'll use for active panes's borders - SystemAccentColor
//   * The Brush we'll use for inactive panes - TabViewBackground (to match the
//     color of the titlebar)
// Arguments:
// - <none>
// Return Value:
// - <none>
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

std::shared_ptr<LeafPane> LeafPane::FindActivePane()
{
    return _lastActive ? shared_from_this() : nullptr;
}

std::shared_ptr<LeafPane> LeafPane::_FindFirstLeaf()
{
    return shared_from_this();
}

void LeafPane::PropagateToLeaves(std::function<void(LeafPane&)> action)
{
    action(*this);
}

void LeafPane::PropagateToLeavesOnEdge(const winrt::TerminalApp::Direction& /* edge */, std::function<void(LeafPane&)> action)
{
    action(*this);
}

void LeafPane::UpdateSettings(const TerminalSettings& settings, const GUID& profile)
{
    if (profile == _profile)
    {
        _control.UpdateSettings(settings);
    }
}

TermControl LeafPane::GetTerminalControl() const noexcept
{
    return _control;
}

GUID LeafPane::GetProfile() const noexcept
{
    return _profile;
}

// Method Description:
// - Determines whether the pane can be split.
// Arguments:
// - splitType: what type of split we want to create.
// Return Value:
// - True if the pane can be split. False otherwise.
bool LeafPane::CanSplit(SplitState splitType)
{
    splitType = _ConvertAutomaticSplitState(splitType);
    const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                           gsl::narrow_cast<float>(_root.ActualHeight()) };

    const Size minSize = _GetMinSize();

    if (splitType == SplitState::Vertical)
    {
        const auto widthMinusSeparator = actualSize.Width - CombinedPaneBorderSize;
        const auto newWidth = widthMinusSeparator * Half;

        return newWidth > minSize.Width;
    }

    if (splitType == SplitState::Horizontal)
    {
        const auto heightMinusSeparator = actualSize.Height - CombinedPaneBorderSize;
        const auto newHeight = heightMinusSeparator * Half;

        return newHeight > minSize.Height;
    }

    return false;
}

// Method Description:
// - Splits this pane, which results in creating a new LeafPane, that is our new
//   sibling and a new ParentPane, which contains both us and the new leaf. W
//   Invokes Splitted handler, in which, whoever owns us should, replace us with
//   this newly created ParentPane.
// Arguments:
// - splitType: what type of split we want to create.
// - profile: The profile GUID to associate with the newly created LeafPane.
// - control: A TermControl that will be placed into the new LeafPane.
// Return Value:
// - Newly created LeafPane that is now our neighbour.
std::shared_ptr<LeafPane> LeafPane::Split(winrt::TerminalApp::SplitState splitType,
                                          const GUID& profile,
                                          const winrt::Microsoft::Terminal::TerminalControl::TermControl& control)
{
    splitType = _ConvertAutomaticSplitState(splitType);
    const auto newNeighbour = std::make_shared<LeafPane>(profile, control);

    // Update the border of this pane and set appropriate border for the new leaf pane.
    if (splitType == SplitState::Vertical)
    {
        newNeighbour->_borders = _borders | Borders::Left;
        _borders = _borders | Borders::Right;
    }
    else
    {
        newNeighbour->_borders = _borders | Borders::Top;
        _borders = _borders | Borders::Bottom;
    }

    _UpdateBorders();
    newNeighbour->_UpdateBorders();

    if (WasLastActive())
    {
        ClearActive();
        newNeighbour->SetActive(false);
    }

    // Parent pane has to know it's size when creating, which will just be the size of ours.
    Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                     gsl::narrow_cast<float>(_root.ActualHeight()) };
    const auto newParent = std::make_shared<ParentPane>(shared_from_this(), newNeighbour, splitType, Half, actualSize);

    _SplittedHandlers(newParent);

    // Call InitializeChildren after invoking Splitted handlers, because that is were the we are detached and
    // the new parent is attached to xaml view. Only when we are detached can the new parent actually attach us.
    newParent->InitializeChildren();

    return newNeighbour;
}

// Method Description:
// - Converts an "automatic" split type into either Vertical or Horizontal,
//   based upon the current dimensions of the pane.
// - If any of the other SplitState values are passed in, they're returned
//   unmodified.
// Arguments:
// - splitType: The SplitState to attempt to convert
// Return Value:
// - One of Horizontal or Vertical
SplitState LeafPane::_ConvertAutomaticSplitState(const SplitState& splitType) const
{
    // Careful here! If the pane doesn't yet have a size, these dimensions will
    // be 0, and we'll always return Vertical.

    if (splitType == SplitState::Automatic)
    {
        // If the requested split type was "auto", determine which direction to
        // split based on our current dimensions
        const Size actualSize{ gsl::narrow_cast<float>(_root.ActualWidth()),
                               gsl::narrow_cast<float>(_root.ActualHeight()) };
        return actualSize.Width >= actualSize.Height ? SplitState::Vertical : SplitState::Horizontal;
    }
    return splitType;
}

// Method Description:
// - Sets the "Active" state on this pane. Only one pane in a tree of panes
//   should be "active".
// - Updates our visuals to match our new state, including highlighting our borders.
// Arguments:
// - <none>
// Return Value:
// - <none>
void LeafPane::SetActive(const bool focusControl)
{
    _lastActive = true;
    if (focusControl)
    {
        _control.Focus(FocusState::Programmatic);
    }
    _UpdateVisuals();
}

// Method Description:
// - Remove the "Active" state from this pane.
// - Updates our visuals to match our new state, including highlighting our borders.
// Arguments:
// - <none>
// Return Value:
// - <none>
void LeafPane::ClearActive()
{
    _lastActive = false;
    _UpdateVisuals();
}

// Method Description:
// - Returns true if this pane was the last pane to be focused in a tree of panes.
// Arguments:
// - <none>
// Return Value:
// - true iff we were the last pane focused in this tree of panes.
bool LeafPane::WasLastActive() const noexcept
{
    return _lastActive;
}

// Method Description:
// - Update the focus state of this pane. We'll make sure to colorize our
//   borders depending on if we are the active pane or not.
// Arguments:
// - <none>
// Return Value:
// - <none>
void LeafPane::_UpdateVisuals()
{
    _border.BorderBrush(_lastActive ? s_focusedBorderBrush : s_unfocusedBorderBrush);
}

// Method Description:
// - Sets the thickness of each side of our borders to match our _borders state.
// Arguments:
// - <none>
// Return Value:
// - <none>
void LeafPane::_UpdateBorders()
{
    double top = 0, bottom = 0, left = 0, right = 0;

    Thickness newBorders{ 0 };
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
    _border.BorderThickness(ThicknessHelper::FromLengths(left, top, right, bottom));
}

// Method Description:
// - Called when we were children of a parent pane and our neighbour pane was closed. This
//   will update border on the side that was touching that neighbour.
// Arguments:
// - closedNeightbour - The sibling leaf pane that was just closed.
// - neightbourDirection - The side at which we were touching that sibling.
// Return Value:
// - <none>
void LeafPane::UpdateBorderWithClosedNeightbour(std::shared_ptr<LeafPane> closedNeightbour, const winrt::TerminalApp::Direction& neightbourDirection)
{
    // Prepare a mask that includes the only the border which was touching our neighbour.
    const auto borderMask = static_cast<Borders>(1 << (static_cast<int>(neightbourDirection) - 1));

    // Set the border on this side to the state that the neighbour had.
    WI_UpdateFlagsInMask(_borders, borderMask, closedNeightbour->_borders);

    _UpdateBorders();
}

// Method Description:
// - Fire our Closed event to tell our parent that we should be removed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void LeafPane::Close()
{
    // Fire our Closed event to tell our parent that we should be removed.
    _ClosedHandlers(nullptr, nullptr);
}

// Method Description:
// - Called when our attached control is closed. Triggers listeners to our close event.
// Arguments:
// - <none>
// Return Value:
// - <none>
winrt::fire_and_forget LeafPane::_ControlConnectionStateChangedHandler(const TermControl& /*sender*/, const winrt::Windows::Foundation::IInspectable& /*args*/)
{
    const auto newConnectionState = _control.ConnectionState();
    if (newConnectionState < ConnectionState::Closed)
    {
        // Pane doesn't care if the connection isn't entering a terminal state.
        co_return;
    }

    auto weakThis{ weak_from_this() };
    co_await winrt::resume_foreground(_root.Dispatcher());

    if (auto strongThis{ weakThis.lock() })
    {
        const auto& settings = CascadiaSettings::GetCurrentAppSettings();
        auto paneProfile = settings.FindProfile(_profile);
        if (paneProfile)
        {
            auto mode = paneProfile->GetCloseOnExitMode();
            if ((mode == CloseOnExitMode::Always) ||
                (mode == CloseOnExitMode::Graceful && newConnectionState == ConnectionState::Closed))
            {
                strongThis->Close();
            }
        }
    }
}

// Event Description:
// - Called when our control gains focus. We'll use this to trigger our GotFocus
//   callback. The tab that's hosting us should have registered a callback which
//   can be used to mark us as active.
// Arguments:
// - <unused>
// Return Value:
// - <none>
void LeafPane::_ControlGotFocusHandler(winrt::Windows::Foundation::IInspectable const& /* sender */,
                                       RoutedEventArgs const& /* args */)
{
    _GotFocusHandlers(shared_from_this());
}

Pane::SnapSizeResult LeafPane::_CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
{
    // We're a leaf pane, so just align to the grid of controlling terminal.

    const auto minSize = _GetMinSize();
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

void LeafPane::_AdvanceSnappedDimension(const bool widthOrHeight, LayoutSizeNode& sizeNode) const
{
    // We're a leaf pane, so just add one more row or column (unless isMinimumSize
    // is true, see below).

    if (sizeNode.isMinimumSize)
    {
        // If the node is of its minimum size, this size might not be snapped (it might
        // be, say, half a character, or fixed 10 pixels), so snap it upward. It might
        // however be already snapped, so add 1 to make sure it really increases
        // (not strictly necessary but to avoid surprises).
        sizeNode.size = _CalcSnappedDimension(widthOrHeight, sizeNode.size + 1).higher;
    }
    else
    {
        const auto cellSize = _control.CharacterDimensions();
        sizeNode.size += widthOrHeight ? cellSize.Width : cellSize.Height;
    }

    // Because we have grown, we're certainly no longer of our
    // minimal size (if we've ever been).
    sizeNode.isMinimumSize = false;
}

Size LeafPane::_GetMinSize() const
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

DEFINE_EVENT(LeafPane, Splitted, _SplittedHandlers, winrt::delegate<std::shared_ptr<ParentPane>>);
DEFINE_EVENT(LeafPane, GotFocus, _GotFocusHandlers, winrt::delegate<std::shared_ptr<LeafPane>>);
