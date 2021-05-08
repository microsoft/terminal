// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#ifdef UNIT_TESTING
class RunLengthEncodingTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        template<typename T, typename S, typename ParentIt>
        class rle_iterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = T;
            using pointer = T*;
            using reference = T&;
            using size_type = S;
            using difference_type = typename ParentIt::difference_type;

            explicit rle_iterator(ParentIt&& it) :
                _it(std::forward<ParentIt>(it)),
                _usage(1)
            {
            }

            [[nodiscard]] reference operator*() const noexcept
            {
                return _it->value;
            }

            [[nodiscard]] pointer operator->() const noexcept
            {
                return &operator*();
            }

            rle_iterator& operator++()
            {
                operator+=(1);
                return *this;
            }

            rle_iterator operator++(int)
            {
                rle_iterator tmp = *this;
                operator+=(1);
                return tmp;
            }

            rle_iterator& operator--()
            {
                operator-=(1);
                return *this;
            }

            rle_iterator operator--(int)
            {
                rle_iterator tmp = *this;
                operator-=(1);
                return tmp;
            }

            rle_iterator& operator+=(difference_type move)
            {
                // TODO: Optional iterator debug
                if (move >= 0) // positive direction
                {
                    // While we still need to move...
                    while (move > 0)
                    {
                        // Check how much space we have left on this run.
                        // A run that is 6 long (_it->length) and
                        // we have addressed the 4th position (_usage, starts at 1).
                        // Then there are 2 left.
                        const auto space = static_cast<difference_type>(_it->length - _usage);

                        // If we have enough space to move...
                        if (space >= move)
                        {
                            // Move the storage forward the requested distance.
                            _usage += gsl::narrow_cast<decltype(_usage)>(move);
                            move = 0;
                        }
                        // If we do NOT have enough space.
                        else
                        {
                            // Reduce the requested distance by the remaining space
                            // to count "burning out" this run.
                            // + 1 more for jumping to the next item.
                            move -= space + 1;

                            // Advance the underlying iterator.
                            ++_it;

                            // Signify we're on the first position.
                            _usage = 1;
                        }
                    }
                }
                else // negative direction
                {
                    // Flip the sign to make first just the magnitude since this
                    // branch is already the direction.
                    move = -move;

                    // While we still need to move...
                    while (move > 0)
                    {
                        // Check how much space we have used on this run.
                        // A run that is 6 long (_it->length) and
                        // we have addressed the 4th position (_usage, starts at 1).
                        // We can move to the 1st position, or 3 to the left.
                        const auto space = static_cast<difference_type>(_usage - 1);

                        // If we have enough space to move...
                        if (space >= move)
                        {
                            // Move the storage forward the requested distance.
                            _usage -= gsl::narrow_cast<decltype(_usage)>(move);
                            move = 0;
                        }
                        // If we do NOT have enough space.
                        else
                        {
                            // Reduce the requested distance by the total usage
                            // to count "burning out" this run.
                            move -= _usage;

                            // Advance the underlying iterator.
                            --_it;

                            // Signify we're on the last position.
                            _usage = _it->length;
                        }
                    }
                }
                return *this;
            }

            rle_iterator& operator-=(const difference_type offset)
            {
                return *this += -offset;
                return *this += -offset;
            }

            [[nodiscard]] rle_iterator operator+(const difference_type offset) const
            {
                auto tmp = *this;
                return tmp += offset;
            }

            [[nodiscard]] rle_iterator operator-(const difference_type offset) const
            {
                auto tmp = *this;
                return tmp -= offset;
            }

            [[nodiscard]] difference_type operator-(const rle_iterator& right) const
            {
                // TODO: Optional iterator debug

                // Hold the accumulation.
                difference_type accumulation = 0;

                // Make ourselves a copy of the right side. We'll
                auto tmp = right;

                // While we're pointing to a run that is RIGHT of tmp...
                while (_it > tmp._it)
                {
                    // Add all remaining space in tmp to the accumulation.
                    // + 1 more for jumping to the next item.
                    accumulation += tmp._it->length - tmp._usage + 1;

                    // Move tmp's iterator rightward.
                    ++tmp._it;

                    // Set first to the first position in the run.
                    tmp._usage = 1;
                }

                // While we're pointing to a run that is LEFT of tmp...
                while (_it < tmp._it)
                {
                    // Subtract all used space in tmp from the accumulation.
                    accumulation -= _usage;

                    // Move tmp's iterator leftward.
                    --tmp._it;

                    // Set first to the last position in the run.
                    tmp._usage = tmp._it->length;
                }

                // Now both iterators should be at the same position.
                // Just accumulate the difference between their usages.
                accumulation += _usage - tmp._usage;

                return accumulation;
            }

            [[nodiscard]] reference operator[](const difference_type offset) const
            {
                return *operator+(offset);
            }

            [[nodiscard]] bool operator==(const rle_iterator& right) const noexcept
            {
                // TODO: Optional iterator debug
                return _it == right._it && _usage == right._usage;
            }

            [[nodiscard]] bool operator!=(const rle_iterator& right) const noexcept
            {
                return !(*this == right);
            }

            [[nodiscard]] bool operator<(const rle_iterator& right) const noexcept
            {
                // TODO: Optional iterator debug
                return _it < right._it || (_it == right._it && _usage < right._usage);
            }

            [[nodiscard]] bool operator>(const rle_iterator& right) const noexcept
            {
                return right < *this;
            }

            [[nodiscard]] bool operator<=(const rle_iterator& right) const noexcept
            {
                return !(right < *this);
            }

            [[nodiscard]] bool operator>=(const rle_iterator& right) const noexcept
            {
                return !(*this < right);
            }

        private:
            ParentIt _it;
            size_type _usage;
        };
    } // namespace details

    template<typename T, typename S>
    struct rle_pair
    {
        using value_type = T;
        using size_type = S;

        rle_pair() = default;

        rle_pair(const rle_pair&) = default;
        rle_pair& operator=(const rle_pair&) = default;

        rle_pair(rle_pair&&) = default;
        rle_pair& operator=(rle_pair&&) = default;

        constexpr rle_pair(const T& value, const S& length) noexcept(std::is_nothrow_copy_constructible_v<T>&& std::is_nothrow_copy_constructible_v<S>) :
            value(value), length(length)
        {
        }

        constexpr rle_pair(T&& value, S&& length) noexcept(std::is_nothrow_constructible_v<T>&& std::is_nothrow_constructible_v<S>) :
            value(std::move<T>(value)), length(std::move<S>(length))
        {
        }

        constexpr void swap(rle_pair& other) noexcept(std::is_nothrow_swappable_v<T>&& std::is_nothrow_swappable_v<S>)
        {
            if (this != std::addressof(other))
            {
                std::swap(value, other.value);
                std::swap(length, other.length);
            }
        }

        value_type value{};
        size_type length{};
    };

    template <typename T, typename S>
    [[nodiscard]] constexpr bool operator==(const rle_pair<T, S>& lhs, const rle_pair<T, S>& rhs) {
        return lhs.value == rhs.value && lhs.length == rhs.length;
    }

    template <typename T, typename S>
    [[nodiscard]] constexpr bool operator!=(const rle_pair<T, S>& lhs, const rle_pair<T, S>& rhs) {
        return !(lhs == rhs);
    }

    template<typename T, typename S = std::size_t, typename Container = std::vector<rle_pair<T, S>>>
    class basic_rle
    {
    public:
        using value_type = T;
        using allocator_type = typename Container::allocator_type;
        using pointer = typename Container::pointer;
        using const_pointer = typename Container::const_pointer;
        using reference = T&;
        using const_reference = const T&;
        using size_type = S;
        using difference_type = S;

        using rle_iterator = typename Container::iterator;
        using rle_const_iterator = typename Container::const_iterator;
        using rle_type = rle_pair<value_type, size_type>;

        using const_iterator = details::rle_iterator<const T, S, rle_const_iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        static constexpr size_type npos = size_type(-1);

        // We don't check anywhere whether a size_type value is negative.
        // Having signed integers would break that.
        static_assert(std::is_unsigned<size_type>::value, "the run length S must be unsigned");
        static_assert(std::is_same<rle_type, typename Container::value_type>::value, "the value type of the Container must be rle_pair<T, S>");

        constexpr basic_rle() noexcept = default;
        ~basic_rle() = default;

        basic_rle(const basic_rle& other) = default;
        basic_rle& operator=(const basic_rle& other) = default;

        basic_rle(basic_rle&& other) noexcept :
            _runs(std::move(other._runs)), _total_length(std::exchange(other._total_length, 0))
        {
        }

        basic_rle& operator=(basic_rle&& other) noexcept
        {
            _runs = std::move(other._runs);
            _total_length = std::exchange(other._total_length, 0);
            return *this;
        }

        basic_rle(const size_type length, const value_type& value) :
            _total_length(length)
        {
            _runs.push_back({value, length});
        }

        void swap(basic_rle& other)
        {
            _runs.swap(other._runs);
            std::swap(_total_length, other._total_length);
        }

        // Returns the total length of all runs as encoded.
        size_type size() const noexcept
        {
            return _total_length;
        }

        // This method gives access to the raw run length encoded array
        // and allows users of this class to iterate over those.
        const Container& runs() const noexcept
        {
            return _runs;
        }

        // Get the value at the position
        const_reference at(size_type position) const
        {
           size_type applies;
            return at(position, applies);
        }

        // Get the value at the position
        const_reference at(size_type position, size_type& applies) const
        {
            auto begin = _runs.begin();
            auto end = _runs.end();

            rle_scanner scanner(begin, end);
            auto [it, pos] = scanner.scan(position);

            if (it == end)
            {
                throw std::out_of_range("position out of range");
            }

            applies = it->length - pos;
            return it->value;
        }

        [[nodiscard]] basic_rle slice(size_type start_index, size_type end_index) const
        {
            if (end_index > _total_length)
            {
                end_index = _total_length;
            }

            if (start_index >= end_index)
            {
                return {};
            }

            // Thanks to the prior conditions we can safely assume that:
            // * 0 <= start_index < _total_length
            // * 0 < end_index <= _total_length
            // * start_index < end_index
            //
            // --> It's safe to subtract 1 from end_index

            rle_scanner scanner(_runs.begin(), _runs.end());
            auto [begin_run, start_run_pos] = scanner.scan(start_index);
            auto [end_run, end_run_pos] = scanner.scan(end_index - 1);

            Container slice{ begin_run, end_run + 1 };
            slice.back().length = end_run_pos + 1;
            slice.front().length -= start_run_pos;

            return { std::move(slice), end_index - start_index };
        }

        // Set the range [start_index, end_index) to the given value.
        void replace(size_type start_index, size_type end_index, const value_type& value)
        {
            // start_index and end_index must be inside the inclusive range [0, _total_length].
            Expects(start_index <= end_index);

            rle_type new_run{ value, end_index - start_index };
            replace(start_index, end_index, { &new_run, 1 });
        }

        // Replace the range [start_index, end_index) with the given run.
        // NOTE: This can change the size/length of the vector.
        void replace(size_type start_index, size_type end_index, rle_type new_run)
        {
            replace(start_index, end_index, { &new_run, 1 });
        }

        // Replace the range [start_index, end_index) with the given list of runs.
        // NOTE: This can change the size/length of the vector.
        void replace(size_type start_index, size_type end_index, gsl::span<rle_type> new_runs)
        {
            if (new_runs.empty())
            {
                return;
            }

            if (end_index > _total_length)
            {
                end_index = _total_length;
            }

            // start_index and end_index must be inside the inclusive range [0, _total_length].
            Expects(start_index <= end_index);

            rle_scanner scanner{ _runs.begin(), _runs.end() };
            auto [begin, begin_pos] = scanner.scan(start_index);
            auto [end, end_pos] = scanner.scan(end_index);

            // Two complications can occur during insertion of new_runs:
            // 1. The it/end run has the same value as the preceding/succeeding run.
            //    --> The new runs must be joined with the existing runs.
            // 2. The it/end run might it/end inside an existing run.
            //    --> The existing run needs to be split up.

            // 1. The it/end run has the same value as the preceding/succeeding run.
            //    --> The new runs must be joined with the existing runs.
            size_type begin_additional_length = 0;
            size_type end_additional_length = 0;
            if (start_index != 0)
            {
                auto previous = begin_pos ? begin : begin - 1;
                if (previous->value == new_runs.front().value)
                {
                    begin_additional_length = begin_pos ? begin_pos : previous->length;
                    begin_pos = 0;
                    begin = previous;
                }
            }
            if (end_index != _total_length)
            {
                // Like all iterators in C++ "end" already points 1 item past "end_index".
                // --> No need for something analogue to "previous".
                if (end->value == new_runs.back().value)
                {
                    end_additional_length = end->length - end_pos;
                    end_pos = 0;
                    ++end;
                }
            }

            // If we have a replacement like the following:
            //   1 1 1 1 1
            // +     2 2
            // It'll result in the following _three_ runs:
            // = 1 1|2 2|1
            //           ^
            // mid_insertion_trailer contains the run (marked as "^")
            // which needs to be appended after the replacement runs.
            std::optional<rle_type> mid_insertion_trailer;
            if (begin == end && begin_pos != 0)
            {
                mid_insertion_trailer.emplace(begin->value, begin->length - end_pos);
                // We've "consumed" end_pos
                end_pos = 0;
            }

            // 2. The it/end run might it/end inside an existing run.
            //    --> The existing run needs to be split up.
            //
            // For example:
            //
            //   1 1 1|2 2
            // +     3 3
            // = 1 1|3 3|2
            //   ^ ^     ^
            // --> We must shorten the
            //     * it slice to a length of 2
            //     * end slice to a length of 1
            //
            // NOTE: iterators in C++ are in the range [it, end).
            if (begin_pos)
            {
                begin->length = begin_pos;
                // it is part of the to-be-replaced range, was "abused" to adjust the preceding run's length.
                // --> We must increment it.
                ++begin;
            }
            if (end_pos)
            {
                // end points past the to-be-replaced range and doesn't need to be decremented.
                end->length -= end_pos;
            }

            // NOTE: Due to the prior case "2." it can be greater than end!
            const size_t available_space = begin < end ? end - begin : 0;
            const size_t required_space = new_runs.size() + (mid_insertion_trailer ? 1 : 0);

            const auto begin_index = begin - _runs.begin();

            const auto new_runs_begin = new_runs.begin();
            const auto new_runs_end = new_runs.end();
            auto new_runs_it = new_runs_begin;

            // First copy over as much data as can fit into the existing [start_index, end_index) range.
            // Afterwards two situations can occur:
            //
            // * All data was copied and we have  we have more space left
            const auto direct_copy_end = new_runs_it + std::min(available_space, new_runs.size());
            begin = std::copy(new_runs_it, direct_copy_end, begin);
            new_runs_it = direct_copy_end;

            if (available_space >= required_space)
            {
                // The entirety of required_space was used up and new_runs was fully copied over.
                // We now need to .erase() the unneeded space in the underlying vector.

                if (mid_insertion_trailer)
                {
                    *begin = *std::move(mid_insertion_trailer);
                    ++begin;
                }

                end = _runs.erase(begin, end) - 1;
            }
            else
            {
                // The entirety of available_space was used up and we have remaining new_runs elements to copy over.
                // We now need to make space for the new elements.

                if (mid_insertion_trailer)
                {
                    const auto insert_pos = begin - _runs.begin();
                    _runs.insert(begin, required_space - available_space, {});
                    begin = std::copy(new_runs_it, new_runs_end, _runs.begin() + insert_pos);
                    *begin = *std::move(mid_insertion_trailer);
                }
                else
                {
                    _runs.insert(begin, new_runs_it, new_runs_end);
                }
            }

            // Due to condition "1." it's possible for two existing, neighboring runs to be joined.
            // --> We must extend the length of those existing runs.
            // NOTE: Both it and end below may point to the same run!
            if (begin_additional_length)
            {
                begin = _runs.begin() + begin_index;
                begin->length += begin_additional_length;
            }
            if (end_additional_length)
            {
                end->length += end_additional_length;
            }

            const auto pre_length = start_index;
            const auto post_length = _total_length - end_index;
            const auto new_length = std::accumulate(new_runs_begin, new_runs_end, size_type(0), [](const size_type& acc, const rle_type& run) { return acc + run.length; });
            _total_length = pre_length + post_length + new_length;
        }

        // Replaces every value seen in the run with a new one
        // Does not change the length or position of the values.
        void replace_values(const value_type& old_value, const value_type& new_value) noexcept(std::is_nothrow_copy_assignable<value_type>::value)
        {
            for (auto& run : _runs)
            {
                if (run.value == old_value)
                {
                    run.value = new_value;
                }
            }
        }

        // Adjust the size of the vector.
        // If the size is being increased, the last run is extended to fill up the new vector size.
        // If the size is being decreased, the trailing runs are cut off to fit.
        void resize_trailing_extent(const size_type new_size)
        {
            if (new_size == 0)
            {
                _runs.clear();
            }
            else if (new_size < _total_length)
            {
                rle_scanner scanner(_runs.begin(), _runs.end());
                auto [run, pos] = scanner.scan(new_size - 1);

                run->length = pos + 1;

                _runs.erase(++run, _runs.cend());
            }
            else if (new_size > _total_length)
            {
                Expects(!_runs.empty());
                auto& run = _runs.back();

                run.length += new_size - _total_length;
            }

            _total_length = new_size;
        }

        constexpr bool operator==(const basic_rle& other) const noexcept
        {
            return _total_length == other._total_length && _runs == other._runs;
        }

        constexpr bool operator!=(const basic_rle& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return const_iterator(_runs.begin());
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return const_iterator(_runs.end());
        }

        [[nodiscard]] const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }

        [[nodiscard]] const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(begin());
        }

        [[nodiscard]] const_iterator cbegin() const noexcept
        {
            return begin();
        }

        [[nodiscard]] const_iterator cend() const noexcept
        {
            return end();
        }

        [[nodiscard]] const_reverse_iterator crbegin() const noexcept
        {
            return rbegin();
        }

        [[nodiscard]] const_reverse_iterator crend() const noexcept
        {
            return rend();
        }

#ifdef UNIT_TESTING
        std::wstring to_string() const
        {
            std::wstringstream wss;
            wss << std::endl
                << L"Run of size " << size() << " contains:" << std::endl;

            for (auto& item : _runs)
            {
                wss << wil::str_printf<std::wstring>(L"[%td for %td]", item.value, item.length) << L" ";
            }

            wss << std::endl;
            return wss.str();
        }
#endif

    private:
        basic_rle(Container&& runs, size_type size) :
            _runs(std::forward<Container>(runs)),
            _total_length(size)
        {
        }

        template<typename It>
        struct rle_scanner
        {
            explicit rle_scanner(It begin, It end) noexcept :
                it(std::move(begin)), end(std::move(end)) {}

            std::pair<It, size_type> scan(size_type index) noexcept
            {
                run_pos = 0;

                for (; it != end; ++it)
                {
                    const auto new_total = total + it->length;
                    if (new_total > index)
                    {
                        run_pos = index - total;
                        break;
                    }

                    total = new_total;
                }

                return { it, run_pos };
            }

        private:
            It it;
            const It end;
            size_type run_pos = 0;
            size_type total = 0;
        };

        Container _runs;
        S _total_length{ 0 };

#ifdef UNIT_TESTING
        friend class ::RunLengthEncodingTests;
#endif
    };

    template<typename T, typename S = std::size_t>
    using rle = basic_rle<T, S, std::vector<rle_pair<T, S>>>;

#ifdef BOOST_CONTAINER_CONTAINER_SMALL_VECTOR_HPP
    template<typename T, typename S = std::size_t, std::size_t N = 1>
    using small_rle = basic_rle<T, S, boost::container::small_vector<rle_pair<T, S>, N>>;
#endif
};

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<typename T>
    class VerifyOutputTraits<::til::rle<T>>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::rle<T>& object)
        {
            return WEX::Common::NoThrowString(object.to_string().c_str());
        }
    };

    template<typename T>
    class VerifyCompareTraits<::til::rle<T>, ::til::rle<T>>
    {
    public:
        static bool AreEqual(const ::til::rle<T>& expected, const ::til::rle<T>& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::rle<T>& expected, const ::til::rle<T>& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::rle<T>& expectedLess, const ::til::rle<T>& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::rle<T>& expectedGreater, const ::til::rle<T>& expectedLess) = delete;

        static bool IsNull(const ::til::rle<T>& object) noexcept
        {
            return object == til::rle<T>{};
        }
    };

};
#endif
