// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Par for the course, the XAML timer class is "self-referential". Releasing all references
// to an instance will not stop the timer. Only calling Stop() explicitly will achieve that.
struct SafeDispatcherTimer
{
    SafeDispatcherTimer() = default;
    SafeDispatcherTimer(SafeDispatcherTimer const&) = delete;
    SafeDispatcherTimer& operator=(SafeDispatcherTimer const&) = delete;
    SafeDispatcherTimer(SafeDispatcherTimer&&) = delete;
    SafeDispatcherTimer& operator=(SafeDispatcherTimer&&) = delete;

    ~SafeDispatcherTimer()
    {
        Destroy();
    }

    explicit operator bool() const noexcept
    {
        return _timer != nullptr;
    }

    winrt::Windows::Foundation::TimeSpan Interval()
    {
        return _getTimer().Interval();
    }

    void Interval(winrt::Windows::Foundation::TimeSpan const& value)
    {
        _getTimer().Interval(value);
    }

    bool IsEnabled()
    {
        return _timer && _timer.IsEnabled();
    }

    void Tick(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler)
    {
        auto& timer = _getTimer();
        if (_token)
        {
            timer.Tick(_token);
        }
        _token = timer.Tick(handler);
    }

    void Start()
    {
        _getTimer().Start();
    }

    void Stop() const
    {
        if (_timer)
        {
            _timer.Stop();
        }
    }

    void Destroy()
    {
        if (!_timer)
        {
            return;
        }

        _timer.Stop();
        if (_token)
        {
            _timer.Tick(_token);
        }
        _timer = nullptr;
        _token = {};
    }

private:
    ::winrt::Windows::UI::Xaml::DispatcherTimer& _getTimer()
    {
        if (!_timer)
        {
            _timer = ::winrt::Windows::UI::Xaml::DispatcherTimer{};
        }
        return _timer;
    }

    ::winrt::Windows::UI::Xaml::DispatcherTimer _timer{ nullptr };
    winrt::event_token _token;
};
