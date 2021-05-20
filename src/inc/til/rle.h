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

            // TODO GH#10135: Enable checked iterators for _ITERATOR_DEBUG_LEVEL != 0.
            explicit rle_iterator(ParentIt&& it) noexcept :
                _it{ std::forward<ParentIt>(it) },
                _pos{ 0 }
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

            rle_iterator& operator++() noexcept
            {
                ++_pos;
                if (_pos == _it->length)
                {
                    ++_it;
                    _pos = 0;
                }
                return *this;
            }

            rle_iterator operator++(int) noexcept
            {
                auto tmp = *this;
                ++tmp;
                return tmp;
            }

            rle_iterator& operator--() noexcept
            {
                if (_pos == 0)
                {
                    --_it;
                    _pos = _it->length;
                }
                --_pos;
                return *this;
            }

            rle_iterator operator--(int) noexcept
            {
                auto tmp = *this;
                --tmp;
                return tmp;
            }

            rle_iterator& operator+=(difference_type move) noexcept
            {
                // Splitting our function into a forward and backward move
                // makes implementing the arithmetic quite a bit simpler.
                if (move >= 0)
                {
                    while (move > 0)
                    {
                        // If we have a run like this:
                        //   1 1 1|2 2 2|3 3 3
                        //           ^
                        // And this iterator points to ^, then space will be 2,
                        // as that's the number of times this iterator would continue
                        // yielding the number "2", if we were using operator++().
                        const auto space = static_cast<difference_type>(_it->length - _pos);

                        if (move < space)
                        {
                            // At this point: move <= std::numeric_limits<size_type>::max().
                            // --> the narrowing is safe.
                            _pos += gsl::narrow_cast<size_type>(move);
                            break;
                        }

                        move -= space;
                        ++_it;
                        _pos = 0;
                    }
                }
                else
                {
                    move = -move;

                    while (move > 0)
                    {
                        // If we have a run like this:
                        //   1 1 1|2 2 2|3 3 3
                        //           ^
                        // And this iterator points to ^, then space will be 1,
                        // as that's the number of times this iterator would continue
                        // yielding the number "2", if we were using operator--().
                        const auto space = static_cast<difference_type>(_pos);

                        if (move <= space)
                        {
                            // At this point: move <= std::numeric_limits<size_type>::max()
                            // --> the narrowing is safe.
                            _pos -= gsl::narrow_cast<size_type>(move);
                            break;
                        }

                        // When moving backwards we want to move to the last item
                        // in the previous run (that is: _pos == length - 1).
                        // --> Don't just move to the beginning of this run (-= _pos),
                        //     but actually one item further (-= 1).
                        move -= static_cast<difference_type>(_pos) + 1;
                        --_it;
                        // _pos is supposed to be in the range [0, _it->length).
                        // --> The last position in the previous run is length - 1;
                        _pos = _it->length - 1;
                    }
                }

                return *this;
            }

            rle_iterator& operator-=(const difference_type offset) noexcept
            {
                return *this += -offset;
            }

            [[nodiscard]] rle_iterator operator+(const difference_type offset) const noexcept
            {
                auto tmp = *this;
                return tmp += offset;
            }

            [[nodiscard]] rle_iterator operator-(const difference_type offset) const noexcept
            {
                auto tmp = *this;
                return tmp -= offset;
            }

            [[nodiscard]] difference_type operator-(const rle_iterator& right) const noexcept
            {
                // If we figure out which of the two iterators is "lower" (nearer to begin()) and
                // "upper" (nearer to end()), we can simplify the way we think about this algorithm:
                // The distance equals the length of all runs between lower and upper,
                // excluding the positions of the lower and upper iterator.
                //
                // For instance:
                //   1 1 1|2 2 2 2|3 3|4 4 4
                //       ^               ^
                //     lower           upper
                //   _pos == 2       _pos == 1
                //
                // The total distance equals the total length all runs that are covered by
                // lower up until (but not including) upper (here: 9), minus the number of
                // items not covered by lower (here: 2, the same as _pos), plus the ones
                // covered by upper, excluding itself (here: 1, the same as _pos).

                const auto negative = *this < right;
                const auto& lower = negative ? *this : right;
                const auto& upper = negative ? right : *this;
                difference_type distance = 0;

                for (auto it = lower._it; it < upper._it; ++it)
                {
                    distance += it->length;
                }

                distance -= lower._pos;
                distance += upper._pos;

                return negative ? -distance : distance;
            }

            [[nodiscard]] reference operator[](const difference_type offset) const noexcept
            {
                return *operator+(offset);
            }

            [[nodiscard]] bool operator==(const rle_iterator& right) const noexcept
            {
                return _it == right._it && _pos == right._pos;
            }

            [[nodiscard]] bool operator!=(const rle_iterator& right) const noexcept
            {
                return !(*this == right);
            }

            [[nodiscard]] bool operator<(const rle_iterator& right) const noexcept
            {
                return _it < right._it || (_it == right._it && _pos < right._pos);
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
            size_type _pos;
        };
    } // namespace details

    // rle_pair is a simple clone of std::pair, with one difference:
    // copy and move constructors and operators are explicitly defaulted.
    // This allows rle_pair to be std::is_trivially_copyable, if both T and S are.
    // --> rle_pair can be used with memcpy(), unlike std::pair.
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
            value(std::forward<T>(value)), length(std::forward<S>(length))
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

    template<typename T, typename S>
    [[nodiscard]] constexpr bool operator==(const rle_pair<T, S>& lhs, const rle_pair<T, S>& rhs)
    {
        return lhs.value == rhs.value && lhs.length == rhs.length;
    }

    template<typename T, typename S>
    [[nodiscard]] constexpr bool operator!=(const rle_pair<T, S>& lhs, const rle_pair<T, S>& rhs)
    {
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

        using const_iterator = details::rle_iterator<const T, S, typename Container::const_iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using rle_type = rle_pair<value_type, size_type>;
        using container = Container;

        // We don't check anywhere whether a size_type value is negative.
        // Having signed integers would break that.
        static_assert(std::is_unsigned<size_type>::value, "the run length S must be unsigned");
        static_assert(std::is_same<rle_type, typename Container::value_type>::value, "the value type of the Container must be rle_pair<T, S>");

        constexpr basic_rle() noexcept = default;
        ~basic_rle() = default;

        basic_rle(const basic_rle& other) = default;
        basic_rle& operator=(const basic_rle& other) = default;

        basic_rle(basic_rle&& other) noexcept :
            _runs(std::move(other._runs)), _total_length(other._total_length)
        {
            // C++ fun fact:
            // "std::move" actually doesn't actually promise to _really_ move stuff from A to B,
            // but rather "leaves the source in an unspecified but valid state" according to the spec.
            // Probably for the sake of performance or something.
            // Quite ironic given that the committee refuses to change the STL ABI,
            // forcing us to reinvent std::pair as til::rle_pair.
            // --> Let's assume that container behavior falls into only two categories:
            //     * Moves the underlying memory, setting .size() to 0
            //     * Leaves the source intact (basically copying it)
            //     We can detect these cases using _runs.empty() and set _total_length accordingly.
            if (other._runs.empty())
            {
                other._total_length = 0;
            }
        }

        basic_rle& operator=(basic_rle&& other) noexcept
        {
            _runs = std::move(other._runs);
            _total_length = other._total_length;

            // See basic_rle(basic_rle&&) for why this is necessary.
            if (other._runs.empty())
            {
                other._total_length = 0;
            }

            return *this;
        }

        basic_rle(std::initializer_list<rle_type> runs) :
            _runs(runs), _total_length(0)
        {
            for (const auto& run : _runs)
            {
                _total_length += run.length;
            }
        }

        basic_rle(container&& runs) :
            _runs(std::forward<container>(runs)), _total_length(0)
        {
            for (const auto& run : _runs)
            {
                _total_length += run.length;
            }
        }

        basic_rle(const size_type length, const value_type& value) :
            _total_length(length)
        {
            if (length)
            {
                _runs.emplace_back(value, length);
            }
        }

        void swap(basic_rle& other) noexcept
        {
            _runs.swap(other._runs);
            std::swap(_total_length, other._total_length);
        }

        bool empty() const noexcept
        {
            return _total_length == 0;
        }

        // Returns the total length of all runs as encoded.
        size_type size() const noexcept
        {
            return _total_length;
        }

        // This method gives access to the raw run length encoded array
        // and allows users of this class to iterate over those.
        const container& runs() const noexcept
        {
            return _runs;
        }

        // Get the value at the position
        const_reference at(size_type position) const
        {
            const auto begin = _runs.begin();
            const auto end = _runs.end();

            rle_scanner scanner(begin, end);
            auto it = scanner.scan(position).first;

            if (it == end)
            {
                throw std::out_of_range("position out of range");
            }

            return it->value;
        }

        // Returns the range [start_index, end_index) as a new vector.
        // It works just like std::string::substr(), but with absolute indices.
        [[nodiscard]] basic_rle slice(size_type start_index, size_type end_index) const noexcept
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

            container slice{ begin_run, end_run + 1 };
            slice.back().length = end_run_pos + 1;
            slice.front().length -= start_run_pos;

            return { std::move(slice), static_cast<size_type>(end_index - start_index) };
        }

        // Replace the range [start_index, end_index) with the given value.
        // If end_index is larger than size() it's set to size().
        // start_index must be smaller or equal to end_index.
        void replace(size_type start_index, size_type end_index, const value_type& value)
        {
            _check_indices(start_index, end_index);

            const rle_type replacement{ value, static_cast<size_type>(end_index - start_index) };
            _replace_unchecked(start_index, end_index, { &replacement, 1 });
        }

        // Replace the range [start_index, end_index) with the given run.
        // If end_index is larger than size() it's set to size().
        // start_index must be smaller or equal to end_index.
        void replace(size_type start_index, size_type end_index, const rle_type& replacement)
        {
            replace(start_index, end_index, { &replacement, 1 });
        }

        // Replace the range [start_index, end_index) with replacements.
        // If end_index is larger than size() it's set to size().
        // start_index must be smaller or equal to end_index.
        void replace(size_type start_index, size_type end_index, const gsl::span<const rle_type> replacements)
        {
            _check_indices(start_index, end_index);
            _replace_unchecked(start_index, end_index, replacements);
        }

        // Replaces every instance of old_value in this vector with new_value.
        void replace_values(const value_type& old_value, const value_type& new_value)
        {
            for (auto& run : _runs)
            {
                if (run.value == old_value)
                {
                    run.value = new_value;
                }
            }

            _compact();
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

                run->length = ++pos;

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
        [[nodiscard]] std::wstring to_string() const
        {
            std::wstringstream ss;
            bool beginning = true;

            for (const auto& run : _runs)
            {
                if (beginning)
                {
                    beginning = false;
                }
                else
                {
                    ss << '|';
                }

                for (size_t i = 0; i < run.length; ++i)
                {
                    if (i != 0)
                    {
                        ss << ' ';
                    }

                    ss << run.value;
                }
            }

            return ss.str();
        }
#endif

    private:
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
                    const size_type new_total = total + it->length;
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

        basic_rle(container&& runs, size_type size) :
            _runs(std::forward<container>(runs)),
            _total_length(size)
        {
        }

        void _compact()
        {
            auto it = _runs.begin();
            const auto end = _runs.end();

            if (it == end)
            {
                return;
            }

            for (auto ref = it; ++it != end; ref = it)
            {
                if (ref->value == it->value)
                {
                    ref->length += it->length;

                    while (++it != end)
                    {
                        if (ref->value == it->value)
                        {
                            ref->length += it->length;
                        }
                        else
                        {
                            *++ref = std::move(*it);
                        }
                    }

                    _runs.erase(++ref, end);
                    return;
                }
            }
        }

        inline void _check_indices(size_type start_index, size_type& end_index)
        {
            if (end_index > _total_length)
            {
                end_index = _total_length;
            }

            // start_index and end_index must be inside the inclusive range [0, _total_length].
            if (start_index > end_index)
            {
                throw std::out_of_range("start_index <= end_index");
            }
        }

        // Replace the range [start_index, end_index) with replacements.
        void _replace_unchecked(size_type start_index, size_type end_index, const gsl::span<const rle_type> replacements)
        {
            //
            //
            //
            // MUST READ: How this function (mostly) works
            // -------------------------------------------
            //
            // ## Overview
            //
            // Assuming this instance consists of:
            //   _runs == {{1, 3}, {2, 3}, {3, 3}}
            // Or shown in a more visual way:
            //   1 1 1|2 2 2|3 3 3
            //
            // If we're called with:
            //   _replace_unchecked(3, 6, {{1, 2}, {4, 1}, {2, 1}})
            // Or shown in a more visual way:
            //   1 1 1|2 2 2|3 3 3
            //       ^    ^         <-- the first ^ is "start_index" (inclusive) and the second "end_index" (exclusive)
            //       1 1|4|2        <-- the "replacements"
            //
            // This results in:
            //   1 1 1 1|4|2 2|3 3 3
            // and _total_length increases by 1.
            //
            //
            // ## Trivial algorithm
            //
            // Assuming we have the following situation:
            //   1 1 1|2 2 2|3 3 3
            //       ^    ^
            //       1 1|4|2
            //
            // A trivial algorithm can achieve this in 3-4 steps:
            // 1. Remove the to be replaced range (marked with ^).
            //    The lengths of existing runs must be modified accordingly.
            //    Resulting in:
            //      1 1|2|3 3 3
            //         ^         <-- the insertion point for replacements
            //
            // 2. (Optional) If the replaced range starts and ends within the same run,
            //    we need to split it up into two. An example can be found below.
            // 3. Add the new replacements:
            //      1 1|1 1|4|2|2|3 3 3
            // 4. Join adjacent runs together (using _compact):
            //      1 1 1 1|4|2 2|3 3 3
            //
            // An example for the optional step 2:
            //   1 1 1|2 2 2|3 3 3
            //           ^^
            //           1 1
            // Resulting in:
            //   1 1 1|2|1 1|2|3 3 3
            //         ^     ^        <-- the {2, 3} run was split up
            //
            // All 4 steps require elements in the underlying _runs vector to be shuffled around.
            // This function is long and complex, as it determines the place of insertion
            // as well as joining of adjacent runs before applying any modifications.
            //
            //
            // ## Optimized algorithm
            //
            // Note: "step N" refers to the 4 steps in previous "Trivial algorithm" section.
            //
            // There are 3 ways to reduce the cost of the trivial algorithm.
            // Before modifying the underlying _runs vector we must detect:
            // * (step 2) Whether the replaced range starts and ends within the same run,
            //   forcing us to split up a run and **add an additional element**.
            // * (step 4) "adjacent runs" which would occur after insertion.
            //   We must insert **one run less each** if either the first or last element
            //   of "replacements" is the same as it's existing successor/predecessor element.
            //   This fact is even true in case like this:
            //     1 1|2 2|1 1
            //         ^  ^
            //         1 1
            //   Resulting in a single run and the removal of 2 elements from _runs:
            //     1 1 1 1 1 1
            // * How many runs we need to insert in total (including the previous 2 points)
            //   and how many existing runs this will replace. Using this information
            //   we can merge removal (step 1) and insertion (step 3) together.
            //
            // Let's look at the example from the previous section and
            // assume we apply the previously mentioned optimizations
            // This allows us to detect the adjacent runs and turn this:
            //   1 1 1|2 2 2|3 3 3
            //       ^    ^
            //       1 1|4|2
            // Into this:
            //   1 1 1 1|2 2|3 3 3
            //          ^
            //          4
            // Our algorithm now only needs to make a single insertion into _runs.
            //
            // Let's look at the example for the optional step 2:
            //   1 1 1|2 2 2|3 3 3
            //           ^^
            //           1 1
            // We can detect early that we need to add an additional element.
            // This allows us to change it into a single insertion again:
            //   1 1 1|2|3 3 3
            //          ^
            //          1 1|2
            //
            // Similarly we can detect cases where we replace more runs than we insert.
            // For instance:
            //   1 1 1|2 2 2|3 3 3|4 4 4|5 5 5
            //           ^              ^
            //               6 6 6
            // After shortening the existing runs this is turned into a copy operation:
            //   1 1 1|2|3 3 3|4 4 4|5 5 5
            //           ^    ^
            //           6 6 6
            // And a removal of the extra space:
            //   1 1 1|2|6 6 6|4 4 4|5 5 5
            //                 ^    ^
            // Resulting in:
            //   1 1 1|2|6 6 6|5 5 5
            //
            //
            // ## Implementation
            //
            // The need to calculate the exact space requirements before insertion of new or
            // removal of existing runs requires us to have our steps in a specific order.
            //
            // [Step1]: Detect future adjacent runs.
            //   As this requires us to insert up to 2 runs less.
            //   For instance:
            //     1 1 1|2 2 2|3 3 3
            //           ^  ^
            //           1 1
            //   = 1 1 1 1 1|2|3 3 3
            //          ^-- The first run was joined in place by increasing its length by 2.
            //   This continues in [Step7].
            // [Step2]: Detect whether a run needs to be split in 2.
            //   As this requires us to insert 1 additional run.
            //   For instance:
            //     1 1 1|2 2 2|3 3 3
            //             ^^
            //             1 1
            //   = 1 1 1|2|1 1|2|3 3 3
            //                 ^-- An additional run was inserted.
            //   This continues in [Step5].
            // [Step3]: Adjust the lengths of existing runs.
            //   For instance:
            //     1 1 1|2 2 2|3 3 3
            //       ^  ^
            //       3 3
            //   = 1|3 3|2 2 2|3 3 3
            //     ^-- The first existing run was shortened by 2.
            // [Step4]: Copy over as many runs into the to-be-replaced range as possible.
            // [Step5]: If we split up a run, we must copy in the trailing end now.
            // [Step6.1]: If we still have any remaining extra space in the to-be-replaced range we need to remove it.
            // [Step6.2]: Otherwise if the space wasn't enough we need to insert the remaining runs.
            // [Step7]: Apply the additional lengths for adjacent runs.
            // [Step8]: Recalculate the _total_length.
            //
            //
            //

            // TODO GH#10135: Ensure replacements contains no runs with .length == 0.

            rle_scanner scanner{ _runs.begin(), _runs.end() };
            auto [begin, begin_pos] = scanner.scan(start_index);
            auto [end, end_pos] = scanner.scan(end_index);

            // This condition handles pure removals, where replacements.size() == 0.
            //
            // But this isn't just a shortcut optimization...
            // The remaining code in this function assumes that replacements.size() != 0
            // and will happily access replacements.front()/.back() for instance.
            // Otherwise the logic within this if condition is identical to the rest of this function.
            //
            // NOTE:
            //   Optimally the remaining code in this method should be made compatible with empty replacements.
            //   Especially since this logic is extremely similar to the one below for non-empty replacements.
            if (replacements.empty())
            {
                const size_type removed = end_index - start_index;

                if (start_index != 0 && end_index != _total_length)
                {
                    const auto previous = begin_pos ? begin : begin - 1;
                    if (previous->value == end->value)
                    {
                        end->length -= end_pos - (begin_pos ? begin_pos : previous->length);
                        begin_pos = 0;
                        end_pos = 0;
                        begin = previous;
                    }
                }

                if (begin_pos)
                {
                    begin->length = begin_pos;
                    ++begin;
                }
                if (end_pos)
                {
                    end->length -= end_pos;
                }

                _runs.erase(begin, end);
                _total_length -= removed;
                return;
            }

            // [Step1]
            size_type begin_additional_length = 0;
            size_type end_additional_length = 0;
            if (start_index != 0)
            {
                const auto previous = begin_pos ? begin : begin - 1;
                if (previous->value == replacements.front().value)
                {
                    begin_additional_length = begin_pos ? begin_pos : previous->length;
                    begin_pos = 0;
                    begin = previous;
                }
            }
            if (end_index != _total_length)
            {
                // end already points 1 item past "end_index".
                // --> No need for something analogue to "previous" above.
                if (end->value == replacements.back().value)
                {
                    end_additional_length = end->length - end_pos;
                    end_pos = 0;
                    ++end;
                }
            }

            // [Step2]
            std::optional<rle_type> mid_insertion_trailer;
            if (begin == end && begin_pos != 0)
            {
                mid_insertion_trailer.emplace(begin->value, static_cast<size_type>(begin->length - end_pos));
                // mid_insertion_trailer contains the element that will be inserted past
                // the to-be-replaced range. We must ensure that we don't accidentally
                // adjust the length of an unrelated run and thus set end_post to 0.
                end_pos = 0;
            }

            // [Step3]
            if (begin_pos)
            {
                begin->length = begin_pos;
                // begin is part of the to-be-replaced range.
                // We've used the run begin is pointing to adjust it's length.
                // --> We must increment it in order to not overwrite it in [Step4].
                ++begin;
            }
            if (end_pos)
            {
                // Similarly to before we must adjust the length,
                // but this time we don't need to decrement end, as it's
                // already pointing past the to-be-replaced range anyways.
                end->length -= end_pos;
            }

            // NOTE: It's possible for begin > end, as we increment begin in [Step3].
            const size_t available_space = begin < end ? end - begin : 0;
            const size_t required_space = replacements.size() + (mid_insertion_trailer ? 1 : 0);
            const auto begin_index = begin - _runs.begin();
            const auto replacements_begin = replacements.begin();
            const auto replacements_end = replacements.end();

            // [Step4]
            const auto direct_copy_end = replacements_begin + std::min(available_space, replacements.size());
            begin = std::copy(replacements_begin, direct_copy_end, begin);

            if (available_space >= required_space)
            {
                // [Step6.1]
                _runs.erase(begin, end);
            }
            else
            {
                if (mid_insertion_trailer)
                {
                    // Unfortunately there's no efficient way to express "insert an iterator range
                    // plus one extra element at the end" with standard vector containers.
                    // --> First make some space for N+1 elements using default initialization.
                    //     Then insert the new runs and finally the mid_insertion_trailer.
                    _runs.insert(begin, required_space - available_space, {});
                    // [Step6.2]
                    begin = std::copy(direct_copy_end, replacements_end, _runs.begin() + begin_index);
                    // [Step5]
                    *begin = *std::move(mid_insertion_trailer);
                }
                else
                {
                    // [Step6.2]
                    _runs.insert(begin, direct_copy_end, replacements_end);
                }
            }

            // [Step7]
            if (begin_additional_length)
            {
                begin = _runs.begin() + begin_index;
                begin->length += begin_additional_length;
            }
            if (end_additional_length)
            {
                end = _runs.begin() + begin_index + required_space - 1;
                end->length += end_additional_length;
            }

            // [Step8]
            _total_length -= end_index - start_index;
            for (const auto& run : replacements)
            {
                _total_length += run.length;
            }
        }

        container _runs;
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
    template<typename T, typename S, typename Container>
    class VerifyOutputTraits<::til::basic_rle<T, S, Container>>
    {
        using rle_vector = ::til::basic_rle<T, S, Container>;

    public:
        static WEX::Common::NoThrowString ToString(const rle_vector& object)
        {
            return WEX::Common::NoThrowString(object.to_string().c_str());
        }
    };

    template<typename T, typename S, typename Container>
    class VerifyCompareTraits<::til::basic_rle<T, S, Container>, ::til::basic_rle<T, S, Container>>
    {
        using rle_vector = ::til::basic_rle<T, S, Container>;

    public:
        static bool AreEqual(const rle_vector& expected, const rle_vector& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const rle_vector& expected, const rle_vector& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const rle_vector& expectedLess, const rle_vector& expectedGreater) = delete;
        static bool IsGreaterThan(const rle_vector& expectedGreater, const rle_vector& expectedLess) = delete;
        static bool IsNull(const rle_vector& object) = delete;
    };
};
#endif
