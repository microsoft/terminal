// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

template<bool leading, typename... Args>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<leading, Args...>>
{
    using impl = til::throttled_func<leading, Args...>;

public:
    ThrottledFunc(
        winrt::Windows::UI::Core::CoreDispatcher dispatcher,
        typename impl::filetime_duration delay,
        typename impl::function func) :
        _dispatcher{ std::move(dispatcher) },
        _func{ std::move(func) },
        _impl{
            delay,
            [this](auto&&... args) {
                // With C++20 we'll be able to write:
                //   [...args = std::forward<Args>(args)]() { ... }
                _dispatcher.RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [=, weakSelf = this->weak_from_this()] {
                    if (auto self{ weakSelf.lock() })
                    {
                        self->_func(args...);
                    }
                });
            }
        }
    {
    }

    template<typename... MakeArgs>
    void Run(MakeArgs&&... args)
    {
        _impl(std::forward<MakeArgs>(args)...);
    }

    template<typename F>
    void ModifyPending(F f)
    {
        _impl.modify_pending(f);
    }

private:
    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;
    typename impl::function _func;
    // As the last member, _throttler will be destroyed first.
    // This ensures that _func and _dispatcher are still valid
    // in case one final delayed invocation gets through.
    impl _impl;
};

template<typename... Args>
using ThrottledFuncTrailing = ThrottledFunc<false, Args...>;
using ThrottledFuncLeading = ThrottledFunc<true>;
