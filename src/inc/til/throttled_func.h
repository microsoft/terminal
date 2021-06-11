// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    namespace details
    {
        template<typename... Args>
        class throttled_func_storage
        {
        public:
            template<typename... MakeArgs>
            bool emplace(MakeArgs&&... args)
            {
                std::scoped_lock guard{ _lock };

                const bool hadValue = _pendingRunArgs.has_value();
                _pendingRunArgs.emplace(std::forward<MakeArgs>(args)...);
                return hadValue;
            }

            template<typename F>
            void modify_pending(F f)
            {
                std::scoped_lock guard{ _lock };

                if (_pendingRunArgs)
                {
                    std::apply(f, *_pendingRunArgs);
                }
            }

            inline void apply(const std::function<void(Args...)>& func)
            {
                apply_maybe(func);
            }

            void apply_maybe(const std::function<void(Args...)>& func)
            {
                std::optional<std::tuple<Args...>> args;
                {
                    std::scoped_lock guard{ _lock };
                    _pendingRunArgs.swap(args);
                }

                if (args)
                {
                    std::apply(func, *args);
                }
            }

        private:
            // std::mutex uses imperfect Critical Sections on Windows.
            // --> std::shared_mutex uses SRW locks that are small and fast.
            std::shared_mutex _lock;
            std::optional<std::tuple<Args...>> _pendingRunArgs;
        };

        template<>
        class throttled_func_storage<>
        {
        public:
            inline bool emplace()
            {
                return _isPending.exchange(true, std::memory_order_relaxed);
            }

            inline void apply(const std::function<void()>& func)
            {
                reset();
                func();
            }

            inline void apply_maybe(const std::function<void()>& func)
            {
                // on x86-64 .exchange() is implemented using the xchg instruction.
                // Its latency is vastly higher than the mov instruction used for a .store().
                // --> For this reason we have both apply() and apply_maybe() to prevent
                //     us from suffering xchg's latency if we know _isPending is true.
                if (_isPending.exchange(false, std::memory_order_relaxed))
                {
                    func();
                }
            }

            inline void reset()
            {
                _isPending.store(false, std::memory_order_relaxed);
            }

        private:
            std::atomic<bool> _isPending;
        };
    } // namespace details

    template<bool leading, typename... Args>
    class throttled_func
    {
    public:
        using filetime_duration = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;
        using function = std::function<void(Args...)>;

        // Throttles invocations to the given `func` to not occur more often than `delay`.
        //
        // If this is a:
        // * leading_throttled_func: `func` will be invoked immediately and
        //   further invocations prevented until `delay` time has passed.
        // * trailing_throttled_func: On the first invocation a timer of `delay` time will
        //   be started. After the timer has expired `func` will be invoked just once.
        //
        // After `func` was invoked the state is reset and this cycle is repeated again.
        throttled_func(filetime_duration delay, function func) :
            _delay{ -delay.count() },
            _func{ std::move(func) },
            _timer{ _createTimer() }
        {
            if (_delay >= 0)
            {
                throw std::invalid_argument("non-positive delay specified");
            }
        }

        // throttled_func uses its `this` pointer when creating _timer.
        // Since the timer cannot be recreated, instances cannot be moved either.
        throttled_func(const throttled_func&) = delete;
        throttled_func& operator=(const throttled_func&) = delete;
        throttled_func(throttled_func&&) = delete;
        throttled_func& operator=(throttled_func&&) = delete;

        // Throttles the invocation of the function passed to the constructor.
        // If this is a trailing_throttled_func:
        //   If you call this function again before the underlying
        //   timer has expired, the new arguments will be used.
        template<typename... MakeArgs>
        void operator()(MakeArgs&&... args)
        {
            if (!_storage.emplace(std::forward<MakeArgs>(args)...))
            {
                _schedule_timer();
            }
        }

        // Modifies the pending arguments for the next function
        // invocation, if there is one pending currently.
        //
        // `func` will be invoked as func(Args...). Make sure to bind any
        // arguments in `func` by reference if you'd like to modify them.
        template<typename F>
        void modify_pending(F func)
        {
            _storage.modify_pending(func);
        }

        // Makes sure that the currently pending timer is executed
        // as soon as possible and in that case waits for its completion.
        void flush()
        {
            WaitForThreadpoolTimerCallbacks(_timer.get(), true);

            // Since we potentially canceled the pending timer we have to call _func() now.
            // --> We have to do the same thing _timer_callback does.
            //
            // But since we don't know whether we canceled a timer,
            // we have to use apply_maybe() instead of apply().
            // (apply_maybe is slightly less efficient than apply.)
            if constexpr (leading)
            {
                _storage.reset();
            }
            else
            {
                _storage.apply_maybe(_func);
            }
        }

    private:
        static void __stdcall _timer_callback(PTP_CALLBACK_INSTANCE /*instance*/, PVOID context, PTP_TIMER /*timer*/) noexcept
        try
        {
            auto& self = *static_cast<throttled_func*>(context);

            if constexpr (leading)
            {
                self._storage.reset();
            }
            else
            {
                // We know for sure that _storage._isPending is true.
                // --> use .apply() to get a slight performance benefit over .apply_maybe().
                self._storage.apply(self._func);
            }
        }
        CATCH_LOG()

        void _schedule_timer()
        {
            if constexpr (leading)
            {
                _func();
            }

            SetThreadpoolTimerEx(_timer.get(), reinterpret_cast<PFILETIME>(&_delay), 0, 0);
        }

        inline wil::unique_threadpool_timer _createTimer()
        {
            wil::unique_threadpool_timer timer{ CreateThreadpoolTimer(&_timer_callback, this, nullptr) };
            THROW_LAST_ERROR_IF(!timer);
            return timer;
        }

        int64_t _delay;
        function _func;
        wil::unique_threadpool_timer _timer;

        details::throttled_func_storage<Args...> _storage;
    };

    template<typename... Args>
    using throttled_func_trailing = throttled_func<false, Args...>;
    using throttled_func_leading = throttled_func<true>;
} // namespace til
