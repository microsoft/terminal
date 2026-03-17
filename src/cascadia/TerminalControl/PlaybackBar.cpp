// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "PlaybackBar.h"

#include "PlaybackBar.g.cpp"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    PlaybackBar::PlaybackBar()
    {
        InitializeComponent();
        _hideTimer.Tick({ get_weak(), &PlaybackBar::_onHideTimerTick });
    }

    void PlaybackBar::Show(const TerminalConnection::AsciicastConnection& connection)
    {
        _connection = connection;
        _isOpen = true;

        const auto totalDuration = _connection.TotalDuration();
        TimelineSlider().Maximum(totalDuration);
        TimelineSlider().Value(0);
        _updateTimeDisplay();

        // Show bar initially, then hide after 2 seconds.
        _showBar();
        _hideTimer.Interval(std::chrono::milliseconds{ 2000 });
        _hideTimer.Start();

        _positionChangedRevoker = _connection.PlaybackPositionChanged(
            winrt::auto_revoke,
            [weakThis = get_weak()](auto&&, auto&&) {
                if (auto self = weakThis.get())
                {
                    self->Dispatcher().RunAsync(
                        Windows::UI::Core::CoreDispatcherPriority::Normal,
                        [weakSelf = self->get_weak()]() {
                            if (auto s = weakSelf.get())
                            {
                                if (!s->_isDraggingSlider && s->_connection)
                                {
                                    s->TimelineSlider().Value(s->_connection.CurrentPosition());
                                    s->_updateTimeDisplay();
                                }
                            }
                        });
                }
            });

        _stateChangedRevoker = _connection.PlaybackStateChanged(
            winrt::auto_revoke,
            [weakThis = get_weak()](auto&&, auto&&) {
                if (auto self = weakThis.get())
                {
                    self->Dispatcher().RunAsync(
                        Windows::UI::Core::CoreDispatcherPriority::Normal,
                        [weakSelf = self->get_weak()]() {
                            if (auto s = weakSelf.get())
                            {
                                if (s->_connection)
                                {
                                    const auto paused = s->_connection.IsPaused();
                                    // Play icon when paused, Pause icon when playing
                                    s->PlayPauseButton().Content(
                                        winrt::box_value(paused ? L"\xE768" : L"\xE769"));
                                    s->PlayPauseButton().SetValue(
                                        Windows::UI::Xaml::Controls::ToolTipService::ToolTipProperty(),
                                        winrt::box_value(paused ? L"Play" : L"Pause"));
                                }
                            }
                        });
                }
            });

        Visibility(Windows::UI::Xaml::Visibility::Visible);
    }

    void PlaybackBar::Hide()
    {
        _isOpen = false;
        _positionChangedRevoker = {};
        _stateChangedRevoker = {};
        _connection = nullptr;
        Visibility(Windows::UI::Xaml::Visibility::Collapsed);
    }

    void PlaybackBar::OnPlayPauseClicked(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (_connection)
        {
            if (_connection.IsPaused())
            {
                _connection.ResumePlayback();
            }
            else
            {
                _connection.PausePlayback();
            }
        }
    }

    void PlaybackBar::OnRestartClicked(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (_connection)
        {
            _connection.RestartPlayback();
        }
    }

    void PlaybackBar::OnSpeedClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (_connection)
        {
            if (const auto item = sender.try_as<Windows::UI::Xaml::Controls::MenuFlyoutItem>())
            {
                const auto tag = winrt::unbox_value<winrt::hstring>(item.Tag());
                double speed = 1.0;
                if (tag == L"0.5") speed = 0.5;
                else if (tag == L"2") speed = 2.0;
                else if (tag == L"4") speed = 4.0;
                _connection.PlaybackSpeed(speed);
                SpeedButton().Content(winrt::box_value(item.Text()));
            }
        }
    }

    void PlaybackBar::OnCloseClicked(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (_connection)
        {
            _connection.Close();
        }
        Hide();
    }

    void PlaybackBar::OnSliderPressed(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _isDraggingSlider = true;
    }

    void PlaybackBar::OnSliderReleased(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _isDraggingSlider = false;
        if (_connection)
        {
            _connection.SeekPlayback(TimelineSlider().Value());
        }
    }

    void PlaybackBar::OnPointerEnteredBar(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _hideTimer.Stop();
        _showBar();
    }

    void PlaybackBar::OnPointerExitedBar(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _hideTimer.Interval(std::chrono::milliseconds{ 1500 });
        _hideTimer.Start();
    }

    void PlaybackBar::_onHideTimerTick(const Windows::Foundation::IInspectable& /*sender*/, const Windows::Foundation::IInspectable& /*e*/)
    {
        _hideTimer.Stop();
        _hideBar();
    }

    void PlaybackBar::_showBar()
    {
        if (!_barVisible)
        {
            _barVisible = true;
            BarPanel().Opacity(1.0);
        }
    }

    void PlaybackBar::_hideBar()
    {
        if (_barVisible && !_isDraggingSlider)
        {
            _barVisible = false;
            BarPanel().Opacity(0.0);
        }
    }

    void PlaybackBar::_updateTimeDisplay()
    {
        if (_connection)
        {
            const auto current = _connection.CurrentPosition();
            const auto total = _connection.TotalDuration();
            const auto text = _formatTime(current) + L" / " + _formatTime(total);
            TimeDisplay().Text(text);
        }
    }

    winrt::hstring PlaybackBar::_formatTime(double seconds)
    {
        const auto totalSecs = static_cast<int>(seconds);
        const auto mins = totalSecs / 60;
        const auto secs = totalSecs % 60;
        wchar_t buf[16];
        swprintf_s(buf, L"%d:%02d", mins, secs);
        return winrt::hstring{ buf };
    }
}
