// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    struct throttled_func_options
    {
        using filetime_duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;

        filetime_duration delay{};
        bool debounce = false;
        bool leading = false;
        bool trailing = false;
    };

    template<typename... Args>
    class throttled_func
    {
    public:
        using filetime_duration = throttled_func_options::filetime_duration;
        using function = std::function<void(Args...)>;

        // Throttles invocations to the given `func` to not occur more often than specified in options.
        //
        // Options:
        // * delay: The minimum time between invocations
        // * debounce: If true, resets the timer on each call
        // * leading: If true, `func` will be invoked immediately on first call
        // * trailing: If true, `func` will be invoked after the delay
        //
        // At least one of leading or trailing must be true.
        throttled_func(throttled_func_options opts, function func) :
            _func{ std::move(func) },
            _timer{ _create_timer() },
            _debounce{ opts.debounce },
            _leading{ opts.leading },
            _trailing{ opts.trailing }
        {
            if (!_leading && !_trailing)
            {
                throw std::invalid_argument("neither leading nor trailing");
            }

            const auto d = -opts.delay.count();
            if (d >= 0)
            {
                throw std::invalid_argument("non-positive delay specified");
            }
            memcpy(&_delay, &d, sizeof(d));
        }

        // throttled_func uses its `this` pointer when creating _timer.
        // Since the timer cannot be recreated, instances cannot be moved either.
        throttled_func(const throttled_func&) = delete;
        throttled_func& operator=(const throttled_func&) = delete;
        throttled_func(throttled_func&&) = delete;
        throttled_func& operator=(throttled_func&&) = delete;

        // Throttles the invocation of the function passed to the constructor.
        //
        // If Debounce is true and you call this function again before the
        // underlying timer has expired, its timeout will be reset.
        //
        // If Leading is true and you call this function again before the
        // underlying timer has expired, the new arguments will be used.
        template<typename... MakeArgs>
        void operator()(MakeArgs&&... args)
        {
            _lead(std::make_tuple(std::forward<MakeArgs>(args)...));
        }

        // Modifies the pending arguments for the next function
        // invocation, if there is one pending currently.
        //
        // `func` will be invoked as func(Args...). Make sure to bind any
        // arguments in `func` by reference if you'd like to modify them.
        template<typename F>
        void modify_pending(F func)
        {
            const auto guard = _lock.lock_exclusive();
            if (_pendingRunArgs)
            {
                std::apply(func, *_pendingRunArgs);
            }
        }

        // Makes sure that the currently pending timer is executed
        // as soon as possible and in that case waits for its completion.
        // You can use this function in your destructor to ensure that any
        // pending callback invocation is completed as soon as possible.
        //
        // NOTE: Don't call this function if the operator()
        //       could still be called concurrently.
        void flush()
        {
            WaitForThreadpoolTimerCallbacks(_timer.get(), true);
            _timer_callback(nullptr, this, nullptr);
        }

    private:
        static void __stdcall _timer_callback(PTP_CALLBACK_INSTANCE /*instance*/, PVOID context, PTP_TIMER /*timer*/) noexcept
        try
        {
            static_cast<throttled_func*>(context)->_trail();
        }
        CATCH_LOG()

        wil::unique_threadpool_timer _create_timer()
        {
            wil::unique_threadpool_timer timer{ CreateThreadpoolTimer(&_timer_callback, this, nullptr) };
            THROW_LAST_ERROR_IF(!timer);
            return timer;
        }

        void _lead(std::tuple<Args...> args)
        {
            bool timerRunning = false;

            {
                const auto guard = _lock.lock_exclusive();

                timerRunning = _timerRunning;
                _timerRunning = true;

                if (!timerRunning && _leading)
                {
                    // Call the function immediately on the leading edge.
                    // See below (out of lock).
                }
                else if (_trailing)
                {
                    _pendingRunArgs.emplace(std::move(args));
                }
            }

            if (!timerRunning && _leading)
            {
                std::apply(_func, std::move(args));
            }

            if (!timerRunning || _debounce)
            {
                SetThreadpoolTimerEx(_timer.get(), &_delay, 0, 0);
            }
        }

        void _trail()
        {
            decltype(_pendingRunArgs) args;

            {
                const auto guard = _lock.lock_exclusive();

                _timerRunning = false;
                args = std::exchange(_pendingRunArgs, std::nullopt);
            }

            if (args)
            {
                std::apply(_func, *std::move(args));
            }
        }

        function _func;
        wil::unique_threadpool_timer _timer;
        FILETIME _delay;

        wil::srwlock _lock;
        std::optional<std::tuple<Args...>> _pendingRunArgs;
        bool _timerRunning = false;

        bool _debounce;
        bool _leading;
        bool _trailing;
    };
} // namespace til
