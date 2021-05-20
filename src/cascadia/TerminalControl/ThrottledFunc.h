/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ThrottledFunc.h
--*/

#pragma once
#include "pch.h"

template<typename... Args>
class ThrottledFuncStorage
{
public:
    template<typename... MakeArgs>
    bool Emplace(MakeArgs&&... args)
    {
        std::scoped_lock guard{ _lock };

        const bool hadValue = _pendingRunArgs.has_value();
        _pendingRunArgs.emplace(std::forward<MakeArgs>(args)...);
        return hadValue;
    }

    template<typename F>
    void ModifyPending(F f)
    {
        std::scoped_lock guard{ _lock };

        if (_pendingRunArgs.has_value())
        {
            std::apply(f, _pendingRunArgs.value());
        }
    }

    std::tuple<Args...> Extract()
    {
        decltype(_pendingRunArgs) args;
        std::scoped_lock guard{ _lock };
        _pendingRunArgs.swap(args);
        return args.value();
    }

private:
    std::mutex _lock;
    std::optional<std::tuple<Args...>> _pendingRunArgs;
};

template<>
class ThrottledFuncStorage<>
{
public:
    bool Emplace()
    {
        return _isRunPending.test_and_set(std::memory_order_relaxed);
    }

    std::tuple<> Extract()
    {
        Reset();
        return {};
    }

    void Reset()
    {
        _isRunPending.clear(std::memory_order_relaxed);
    }

private:
    std::atomic_flag _isRunPending;
};

// Class Description:
// - Represents a function that takes arguments and whose invocation is
//   delayed by a specified duration and rate-limited such that if the code
//   tries to run the function while a call to the function is already
//   pending, then the previous call with the previous arguments will be
//   cancelled and the call will be made with the new arguments instead.
// - The function will be run on the the specified dispatcher.
template<bool leading, typename... Args>
class ThrottledFunc : public std::enable_shared_from_this<ThrottledFunc<leading, Args...>>
{
public:
    using Func = std::function<void(Args...)>;

    ThrottledFunc(winrt::Windows::UI::Core::CoreDispatcher dispatcher, winrt::Windows::Foundation::TimeSpan delay, Func func) :
        _dispatcher{ std::move(dispatcher) },
        _delay{ std::move(delay) },
        _func{ std::move(func) }
    {
    }

    // Method Description:
    // - Runs the function later with the specified arguments, except if `Run`
    //   is called again before with new arguments, in which case the new
    //   arguments will be used instead.
    // - For more information, read the class' documentation.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - args: the arguments to pass to the function
    // Return Value:
    // - <none>
    template<typename... MakeArgs>
    void Run(MakeArgs&&... args)
    {
        if (!_storage.Emplace(std::forward<MakeArgs>(args)...))
        {
            _Fire();
        }
    }

    // Method Description:
    // - Modifies the pending arguments for the next function invocation, if
    //   there is one pending currently.
    // - Let's say that you just called the `Run` method with some arguments.
    //   After the delay specified in the constructor, the function specified
    //   in the constructor will be called with these arguments.
    // - By using this method, you can modify the arguments before the function
    //   is called.
    // - You pass a function to this method which will take references to
    //   the arguments (one argument corresponds to one reference to an
    //   argument) and will modify them.
    // - When there is no pending invocation of the function, this method will
    //   not do anything.
    // - This method is always thread-safe. It can be called multiple times on
    //   different threads.
    // Arguments:
    // - f: the function to call with references to the arguments
    // Return Value:
    // - <none>
    template<typename F>
    void ModifyPending(F f)
    {
        _storage.ModifyPending(f);
    }

private:
    winrt::fire_and_forget _Fire()
    {
        const auto dispatcher = _dispatcher;
        auto weakSelf = this->weak_from_this();

        if constexpr (leading)
        {
            co_await winrt::resume_foreground(dispatcher);

            if (auto self{ weakSelf.lock() })
            {
                self->_func();
            }
            else
            {
                co_return;
            }

            co_await winrt::resume_after(_delay);

            if (auto self{ weakSelf.lock() })
            {
                self->_storage.Reset();
            }
        }
        else
        {
            co_await winrt::resume_after(_delay);
            co_await winrt::resume_foreground(dispatcher);

            if (auto self{ weakSelf.lock() })
            {
                std::apply(self->_func, self->_storage.Extract());
            }
        }
    }

    winrt::Windows::UI::Core::CoreDispatcher _dispatcher;
    winrt::Windows::Foundation::TimeSpan _delay;
    Func _func;

    ThrottledFuncStorage<Args...> _storage;
};

template<typename... Args>
using ThrottledFuncTrailing = ThrottledFunc<false, Args...>;
using ThrottledFuncLeading = ThrottledFunc<true>;
