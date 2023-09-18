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
                std::unique_lock guard{ _lock };
                const bool hadValue = _pendingRunArgs.has_value();
                _pendingRunArgs.emplace(std::forward<MakeArgs>(args)...);
                return hadValue;
            }

            template<typename F>
            void modify_pending(F f)
            {
                std::unique_lock guard{ _lock };
                if (_pendingRunArgs)
                {
                    std::apply(f, *_pendingRunArgs);
                }
            }

            std::tuple<Args...> take()
            {
                std::unique_lock guard{ _lock };
                auto pendingRunArgs = std::move(*_pendingRunArgs);
                _pendingRunArgs.reset();
                return pendingRunArgs;
            }

            explicit operator bool() const
            {
                std::shared_lock guard{ _lock };
                return _pendingRunArgs.has_value();
            }

        private:
            // std::mutex uses imperfect Critical Sections on Windows.
            // --> std::shared_mutex uses SRW locks that are small and fast.
            mutable std::shared_mutex _lock;
            std::optional<std::tuple<Args...>> _pendingRunArgs;
        };

        template<>
        class throttled_func_storage<>
        {
        public:
            bool emplace()
            {
                return _isPending.exchange(true, std::memory_order_relaxed);
            }

            std::tuple<> take()
            {
                reset();
                return {};
            }

            void reset()
            {
                _isPending.store(false, std::memory_order_relaxed);
            }

            explicit operator bool() const
            {
                return _isPending.load(std::memory_order_relaxed);
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
        // * throttled_func_leading: `func` will be invoked immediately and
        //   further invocations prevented until `delay` time has passed.
        // * throttled_func_trailing: On the first invocation a timer of `delay` time will
        //   be started. After the timer has expired `func` will be invoked just once.
        //
        // After `func` was invoked the state is reset and this cycle is repeated again.
        throttled_func(filetime_duration delay, function func) :
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
                _leading_edge();
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
        // You can use this function in your destructor to ensure that any
        // pending callback invocation is completed as soon as possible.
        //
        // NOTE: Don't call this function if the operator()
        //       could still be called concurrently.
        void flush()
        {
            WaitForThreadpoolTimerCallbacks(_timer.get(), true);
            if (_storage)
            {
                _trailing_edge();
            }
        }

    private:
        static void __stdcall _timer_callback(PTP_CALLBACK_INSTANCE /*instance*/, PVOID context, PTP_TIMER /*timer*/) noexcept
        try
        {
            static_cast<throttled_func*>(context)->_trailing_edge();
        }
        CATCH_LOG()

        void _leading_edge()
        {
            if constexpr (leading)
            {
                _func();
            }

            SetThreadpoolTimerEx(_timer.get(), &_delay, 0, 0);
        }

        void _trailing_edge()
        {
            if constexpr (leading)
            {
                _storage.reset();
            }
            else
            {
                std::apply(_func, _storage.take());
            }
        }

        inline wil::unique_threadpool_timer _createTimer()
        {
            wil::unique_threadpool_timer timer{ CreateThreadpoolTimer(&_timer_callback, this, nullptr) };
            THROW_LAST_ERROR_IF(!timer);
            return timer;
        }

        FILETIME _delay;
        function _func;
        wil::unique_threadpool_timer _timer;
        details::throttled_func_storage<Args...> _storage;
    };

    template<typename... Args>
    using throttled_func_trailing = throttled_func<false, Args...>;
    using throttled_func_leading = throttled_func<true>;
} // namespace til
