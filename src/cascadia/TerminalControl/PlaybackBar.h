// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "PlaybackBar.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct PlaybackBar : PlaybackBarT<PlaybackBar>
    {
        PlaybackBar();

        void Show(const TerminalConnection::AsciicastConnection& connection);
        void Hide();
        bool IsOpen() const noexcept { return _isOpen; }

        void OnPlayPauseClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void OnRestartClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void OnSpeedClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void OnCloseClicked(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void OnSliderPressed(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void OnSliderReleased(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void OnPointerEnteredBar(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void OnPointerExitedBar(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

    private:
        void _updateTimeDisplay();
        void _showBar();
        void _hideBar();
        void _onHideTimerTick(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);
        static winrt::hstring _formatTime(double seconds);

        TerminalConnection::AsciicastConnection _connection{ nullptr };
        TerminalConnection::AsciicastConnection::PlaybackPositionChanged_revoker _positionChangedRevoker;
        TerminalConnection::AsciicastConnection::PlaybackStateChanged_revoker _stateChangedRevoker;
        bool _isOpen{ false };
        bool _isDraggingSlider{ false };
        bool _barVisible{ false };
        SafeDispatcherTimer _hideTimer;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(PlaybackBar);
}
