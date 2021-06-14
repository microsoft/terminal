// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "til/throttled_func.h"

// ThrottledFunc is a copy of til::throttled_func,
// specialized for the use with a WinRT Dispatcher.
template<bool leading, typename... Args>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<leading, Args...>>
{
public:
    using filetime_duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
    using function = std::function<void(Args...)>;

    // Throttles invocations to the given `func` to not occur more often than `delay`.
    //
    // If this is a:
    // * ThrottledFuncLeading: `func` will be invoked immediately and
    //   further invocations prevented until `delay` time has passed.
    // * ThrottledFuncTrailing: On the first invocation a timer of `delay` time will
    //   be started. After the timer has expired `func` will be invoked just once.
    //
    // After `func` was invoked the state is reset and this cycle is repeated again.
    ThrottledFunc(
        winrt::Windows::UI::Core::CoreDispatcher dispatcher,
        filetime_duration delay,
        function func) :
        _dispatcher{ std::move(dispatcher) },
        _func{ std::move(func) },
        _timer{ _createTimer() }
    {
        const auto d = -delay.count();
        if (d >= 0)
        {
            throw std::invalid_argument("non-positive delay specified");
        }

        memcpy(&_delay, &d, sizeof(d));
    }

    // ThrottledFunc uses its `this` pointer when creating _timer.
    // Since the timer cannot be recreated, instances cannot be moved either.
    ThrottledFunc(const ThrottledFunc&) = delete;
    ThrottledFunc& operator=(const ThrottledFunc&) = delete;
    ThrottledFunc(ThrottledFunc&&) = delete;
    ThrottledFunc& operator=(ThrottledFunc&&) = delete;

    // Throttles the invocation of the function passed to the constructor.
    // If this is a trailing_throttled_func:
    //   If you call this function again before the underlying
    //   timer has expired, the new arguments will be used.
    template<typename... MakeArgs>
    void Run(MakeArgs&&... args)
    {
        if (!_storage.emplace(std::forward<MakeArgs>(args)...))
        {
            _leading_edge();
        }
    }

    // Modifies the pending arguments for the next function
    // invocation, if there is one pending currently.
    //
    // `func` will be invoked as func(Args...). Make sure to bind any
    // arguments in `func` by reference if you'd like to modify them.
    template<typename F>
    void ModifyPending(F func)
    {
        _storage.modify_pending(func);
    }

private:
    static void __stdcall _timer_callback(PTP_CALLBACK_INSTANCE /*instance*/, PVOID context, PTP_TIMER /*timer*/) noexcept
    {
        static_cast<ThrottledFunc*>(context)->_trailing_edge();
    }

    template<typename = std::enable_if_t<leading>>
    winrt::fire_and_forget _leading_edge()
    try
    {
        auto weakSelf = this->weak_from_this();
        co_await winrt::resume_foreground(_dispatcher);

        if (auto self{ weakSelf.lock() })
        {
            self->_func();
            SetThreadpoolTimerEx(self->_timer.get(), &self->_delay, 0, 0);
        }
    }
    CATCH_FAIL_FAST()

    template<typename = std::enable_if_t<!leading>>
    void _leading_edge()
    {
        SetThreadpoolTimerEx(_timer.get(), &_delay, 0, 0);
    }

    template<typename = std::enable_if_t<leading>>
    void _trailing_edge()
    {
        _storage.reset();
    }

    template<typename = std::enable_if_t<!leading>>
    winrt::fire_and_forget _trailing_edge()
    try
    {
        auto weakSelf = this->weak_from_this();
        co_await winrt::resume_foreground(_dispatcher);

        if (auto self{ weakSelf.lock() })
        {
            std::apply(self->_func, self->_storage.take());
        }
    }
    CATCH_FAIL_FAST()

    inline wil::unique_threadpool_timer _createTimer()
    {
        wil::unique_threadpool_timer timer{ CreateThreadpoolTimer(&_timer_callback, this, nullptr) };
        THROW_LAST_ERROR_IF(!timer);
        return timer;
    }

    FILETIME _delay;
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;
    function _func;
    wil::unique_threadpool_timer _timer;
    til::details::throttled_func_storage<Args...> _storage;
};

template<typename... Args>
using ThrottledFuncTrailing = ThrottledFunc<false, Args...>;
using ThrottledFuncLeading = ThrottledFunc<true>;
