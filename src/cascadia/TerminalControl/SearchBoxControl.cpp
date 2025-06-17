// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SearchBoxControl.h"
#include "SearchBoxControl.g.cpp"
#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // Constructor
    SearchBoxControl::SearchBoxControl()
    {
        InitializeComponent();

        _initialLoadedRevoker = Loaded(winrt::auto_revoke, [weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto searchbox{ weakThis.get() })
            {
                searchbox->_Initialize();
                searchbox->_initialLoadedRevoker.revoke();
            }
        });
        this->CharacterReceived({ this, &SearchBoxControl::_CharacterHandler });
        this->KeyDown({ this, &SearchBoxControl::_KeyDownHandler });

        _focusableElements.insert(TextBox());
        _focusableElements.insert(CloseButton());
        _focusableElements.insert(CaseSensitivityButton());
        _focusableElements.insert(RegexButton());
        _focusableElements.insert(GoForwardButton());
        _focusableElements.insert(GoBackwardButton());

        StatusBox().Width(_GetStatusMaxWidth());
    }

    winrt::Windows::Foundation::Rect SearchBoxControl::ContentClipRect() const noexcept
    {
        return _contentClipRect;
    }

    void SearchBoxControl::_ContentClipRect(const winrt::Windows::Foundation::Rect& rect)
    {
        if (rect != _contentClipRect)
        {
            _contentClipRect = rect;
            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"ContentClipRect" });
        }
    }

    double SearchBoxControl::OpenAnimationStartPoint() const noexcept
    {
        return _openAnimationStartPoint;
    }

    void SearchBoxControl::_OpenAnimationStartPoint(double y)
    {
        if (y != _openAnimationStartPoint)
        {
            _openAnimationStartPoint = y;
            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"OpenAnimationStartPoint" });
        }
    }

    void SearchBoxControl::_UpdateSizeDependents()
    {
        const winrt::Windows::Foundation::Size infiniteSize{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity() };
        Measure(infiniteSize);
        const auto desiredSize = DesiredSize();
        _OpenAnimationStartPoint(-desiredSize.Height);
        _ContentClipRect({ 0, 0, desiredSize.Width, desiredSize.Height });
    }

    void SearchBoxControl::_PlayOpenAnimation()
    {
        if (CloseAnimation().GetCurrentState() == Media::Animation::ClockState::Active)
        {
            CloseAnimation().Stop();
        }

        if (OpenAnimation().GetCurrentState() != Media::Animation::ClockState::Active)
        {
            OpenAnimation().Begin();
        }
    }

    void SearchBoxControl::_PlayCloseAnimation()
    {
        if (OpenAnimation().GetCurrentState() == Media::Animation::ClockState::Active)
        {
            OpenAnimation().Stop();
        }

        if (CloseAnimation().GetCurrentState() != Media::Animation::ClockState::Active)
        {
            CloseAnimation().Begin();
        }
    }

    // Method Description:
    // - Sets the search box control to its initial state and calls the initialized callback if it's set.
    void SearchBoxControl::_Initialize()
    {
        _UpdateSizeDependents();

        // Search box is in Visible visibility state by default. This is to make
        // sure DesiredSize() returns the correct size for the search box.
        // (DesiredSize() seems to report a size of 0,0 until the control is
        // visible for the first time, i.e not in Collapsed state).
        // Here, we set the search box to "Closed" state (and hence Collapsed
        // visibility) after we've updated the size-dependent properties.
        VisualStateManager::GoToState(*this, L"Closed", false);

        CloseAnimation().Completed([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto searchbox{ weakThis.get() })
            {
                searchbox->CloseAnimation().Stop();
                VisualStateManager::GoToState(*searchbox, L"Closed", false);
            }
        });

        _initialized = true;
        if (_initializedCallback)
        {
            _initializedCallback();
            _initializedCallback = nullptr;
        }
    }

    bool SearchBoxControl::_AnimationEnabled()
    {
        const auto uiSettings = winrt::Windows::UI::ViewManagement::UISettings{};
        const auto isOsAnimationEnabled = uiSettings.AnimationsEnabled();
        const auto isAppAnimationEnabled = Media::Animation::Timeline::AllowDependentAnimations();
        return isOsAnimationEnabled && isAppAnimationEnabled;
    }

    // Method Description:
    // - Opens the search box taking a callback to be executed when it's opened.
    void SearchBoxControl::Open(std::function<void()> callback)
    {
        // defer opening the search box until we have initialized our size-dependent
        // properties so we don't animate to wrong values.
        if (!_initialized)
        {
            _initializedCallback = [this, callback]() { Open(callback); };
        }
        else
        {
            // don't run animation if we're already open.
            // Note: We can't apply this check at the beginning of the function because the
            // search box remains in Visible state (though not really *visible*) during the
            // first load. So, we only need to apply this check here (after checking that
            // we're done initializing).
            if (IsOpen())
            {
                callback();
                return;
            }

            // Stop ongoing close animation if any
            if (CloseAnimation().GetCurrentState() == Media::Animation::ClockState::Active)
            {
                CloseAnimation().Stop();
            }

            VisualStateManager::GoToState(*this, L"Opened", false);

            // Call the callback only after we're in Opened state. Setting focus
            // (through the callback) to a collapsed search box will not work.
            callback();

            // Don't animate if animation is disabled
            if (_AnimationEnabled())
            {
                _PlayOpenAnimation();
            }
        }
    }

    // Method Description:
    // - Closes the search box.
    void SearchBoxControl::Close()
    {
        // Nothing to do if we're already closed
        if (Visibility() == Visibility::Collapsed)
        {
            return;
        }

        if (_AnimationEnabled())
        {
            // close animation will set the state to "Closed" in its Completed handler.
            _PlayCloseAnimation();
        }
        else
        {
            VisualStateManager::GoToState(*this, L"Closed", false);
        }
    }

    bool SearchBoxControl::IsOpen()
    {
        return Visibility() == Visibility::Visible && CloseAnimation().GetCurrentState() != Media::Animation::ClockState::Active;
    }

    winrt::hstring SearchBoxControl::Text()
    {
        return TextBox().Text();
    }

    // Method Description:
    // - Check if the current search direction is forward
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: the current search direction, determined by the
    //         states of the two direction buttons
    bool SearchBoxControl::GoForward()
    {
        return GoForwardButton().IsChecked().GetBoolean();
    }

    // Method Description:
    // - Check if the current search is case-sensitive
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: whether the current search is case-sensitive (case button is checked )
    //   or not
    bool SearchBoxControl::CaseSensitive()
    {
        return CaseSensitivityButton().IsChecked().GetBoolean();
    }

    bool SearchBoxControl::RegularExpression()
    {
        return RegexButton().IsChecked().GetBoolean();
    }

    // Method Description:
    // - Handler for pressing Enter on TextBox, trigger
    //   text search
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            // If the buttons are disabled, then don't allow enter to search either.
            if (!GoForwardButton().IsEnabled() || !GoBackwardButton().IsEnabled())
            {
                return;
            }

            const auto state = CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift);
            if (WI_IsFlagSet(state, CoreVirtualKeyStates::Down))
            {
                Search.raise(Text(), !GoForward(), CaseSensitive(), RegularExpression());
            }
            else
            {
                Search.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
            }
            e.Handled(true);
        }
    }

    // Method Description:
    // - Handler for pressing "Esc" when focusing
    //   on the search dialog, this triggers close
    //   event of the Search dialog
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::_KeyDownHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                           const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            Closed.raise(*this, e);
            e.Handled(true);
        }
    }

    // Method Description:
    // - Handler for pressing Enter on TextBox, trigger
    //   text search
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SearchBoxControl::SetFocusOnTextbox()
    {
        if (TextBox())
        {
            Input::FocusManager::TryFocusAsync(TextBox(), FocusState::Keyboard);
            TextBox().SelectAll();
        }
    }

    // Method Description:
    // - Allows to set the value of the text to search
    // Arguments:
    // - text: string value to populate in the TextBox
    // Return Value:
    // - <none>
    void SearchBoxControl::PopulateTextbox(const winrt::hstring& text)
    {
        if (TextBox())
        {
            TextBox().Text(text);
        }
    }

    // Method Description:
    // - Check if the current focus is on any element within the
    //   search box
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: whether the current focus is on the search box
    bool SearchBoxControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    // Method Description:
    // - Handler for clicking the GoBackward button. This change the value of _goForward,
    //   mark GoBackward button as checked and ensure GoForward button
    //   is not checked
    // Arguments:
    // - sender: not used
    // - e: not used
    // Return Value:
    // - <none>
    void SearchBoxControl::GoBackwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        GoBackwardButton().IsChecked(true);
        if (GoForwardButton().IsChecked())
        {
            GoForwardButton().IsChecked(false);
        }

        // kick off search
        Search.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
    }

    // Method Description:
    // - Handler for clicking the GoForward button. This change the value of _goForward,
    //   mark GoForward button as checked and ensure GoBackward button
    //   is not checked
    // Arguments:
    // - sender: not used
    // - e: not used
    // Return Value:
    // - <none>
    void SearchBoxControl::GoForwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        GoForwardButton().IsChecked(true);
        if (GoBackwardButton().IsChecked())
        {
            GoBackwardButton().IsChecked(false);
        }

        // kick off search
        Search.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
    }

    // Method Description:
    // - Handler for clicking the close button. This destructs the
    //   search box object in TermControl
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::CloseClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& e)
    {
        Closed.raise(*this, e);
    }

    // Method Description:
    // - To avoid Characters input bubbling up to terminal, we implement this handler here,
    //   simply mark the key input as handled
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::_CharacterHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::CharacterReceivedRoutedEventArgs& e)
    {
        e.Handled(true);
    }

    // Method Description:
    // - Handler for changing the text. Triggers SearchChanged event
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        SearchChanged.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
    }

    // Method Description:
    // - Handler for clicking the case sensitivity toggle. Triggers SearchChanged event
    // Arguments:
    // - sender: not used
    // - e: not used
    // Return Value:
    // - <none>
    void SearchBoxControl::CaseSensitivityButtonClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        SearchChanged.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
    }

    void SearchBoxControl::RegexButtonClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        SearchChanged.raise(Text(), GoForward(), CaseSensitive(), RegularExpression());
    }

    // Method Description:
    // - Handler for searchbox pointer-pressed.
    // - Marks pointer events as handled so they don't bubble up to the terminal.
    void SearchBoxControl::SearchBoxPointerPressedHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        e.Handled(true);
    }

    // Method Description:
    // - Handler for searchbox pointer-released.
    // - Marks pointer events as handled so they don't bubble up to the terminal.
    void SearchBoxControl::SearchBoxPointerReleasedHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e)
    {
        e.Handled(true);
    }

    // Method Description:
    // - Formats a status message representing the search state:
    // * "Searching" - if totalMatches is negative
    // * "No results" - if totalMatches is 0
    // * "?/n" - if totalMatches=n matches and we didn't start the iteration over matches
    // (usually we will get this after buffer update)
    // * "m/n" - if we are currently at match m out of n.
    // * "m/max+" - if n > max results to show
    // * "?/max+" - if m > max results to show
    // Arguments:
    // - totalMatches - total number of matches (search results)
    // - currentMatch - the index of the current match (0-based)
    // Return Value:
    // - status message
    winrt::hstring SearchBoxControl::_FormatStatus(int32_t totalMatches, int32_t currentMatch)
    {
        if (totalMatches < 0)
        {
            return RS_(L"TermControl_Searching");
        }

        if (totalMatches == 0)
        {
            return RS_(L"TermControl_NoMatch");
        }

        std::wstring currentString;
        std::wstring totalString;

        if (currentMatch < 0 || currentMatch > (MaximumTotalResultsToShowInStatus - 1))
        {
            currentString = CurrentIndexTooHighStatus;
        }
        else
        {
            currentString = fmt::to_wstring(currentMatch + 1);
        }

        if (totalMatches > MaximumTotalResultsToShowInStatus)
        {
            totalString = TotalResultsTooHighStatus;
        }
        else
        {
            totalString = fmt::to_wstring(totalMatches);
        }

        return winrt::hstring{ RS_fmt(L"TermControl_NumResults", currentString, totalString) };
    }

    // Method Description:
    // - Helper method to measure the width of the text block given the text and the font size
    // Arguments:
    // - text: the text to measure
    // - fontSize: the size of the font to measure
    // Return Value:
    // - the size in pixels
    double SearchBoxControl::_TextWidth(winrt::hstring text, double fontSize)
    {
        winrt::Windows::UI::Xaml::Controls::TextBlock t;
        t.FontSize(fontSize);
        t.Text(text);
        t.Measure({ FLT_MAX, FLT_MAX });
        return t.ActualWidth();
    }

    // Method Description:
    // - This method tries to predict the maximal size of the status box
    // by measuring different possible statuses
    // Return Value:
    // - the size in pixels
    double SearchBoxControl::_GetStatusMaxWidth()
    {
        const auto fontSize = StatusBox().FontSize();
        const auto maxLength = std::max({ _TextWidth(RS_(L"SearchRegexInvalid"), fontSize),
                                          _TextWidth(_FormatStatus(-1, -1), fontSize),
                                          _TextWidth(_FormatStatus(0, -1), fontSize),
                                          _TextWidth(_FormatStatus(MaximumTotalResultsToShowInStatus, MaximumTotalResultsToShowInStatus - 1), fontSize),
                                          _TextWidth(_FormatStatus(MaximumTotalResultsToShowInStatus + 1, MaximumTotalResultsToShowInStatus - 1), fontSize),
                                          _TextWidth(_FormatStatus(MaximumTotalResultsToShowInStatus + 1, MaximumTotalResultsToShowInStatus), fontSize) });

        return maxLength;
    }

    // Method Description:
    // - Formats and sets the status message in the status box.
    // Increases the size of the box if required.
    // Arguments:
    // - totalMatches - total number of matches (search results)
    // - currentMatch - the index of the current match (0-based)
    // Return Value:
    // - <none>
    void SearchBoxControl::SetStatus(int32_t totalMatches, int32_t currentMatch, bool searchRegexInvalid)
    {
        hstring status;
        if (searchRegexInvalid)
        {
            status = RS_(L"SearchRegexInvalid");
        }
        else
        {
            status = _FormatStatus(totalMatches, currentMatch);
        }
        StatusBox().Text(status);
    }

    // Method Description:
    // - Removes the status message in the status box.
    void SearchBoxControl::ClearStatus()
    {
        StatusBox().Text(L"");
    }
}
