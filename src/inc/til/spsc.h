// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// til::spsc::details::arc requires std::atomic<size_type>::wait() and ::notify_one() and at the time of writing no
// STL supports these. Since both Windows and Linux offer a Futex implementation we can easily implement this though.
// On other platforms we fall back to using a std::condition_variable.
#if __cpp_lib_atomic_wait >= 201907
#define _TIL_SPSC_DETAIL_POSITION_IMPL_NATIVE 1
#elif defined(_WIN32_WINNT) && _WIN32_WINNT >= _WIN32_WINNT_WIN8
#define _TIL_SPSC_DETAIL_POSITION_IMPL_WIN 1
#elif __linux__
#define _TIL_SPSC_DETAIL_POSITION_IMPL_LINUX 1
#else
#define _TIL_SPSC_DETAIL_POSITION_IMPL_FALLBACK 1
#endif

// til: Terminal Implementation Library. Also: "Today I Learned".
// spsc: Single Producer Single Consumer. A SPSC queue/channel sends data from exactly one sender to one receiver.
namespace til::spsc
{
    using size_type = uint32_t;

    namespace details
    {
        inline constexpr size_type position_mask = std::numeric_limits<size_type>::max() >> 2u; // 0b00111....
        inline constexpr size_type revolution_flag = 1u << (std::numeric_limits<size_type>::digits - 2u); // 0b01000....
        inline constexpr size_type drop_flag = 1u << (std::numeric_limits<size_type>::digits - 1u); // 0b10000....

        struct block_initially_policy
        {
            using _spsc_policy = int;
            static constexpr bool _block_forever = false;
        };

        struct block_forever_policy
        {
            using _spsc_policy = int;
            static constexpr bool _block_forever = true;
        };

        template<typename WaitPolicy>
        using enable_if_wait_policy_t = typename std::remove_reference_t<WaitPolicy>::_spsc_policy;

#if _TIL_SPSC_DETAIL_POSITION_IMPL_NATIVE
        using atomic_size_type = std::atomic<size_type>;
#else
        // atomic_size_type is a fallback if native std::atomic<size_type>::wait()
        // and ::notify_one() methods are unavailable in the STL.
        struct atomic_size_type
        {
            size_type load(std::memory_order order) const noexcept
            {
                return _value.load(order);
            }

            void store(size_type desired, std::memory_order order) noexcept
            {
#if _TIL_SPSC_DETAIL_POSITION_IMPL_FALLBACK
                // We must use a lock here to prevent us from modifying the value
                // in between wait() reading the value and the thread being suspended.
                std::lock_guard<std::mutex> lock{ _m };
#endif
                _value.store(desired, order);
            }

            void wait(size_type old, [[maybe_unused]] std::memory_order order) const noexcept
            {
#if _TIL_SPSC_DETAIL_POSITION_IMPL_WIN
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile
                WaitOnAddress(const_cast<std::atomic<size_type>*>(&_value), &old, sizeof(_value), INFINITE);
#elif _TIL_SPSC_DETAIL_POSITION_IMPL_LINUX
                futex(FUTEX_WAIT_PRIVATE, old);
#elif _TIL_SPSC_DETAIL_POSITION_IMPL_FALLBACK
                std::unique_lock<std::mutex> lock{ _m };
                _cv.wait(lock, [&]() { return _value.load(order) != old; });
#endif
            }

            void notify_one() noexcept
            {
#if _TIL_SPSC_DETAIL_POSITION_IMPL_WIN
                WakeByAddressSingle(&_value);
#elif _TIL_SPSC_DETAIL_POSITION_IMPL_LINUX
                futex(FUTEX_WAKE_PRIVATE, 1);
#elif _TIL_SPSC_DETAIL_POSITION_IMPL_FALLBACK
                _cv.notify_one();
#endif
            }

        private:
#if _TIL_SPSC_DETAIL_POSITION_IMPL_LINUX
            inline void futex(int futex_op, size_type val) const noexcept
            {
                // See: https://man7.org/linux/man-pages/man2/futex.2.html
                static_assert(sizeof(std::atomic<size_type>) == 4);
                syscall(SYS_futex, &_value, futex_op, val, nullptr, nullptr, 0);
            }
#endif

            std::atomic<size_type> _value{ 0 };

#if _TIL_SPSC_DETAIL_POSITION_IMPL_FALLBACK
        private:
            std::mutex _m;
            std::condition_variable _cv;
#endif
        };
#endif

        template<typename T>
        inline T* alloc_raw_memory(size_t size)
        {
            constexpr auto alignment = alignof(T);
            if constexpr (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                return static_cast<T*>(::operator new(size));
            }
            else
            {
                return static_cast<T*>(::operator new(size, static_cast<std::align_val_t>(alignment)));
            }
        }

        template<typename T>
        inline void free_raw_memory(T* ptr) noexcept
        {
            constexpr auto alignment = alignof(T);
            if constexpr (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(ptr);
            }
            else
            {
                ::operator delete(ptr, static_cast<std::align_val_t>(alignment));
            }
        }

        struct acquisition
        {
            // The index range [begin, end) is the range of slots in the array returned by
            // arc<T>::data() that may be written to / read from respectively.
            // If a range has been successfully acquired "end > begin" is true. end thus can't be 0.
            size_type begin;
            size_type end;

            // Upon release() of an acquisition, next is the value that's written to the consumer/producer position.
            // It's basically the same as end, but with the revolution flag mixed in.
            // If end is equal to capacity, next will be 0 (mixed with the next revolution flag).
            size_type next;

            // If the other side of the queue hasn't been destroyed yet, alive will be true.
            bool alive;

            constexpr acquisition(size_type begin, size_type end, size_type next, bool alive) :
                begin(begin),
                end(end),
                next(next),
                alive(alive)
            {
            }
        };

        // The following assumes you know what ring/circular buffers are. You can read about them here:
        //   https://en.wikipedia.org/wiki/Circular_buffer
        //
        // Furthermore the implementation solves a problem known as the producer-consumer problem:
        //   https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem
        //
        // arc follows the classic spsc design and manages a ring buffer with two positions: _producer and _consumer.
        // They contain the position the producer / consumer will next write to / read from respectively.
        // As usual with ring buffers, these positions are modulo to the _capacity of the underlying buffer.
        // The producer's writable range is [_producer, _consumer) and the consumer's readable is [_consumer, _producer).
        //
        // After you wrote the numbers 0 to 6 into a queue of size 10, a typical state of the ring buffer might be:
        //   [ 0 | 1 | 2 | 3 | 4 | 5 | 6 | _ | _ | _ | _ ]
        //     ^                           ^             ^
        // _consumer = 0            _producer = 7    _capacity = 10
        //
        // As you can see the readable range currently is [_consumer, _producer) = [0, 7).
        // The remaining writable range on the other hand is [_producer, _consumer) = [7, 0).
        // Wait, what? [7, 0)? How does that work? As all positions are modulo _capacity, 0 mod 10 is the same as 10 mod 10.
        // If we only want to read forward in the buffer [7, 0) is thus the same as [7, 10).
        //
        // If we read 3 items from the queue the contents will be:
        //   [ _ | _ | _ | 3 | 4 | 5 | 6 | _ | _ | _ | _ ]
        //                 ^               ^
        //          _consumer = 3      _producer = 7
        //
        // Now the writable range is still [_producer, _consumer), but it wraps around the end of the ring buffer.
        // In this case arc will split the range in two and return each separately in acquire().
        // The first returned range will be [_producer, _capacity) and the second [0, _consumer).
        // The same logic applies if the readable range wraps around the end of the ring buffer.
        //
        // As these are symmetric, the logic for acquiring and releasing ranges is the same for both sides.
        // The producer will acquire() and release() ranges with its own position as "mine" and the consumer's
        // position as "theirs". These arguments are correspondingly flipped for the consumer.
        //
        // As part of the producer-consumer problem, the producer cannot write more values ahead of the
        // consumer than the buffer's capacity. Since both positions are modulo to the capacity we can only
        // determine positional differences smaller than the capacity. Due to that both producer and
        // consumer store a "revolution_flag" as the second highest bit within their positions.
        // This bit is flipped each time the producer/consumer wrap around the end of the ring buffer.
        // If the positions are identical, except for their "revolution_flag" value, the producer thus must
        // be capacity-many positions ahead of the consumer and must wait until items have been consumed.
        //
        // Inversely the consumer must wait until the producer has written at least one value ahead.
        // This can be detected by checking whether the positions are identical including the revolution_flag.
        template<typename T>
        struct arc
        {
            explicit arc(size_type capacity) noexcept :
                _data(alloc_raw_memory<T>(static_cast<size_t>(capacity) * sizeof(T))),
                _capacity(capacity)
            {
            }

            ~arc()
            {
                auto beg = _consumer.load(std::memory_order_acquire);
                auto end = _producer.load(std::memory_order_acquire);
                auto differentRevolution = ((beg ^ end) & revolution_flag) != 0;

                beg &= position_mask;
                end &= position_mask;

                // The producer position will always be ahead of the consumer, but since we're dealing
                // with a ring buffer the producer may be wrapped around the end of the buffer.
                // We thus need to deal with 3 potential cases:
                // * No valid data.
                //   If both positions including their revolution bits are identical.
                // * Valid data in the middle of the ring buffer.
                //   If _producer > _consumer.
                // * Valid data at both ends of the ring buffer.
                //   If the revolution bits differ, even if the positions are otherwise identical,
                //   which they might be if the channel contains exactly as many values as its capacity.
                if (end > beg)
                {
                    std::destroy(_data + beg, _data + end);
                }
                else if (differentRevolution)
                {
                    std::destroy(_data, _data + end);
                    std::destroy(_data + beg, _data + _capacity);
                }

                free_raw_memory(_data);
            }

            void drop_producer()
            {
                drop(_producer);
            }

            void drop_consumer()
            {
                drop(_consumer);
            }

            acquisition producer_acquire(size_type slots, bool blocking) noexcept
            {
                return acquire(_producer, _consumer, revolution_flag, slots, blocking);
            }

            void producer_release(acquisition acquisition) noexcept
            {
                release(_producer, acquisition);
            }

            acquisition consumer_acquire(size_type slots, bool blocking) noexcept
            {
                return acquire(_consumer, _producer, 0, slots, blocking);
            }

            void consumer_release(acquisition acquisition) noexcept
            {
                release(_consumer, acquisition);
            }

            T* data() const noexcept
            {
                return _data;
            }

        private:
            void drop(atomic_size_type& mine)
            {
                // Signal the other side we're dropped. See acquire() for the handling of the drop_flag.
                // We don't need to use release ordering like release() does as each call to
                // any of the producer/consumer methods already results in a call to release().
                // Another release ordered write can't possibly synchronize any more data anyways at this point.
                const auto myPos = mine.load(std::memory_order_relaxed);
                mine.store(myPos | drop_flag, std::memory_order_relaxed);
                mine.notify_one();

                // The first time SPSCBase is dropped (destroyed) we'll set
                // the flag to true and get false, causing us to return early.
                // Only the second time we'll get true.
                // --> The contents are only deleted when both sides have been dropped.
                if (_eitherSideDropped.exchange(true, std::memory_order_relaxed))
                {
                    delete this;
                }
            }

            // NOTE: waitMask MUST be either 0 (consumer) or revolution_flag (producer).
            acquisition acquire(const atomic_size_type& mine, const atomic_size_type& theirs, size_type waitMask, size_type slots, bool blocking) const noexcept
            {
                size_type myPos = mine.load(std::memory_order_relaxed);
                size_type theirPos;

                while (true)
                {
                    // This acquire read synchronizes with the release write in release().
                    theirPos = theirs.load(std::memory_order_acquire);
                    if ((myPos ^ theirPos) != waitMask)
                    {
                        break;
                    }
                    if (!blocking)
                    {
                        return {
                            0,
                            0,
                            0,
                            true,
                        };
                    }

                    theirs.wait(theirPos, std::memory_order_relaxed);
                }

                // If the other side's position contains a drop flag, as a X -> we need to...
                // * producer -> stop immediately
                //   FYI: isProducer == (waitMask != 0).
                // * consumer -> finish consuming all values and then stop
                //   We're finished if the only difference between our
                //   and the other side's position is the drop flag.
                if ((theirPos & drop_flag) != 0 && (waitMask != 0 || (myPos ^ theirPos) == drop_flag))
                {
                    return {
                        0,
                        0,
                        0,
                        false,
                    };
                }

                auto begin = myPos & position_mask;
                auto end = theirPos & position_mask;

                // [begin, end) is the writable/readable range for the producer/consumer.
                // The following detects whether we'd be wrapping around the end of the ring buffer
                // and splits the range into the first half [mine, _capacity).
                // If acquire() is called again it'll return [0, theirs).
                end = end > begin ? end : _capacity;

                // Of course we also need to ensure to not return more than we've been asked for.
                end = std::min(end, begin + slots);

                // "next" will contain the value that's stored into "mine" when release() is called.
                // It's basically the same as "end", but with the revolution flag spliced in.
                // If we acquired the range [mine, _capacity) "end" will equal _capacity
                // and thus wrap around the ring buffer. The next value for "mine" is thus the
                // position zero | the flipped "revolution" (and 0 | x == x).
                auto revolution = myPos & revolution_flag;
                auto next = end != _capacity ? end | revolution : revolution ^ revolution_flag;

                return {
                    begin,
                    end,
                    next,
                    true,
                };
            }

            void release(atomic_size_type& mine, acquisition acquisition) noexcept
            {
                // This release write synchronizes with the acquire read in acquire().
                mine.store(acquisition.next, std::memory_order_release);
                mine.notify_one();
            }

            T* const _data;
            const size_type _capacity;

            std::atomic<bool> _eitherSideDropped{ false };

            atomic_size_type _producer;
            atomic_size_type _consumer;
        };

        inline void validate_size(size_t v)
        {
            if (v > static_cast<size_t>(position_mask))
            {
                throw std::overflow_error{ "size too large for spsc" };
            }
        }
    }

    // Block until at least one item has been written into the sender / read from the receiver.
    inline constexpr details::block_initially_policy block_initially{};

    // Block until all items have been written into the sender / read from the receiver.
    inline constexpr details::block_forever_policy block_forever{};

    template<typename T>
    struct producer
    {
        explicit producer(details::arc<T>* arc) noexcept :
            _arc(arc) {}

        producer<T>(const producer<T>&) = delete;
        producer<T>& operator=(const producer<T>&) = delete;

        producer(producer<T>&& other) noexcept
        {
            drop();
            _arc = std::exchange(other._arc, nullptr);
        }

        producer<T>& operator=(producer<T>&& other) noexcept
        {
            drop();
            _arc = std::exchange(other._arc, nullptr);
            return *this;
        }

        ~producer()
        {
            drop();
        }

        // emplace constructs an item in-place at the end of the queue.
        // It returns true, if the item was successfully placed within the queue.
        // The return value will be false, if the consumer is gone.
        template<typename... Args>
        bool emplace(Args&&... args) const
        {
            auto acquisition = _arc->producer_acquire(1, true);
            if (!acquisition.end)
            {
                return false;
            }

            auto data = _arc->data();
            auto begin = data + acquisition.begin;
            new (begin) T(std::forward<Args>(args)...);

            _arc->producer_release(acquisition);
            return true;
        }

        template<typename InputIt>
        std::pair<size_t, bool> push(InputIt first, InputIt last) const
        {
            return push_n(block_forever, first, std::distance(first, last));
        }

        // push writes the items between first and last into the queue.
        // The amount of successfully written items is returned as the first pair field.
        // The second pair field will be false if the consumer is gone.
        template<typename WaitPolicy, typename InputIt, details::enable_if_wait_policy_t<WaitPolicy> = 0>
        std::pair<size_t, bool> push(WaitPolicy&& policy, InputIt first, InputIt last) const
        {
            return push_n(std::forward<WaitPolicy>(policy), first, std::distance(first, last));
        }

        template<typename InputIt>
        std::pair<size_t, bool> push_n(InputIt first, size_t count) const
        {
            return push_n(block_forever, first, count);
        }

        // push_n writes count items from first into the queue.
        // The amount of successfully written items is returned as the first pair field.
        // The second pair field will be false if the consumer is gone.
        template<typename WaitPolicy, typename InputIt, details::enable_if_wait_policy_t<WaitPolicy> = 0>
        std::pair<size_t, bool> push_n(WaitPolicy&&, InputIt first, size_t count) const
        {
            details::validate_size(count);

            const auto data = _arc->data();
            auto remaining = static_cast<size_type>(count);
            auto blocking = true;
            auto ok = true;

            while (remaining != 0)
            {
                auto acquisition = _arc->producer_acquire(remaining, blocking);
                if (!acquisition.end)
                {
                    ok = acquisition.alive;
                    break;
                }

                const auto begin = data + acquisition.begin;
                const auto got = acquisition.end - acquisition.begin;
                std::uninitialized_copy_n(first, got, begin);
                first += got;
                remaining -= got;

                _arc->producer_release(acquisition);

                if constexpr (!std::remove_reference_t<WaitPolicy>::_block_forever)
                {
                    blocking = false;
                }
            }

            return { count - remaining, ok };
        }

    private:
        void drop()
        {
            if (_arc)
            {
                _arc->drop_producer();
            }
        }

        details::arc<T>* _arc = nullptr;
    };

    template<typename T>
    struct consumer
    {
        explicit consumer(details::arc<T>* arc) noexcept :
            _arc(arc) {}

        consumer<T>(const consumer<T>&) = delete;
        consumer<T>& operator=(const consumer<T>&) = delete;

        consumer(consumer<T>&& other) noexcept
        {
            drop();
            _arc = std::exchange(other._arc, nullptr);
        }

        consumer<T>& operator=(consumer<T>&& other) noexcept
        {
            drop();
            _arc = std::exchange(other._arc, nullptr);
            return *this;
        }

        ~consumer()
        {
            drop();
        }

        // pop returns the next item in the queue, or std::nullopt if the producer is gone.
        std::optional<T> pop() const
        {
            auto acquisition = _arc->consumer_acquire(1, true);
            if (!acquisition.end)
            {
                return std::nullopt;
            }

            auto data = _arc->data();
            auto begin = data + acquisition.begin;

            auto item = std::move(*begin);
            std::destroy_at(begin);

            _arc->consumer_release(acquisition);
            return item;
        }

        template<typename OutputIt>
        std::pair<size_t, bool> pop_n(OutputIt first, size_t count) const
        {
            return pop_n(block_forever, first, count);
        }

        // pop_n reads up to count items into first.
        // The amount of successfully read items is returned as the first pair field.
        // The second pair field will be false if the consumer is gone.
        template<typename WaitPolicy, typename OutputIt, details::enable_if_wait_policy_t<WaitPolicy> = 0>
        std::pair<size_t, bool> pop_n(WaitPolicy&&, OutputIt first, size_t count) const
        {
            details::validate_size(count);

            const auto data = _arc->data();
            auto remaining = static_cast<size_type>(count);
            auto blocking = true;
            auto ok = true;

            while (remaining != 0)
            {
                auto acquisition = _arc->consumer_acquire(remaining, blocking);
                if (!acquisition.end)
                {
                    ok = acquisition.alive;
                    break;
                }

                auto beg = data + acquisition.begin;
                auto end = data + acquisition.end;
                auto got = acquisition.end - acquisition.begin;
                first = std::move(beg, end, first);
                std::destroy(beg, end);
                remaining -= got;

                _arc->consumer_release(acquisition);

                if constexpr (!std::remove_reference_t<WaitPolicy>::_block_forever)
                {
                    blocking = false;
                }
            }

            return { count - remaining, ok };
        }

    private:
        void drop()
        {
            if (_arc)
            {
                _arc->drop_consumer();
            }
        }

        details::arc<T>* _arc = nullptr;
    };

    // channel returns a bounded, lock-free, single-producer, single-consumer
    // FIFO queue ("channel") with the given maximum capacity.
    template<typename T>
    std::pair<producer<T>, consumer<T>> channel(uint32_t capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument{ "invalid capacity" };
        }

        const auto arc = new details::arc<T>(capacity);
        return { std::piecewise_construct, std::forward_as_tuple(arc), std::forward_as_tuple(arc) };
    }
}
