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
            // If you use this as a sample for your own iterator, this looks
            // a bit daunting. But it's almost entirely boilerplate.
            // All you actually have to fill in is:
            // A. size_type might not be necessary for you. It can be inferred
            //    from our parent so I defined it.
            // 1. value_type, pointer, reference, and difference type. These
            //    specify the overall types. They're generally what you want to see
            //    when someone does *iterator or iterator-> or it1 - it2.
            //    If you have half an idea of what those return types should be,
            //    define them at the top or better yet, infer them from the underlying
            //    data source.
            // 2. Fill in operator*() and operator->() pointing directly at the data value.
            // 3. Fill in inc() and dec(). That gives you ++it, it++, --it, and it--.
            // 4. Fill in operator+=(). That gives you +=, +, -=, and -.
            // ALTERNATIVE 3/4. You might be able to just define += and then feed the rest into it
            //    depending on your circumstance.
            // 5. Fill in operator-() for a difference between two instances.
            // 6. Fill in operator[] to go to the offset like an array index.
            // 7. Fill in operator== for equality. Gets == and != in one shot.
            // 8. Fill in operator< for comparison. Also covers > and <= and >=.

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
                return _it->first;
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

            rle_iterator& operator+=(const difference_type offset)
            {
                // TODO: Optional iterator debug
                if (offset < 0) // negative direction
                {
                    // Hold a running count of how much more we need to move.
                    // Flip the sign to make it just the magnitude since this
                    // branch is already the direction.
                    auto move = static_cast<difference_type>(-offset);

                    // While we still need to move...
                    while (move > 0)
                    {
                        // Check how much space we have used on this run.
                        // A run that is 6 long (_it->second) and
                        // we have addressed the 4th position (_usage, starts at 1).
                        // We can move to the 1st position, or 3 to the left.
                        const auto space = static_cast<difference_type>(_usage - 1);

                        // If we have enough space to move...
                        if (space >= move)
                        {
                            // Move the storage forward the requested distance.
                            _usage -= gsl::narrow_cast<decltype(_usage)>(move);

                            // Remove the moved distance.
                            move -= move;
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
                            _usage = _it->second;
                        }
                    }
                }
                else // positive direction
                {
                    // Hold a running count of how much more we need to move.
                    auto move = static_cast<difference_type>(offset);

                    // While we still need to move...
                    while (move > 0)
                    {
                        // Check how much space we have left on this run.
                        // A run that is 6 long (_it->second) and
                        // we have addressed the 4th position (_usage, starts at 1).
                        // Then there are 2 left.
                        const auto space = static_cast<difference_type>(_it->second - _usage);

                        // If we have enough space to move...
                        if (space >= move)
                        {
                            // Move the storage forward the requested distance.
                            _usage += gsl::narrow_cast<decltype(_usage)>(move);

                            // Remove the moved distance.
                            move -= move;
                        }
                        // If we do NOT have enough space.
                        else
                        {
                            // Reduce the requested distance by the remaining space
                            // to count "burning out" this run.
                            // + 1 more for jumping to the next list item.
                            move -= space + 1;

                            // Advance the underlying iterator.
                            ++_it;

                            // Signify we're on the first position.
                            _usage = 1;
                        }
                    }
                }
                return *this;
            }

            rle_iterator& operator-=(const difference_type offset)
            {
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
                    // + 1 more for jumping to the next list item.
                    accumulation += tmp._it->second - tmp._usage + 1;

                    // Move tmp's iterator rightward.
                    ++tmp._it;

                    // Set it to the first position in the run.
                    tmp._usage = 1;
                }

                // While we're pointing to a run that is LEFT of tmp...
                while (_it < tmp._it)
                {
                    // Subtract all used space in tmp from the accumulation.
                    accumulation -= _usage;

                    // Move tmp's iterator leftward.
                    --tmp._it;

                    // Set it to the last position in the run.
                    tmp._usage = tmp._it->second;
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
    };

    template<typename T, typename S = std::size_t, typename Container = std::vector<std::pair<T, S>>>
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

        constexpr basic_rle() noexcept = default;

        basic_rle(const basic_rle& other) :
            _list(other._list), _size(other._size)
        {
        }

        basic_rle& operator=(const basic_rle& other)
        {
            _list = other._list;
            _size = other._size;
            return *this;
        }

        basic_rle(basic_rle&& other) noexcept :
            _list(std::move(other._list)), _size(std::exchange(other._size, 0))
        {
        }

        basic_rle& operator=(basic_rle&& other) noexcept
        {
            _list = std::move(other._list);
            _size = std::exchange(other._size, 0);
            return *this;
        }

        basic_rle(const size_type size, const value_type& value) :
            _size(size)
        {
            assign(value);
        }

        void swap(basic_rle& other)
        {
            std::swap(_list, other._list);
            std::swap(_size, other._size);
        }

        // Returns the total length of all runs as encoded.
        size_type size() const noexcept
        {
            return _size;
        }

        // Get the value at the position
        const_reference at(size_type position) const
        {
            size_type applies;
            THROW_HR_IF(E_INVALIDARG, position >= _size);
            return _at(position, applies)->first;
        }

        [[nodiscard]] basic_rle substr(const size_type offset = 0, const size_type count = std::numeric_limits<size_type>::max()) const
        {
            // TODO: validate params
            const size_type startIndex = offset;
            const size_type endIndex = std::min(_size - offset, count) + offset - 1;

            size_type startApplies, endApplies;
            const auto firstRun{ _at(startIndex, startApplies) };
            const auto lastRun{ _at(endIndex, endApplies) };

            Container substring{ firstRun, lastRun + 1 };
            substring.front().second = startApplies;
            substring.back().second = substring.back().second - endApplies + 1;

            return { std::move(substring), endIndex - startIndex + 1 };
        }

        // Replaces every value seen in the run with a new one
        // Does not change the length or position of the values.
        void replace(const value_type& oldValue, const value_type& newValue) noexcept(std::is_nothrow_copy_assignable<value_type>::value)
        {
            for (auto& run : _list)
            {
                if (run.first == oldValue)
                {
                    run.first = newValue;
                }
            }
        }

        // Adjust the size of the run.
        // If new size is bigger, the last value is extended to new width.
        // If new size is smaller, the runs are cut to fit.
        void resize(const size_type newSize)
        {
            if (newSize <= 0)
            {
                _list.clear();
            }
            // Easy case. If the new row is longer, increase the length of the last run by how much new space there is.
            else if (newSize > _size)
            {
                // Get the attribute that covers the final column of old width.
                auto& run = _list.back();

                // Extend its length by the additional columns we're adding.
                run.second = run.second + newSize - _size;

                // Store that the new total width we represent is the new width.
                _size = newSize;
            }
            // harder case: new row is shorter.
            else
            {
                // Get the attribute that covers the final column of the new width
                size_type applies = 0;
                auto run = _at(newSize - 1, applies);

                // applies was given to us as "how many columns left from this point forward are covered by the returned run"
                // So if the original run was B5 covering a 5 size OldWidth and we have a newSize of 3
                // then when we called FindAttrIndex, it returned the B5 as the pIndexedRun and a 2 for how many more segments it covers
                // after and including the 3rd column.
                // B5-2 = B3, which is what we desire to cover the new 3 size buffer.
                //
                // The const_cast doesn't invoke undefined behavior as this method,
                // the this pointer and thus _list in turn aren't actually const here.
                // It saves us from having to specialize _at() for non-const though.
                const_cast<size_type&>(run->second) = run->second - applies + 1;

                // Store that the new total width we represent is the new width.
                _size = newSize;

                // Erase segments after the one we just updated.
                _list.erase(run + 1, _list.cend());

                // NOTE: Under some circumstances here, we have leftover run segments in memory or blank run segments
                // in memory. We're not going to waste time redimensioning the array in the heap. We're just noting that the useful
                // portions of it have changed.
            }
        }

        // Sets this value at the given start position until the end of this vector.
        void assign(const value_type& value, const size_type start = 0)
        {
            assign(value, start, gsl::narrow_cast<size_type>(_size - start));
        }

        // Sets this value at the given start position for the given run length.
        void assign(const value_type& value, const size_type start, const size_type length)
        {
            // TODO: validate position in bounds?
            std::array<std::pair<T, S>, 1> items{{ value, length }};
            _merge(items.begin(), items.end(), start);
        }

        constexpr bool operator==(const basic_rle& other) const noexcept
        {
            return _size == other._size && _list == other._list;
        }

        constexpr bool operator!=(const basic_rle& other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] const_iterator begin() const noexcept
        {
            return const_iterator(_list.begin());
        }

        [[nodiscard]] const_iterator end() const noexcept
        {
            return const_iterator(_list.end());
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
        [[nodiscard]] typename std::enable_if<std::is_arithmetic<value_type>::value, std::wstring>::type to_string() const
        {
            std::wstringstream wss;
            wss << _size << " items:";
            for (auto& item : *this)
            {
                wss << L" " << item;
            }
            return wss.str();
        }
#endif

    private:
        basic_rle(Container&& list, size_type size) :
            _list(std::forward<Container>(list)),
            _size(size)
        {
        }

        auto _at(size_type position, size_type& applies) const
        {
            FAIL_FAST_IF(!(position < _size)); // The requested index cannot be longer than the total length described by this set of Attrs.

            size_type totalLength = 0;

            FAIL_FAST_IF(!(_list.size() > 0)); // There should be a non-zero and positive number of items in the array.

            // Scan through the internal array from position 0 adding up the lengths that each attribute applies to
            auto runPos = _list.begin();
            do
            {
                totalLength += runPos->second;

                if (totalLength > position)
                {
                    // If we've just passed up the requested position with the length we added, break early
                    break;
                }

                runPos++;
            } while (runPos < _list.end());

            // we should have broken before falling out the while case.
            // if we didn't break, then this ATTR_ROW wasn't filled with enough attributes for the entire row of characters
            FAIL_FAST_IF(runPos >= _list.end());

            // The remaining iterator position is the position of the attribute that is applicable at the position requested (position)
            // Calculate its remaining applicability if requested

            // The length on which the found attribute applies is the total length seen so far minus the position we were searching for.
            FAIL_FAST_IF(!(totalLength > position)); // The length of all attributes we counted up so far should be longer than the position requested or we'll underflow.

            applies = totalLength - position;
            FAIL_FAST_IF(!(applies > 0)); // An attribute applies for >0 characters

            return runPos;
        }

        template<typename InputIterator>
        void _merge(InputIterator first, InputIterator last, const size_type startIndex)
        {
            // NOTE: This method assumes that the run length encoded slice in (first, last) is compact.
            //       For instance if (first, last) contains [A2, A1, B3] instead of [A3, B3] it'll break this class.
            //
            // Definitions:
            // Existing Run = The run length encoded color array we're already storing in memory before this was called.
            // Insert Run = The run length encoded color array that someone is asking us to inject into our stored memory run.
            // New Run = The run length encoded color array that we have to allocate and rebuild to store internally
            //           which will replace Existing Run at the end of this function.
            // Example:
            // _size = 10.
            // Existing Run: R3 -> G5 -> B2
            // Insert Run: Y1 -> N1 at startIndex = 5 and endIndex = 6
            //            (rgInsertAttrs is a 2 length array with Y1->N1 in it and cInsertAttrs = 2)
            // Final Run: R3 -> G2 -> Y1 -> N1 -> G1 -> B2

            const auto newItemsTotalCoverage = std::accumulate(first, last, (size_type)0, [](size_type value, auto& item) -> size_type {
                return value + gsl::narrow_cast<size_type>(item.second);
            });

            const auto newItemsSize = std::distance(first, last);

            // If the insertion size is 1, do some pre-processing to
            // see if we can get this done quickly.
            if (newItemsSize == 1)
            {
                // Get the new color attribute we're trying to apply
                const value_type NewAttr = first->first;

                // If the existing run was only 1 element...
                // ...and the new color is the same as the old, we don't have to do anything and can exit quick.
                if (_list.size() == 1 && _list.at(0).first == NewAttr)
                {
                    return;
                }
                // .. otherwise if we internally have a list of 2 or more and we're about to insert a single color
                // it's possible that we just walk left-to-right through the row and find a quick exit.
                if (startIndex >= 0 && newItemsTotalCoverage == 1)
                {
                    // First we try to find the run where the insertion happens, using lowerBound and upperBound to track
                    // where we are currently at.
                    const auto begin = _list.begin();
                    size_type lowerBound = 0;
                    size_type upperBound = 0;
                    for (size_type i = 0; i < _list.size(); i++)
                    {
                        const auto curr = begin + i;
                        upperBound += curr->second;

                        if (startIndex >= lowerBound && startIndex < upperBound)
                        {
                            // The run that we try to insert into has the same color as the new one.
                            // e.g.
                            // AAAAABBBBBBBCCC
                            //       ^
                            // AAAAABBBBBBBCCC
                            //
                            // 'B' is the new color and '^' represents where startIndex is. We don't have to
                            // do anything.
                            if (curr->first == NewAttr)
                            {
                                return;
                            }

                            // If the current run has length of exactly one, we can simply change the attribute
                            // of the current run.
                            // e.g.
                            // AAAAABCCCCCCCCC
                            //      ^
                            // AAAAADCCCCCCCCC
                            //
                            // Here 'D' is the new color.
                            if (curr->second == 1)
                            {
                                curr->first = NewAttr;
                                return;
                            }

                            // If the insertion happens at current run's lower boundary...
                            if (startIndex == lowerBound && i > 0)
                            {
                                const auto prev = std::prev(curr, 1);
                                // ... and the previous run has the same color as the new one, we can
                                // just adjust the counts in the existing two elements in our internal list.
                                // e.g.
                                // AAAAABBBBBBBCCC
                                //      ^
                                // AAAAAABBBBBBCCC
                                //
                                // Here 'A' is the new color.
                                if (NewAttr == prev->first)
                                {
                                    prev->second++;
                                    curr->second--;

                                    // If we just reduced the right half to zero, just erase it out of the list.
                                    if (curr->second == 0)
                                    {
                                        _list.erase(curr);
                                    }

                                    return;
                                }
                            }

                            // If the insertion happens at current run's upper boundary...
                            if (startIndex == upperBound - 1 && i + 1 < _list.size())
                            {
                                // ...then let's try our luck with the next run if possible. This is basically the opposite
                                // of what we did with the previous run.
                                // e.g.
                                // AAAAAABBBBBBCCC
                                //      ^
                                // AAAAABBBBBBBCCC
                                //
                                // Here 'B' is the new color.
                                const auto next = std::next(curr, 1);
                                if (NewAttr == next->first)
                                {
                                    curr->second--;
                                    next->second++;

                                    if (curr->second == 0)
                                    {
                                        _list.erase(curr);
                                    }

                                    return;
                                }
                            }
                        }

                        // Advance one run in the _list.
                        lowerBound = upperBound;

                        // The lowerBound is larger than startIndex, which means we fail to find an early exit at the run
                        // where the insertion happens. We can just break out.
                        if (lowerBound > startIndex)
                        {
                            break;
                        }
                    }
                }
            }

            // If we're about to cover the entire existing run with a new one, we can also make an optimization.
            if (startIndex == 0 && newItemsTotalCoverage == _size)
            {
                // Just dump what we're given over what we have and call it a day.
                _list.assign(first, last);

                return;
            }

            // In the worst case scenario, we will need a new run that is the length of
            // The existing run in memory + The new run in memory + 1.
            // This worst case occurs when we inject a new item in the middle of an existing run like so
            // Existing R3->B5->G2, Insertion Y2 starting at 5 (in the middle of the B5)
            // becomes R3->B2->Y2->B1->G2.
            // The original run was 3 long. The insertion run was 1 long. We need 1 more for the
            // fact that an existing piece of the run was split in half (to hold the latter half).
            const size_type cNewRun = gsl::narrow_cast<size_type>(_list.size() + newItemsSize + 1);
            Container newRun;
            newRun.reserve(cNewRun);

            // We will start analyzing from the beginning of our existing run.
            // Use some pointers to keep track of where we are in walking through our runs.

            // Get the existing run that we'll be updating/manipulating.
            const auto existingRun = _list.begin();
            auto pExistingRunPos = existingRun;
            const auto pExistingRunEnd = _list.end();
            auto pInsertRunPos = first;
            size_type cInsertRunRemaining = gsl::narrow_cast<size_type>(newItemsSize);
            size_type iExistingRunCoverage = 0;

            // Copy the existing run into the new buffer up to the "start index" where the new run will be injected.
            // If the new run starts at 0, we have nothing to copy from the beginning.
            if (startIndex != 0)
            {
                // While we're less than the desired insertion position...
                while (iExistingRunCoverage < startIndex)
                {
                    // Add up how much length we can cover by copying an item from the existing run.
                    iExistingRunCoverage += pExistingRunPos->second;

                    // Copy it to the new run buffer and advance both pointers.
                    newRun.push_back(*pExistingRunPos++);
                }

                // When we get to this point, we've copied full segments from the original existing run
                // into our new run buffer. We will have 1 or more full segments of color attributes and
                // we MIGHT have to cut the last copied segment's length back depending on where the inserted
                // attributes will fall in the final/new run.
                // Some examples:
                // - Starting with the original string R3 -> G5 -> B2
                // - 1. If the insertion is Y5 at start index 3
                //      We are trying to get a result/final/new run of R3 -> Y5 -> B2.
                //      We just copied R3 to the new destination buffer and we cang skip down and start inserting the new attrs.
                // - 2. If the insertion is Y3 at start index 5
                //      We are trying to get a result/final/new run of R3 -> G2 -> Y3 -> B2.
                //      We just copied R3 -> G5 to the new destination buffer with the code above.
                //      But the insertion is going to cut out some of the length of the G5.
                //      We need to fix this up below so it says G2 instead to leave room for the Y3 to fit in
                //      the new/final run.

                // Fetch out the length so we can fix it up based on the below conditions.
                size_type length = newRun.back().second;

                // If we've covered more cells already than the start of the attributes to be inserted...
                if (iExistingRunCoverage > startIndex)
                {
                    // ..then subtract some of the length of the final cell we copied.
                    // We want to take remove the difference in distance between the cells we've covered in the new
                    // run and the insertion point.
                    // (This turns G5 into G2 from Example 2 just above)
                    length -= (iExistingRunCoverage - startIndex);
                }

                // Now we're still on that "last cell copied" into the new run.
                // If the color of that existing copied cell matches the color of the first segment
                // of the run we're about to insert, we can just increment the length to extend the coverage.
                if (newRun.back().first == pInsertRunPos->first)
                {
                    length += pInsertRunPos->second;

                    // Since the color matched, we have already "used up" part of the insert run
                    // and can skip it in our big "memcopy" step below that will copy the bulk of the insert run.
                    cInsertRunRemaining--;
                    pInsertRunPos++;
                }

                // We're done manipulating the length. Store it back.
                newRun.back().second = length;
            }

            // Bulk copy the majority (or all, depending on circumstance) of the insert run into the final run buffer.
            std::copy_n(pInsertRunPos, cInsertRunRemaining, std::back_inserter(newRun));

            // We're technically done with the insert run now and have 0 remaining, but won't bother updating its pointers
            // and counts any further because we won't use them.

            const size_type endIndex = startIndex + newItemsTotalCoverage - 1;

            // Now we need to move our pointer for the original existing run forward and update our counts
            // on how many cells we could have copied from the source before finishing off the new run.
            while (iExistingRunCoverage <= endIndex)
            {
                FAIL_FAST_IF(!(pExistingRunPos != pExistingRunEnd));
                iExistingRunCoverage += pExistingRunPos->second;
                pExistingRunPos++;
            }

            // If we still have original existing run cells remaining, copy them into the final new run.
            if (pExistingRunPos != pExistingRunEnd || iExistingRunCoverage != (endIndex + 1))
            {
                // We advanced the existing run pointer and its count to on or past the end of what the insertion run filled in.
                // If this ended up being past the end of what the insertion run covers, we have to account for the cells after
                // the insertion run but before the next piece of the original existing run.
                // The example in this case is if we had...
                // Existing Run = R3 -> G5 -> B2 -> X5
                // Insert Run = Y2 @ startIndex = 7 and endIndex = 8
                // ... then at this point in time, our states would look like...
                // New Run so far = R3 -> G4 -> Y2
                // Existing Run Pointer is at X5
                // Existing run coverage count at 3 + 5 + 2 = 10.
                // However, in order to get the final desired New Run
                //   (which is R3 -> G4 -> Y2 -> B1 -> X5)
                //   we would need to grab a piece of that B2 we already skipped past.
                // iExistingRunCoverage = 10. endIndex = 8. endIndex+1 = 9. 10 > 9. So we skipped something.
                if (iExistingRunCoverage > (endIndex + 1))
                {
                    // Back up the existing run pointer so we can grab the piece we skipped.
                    pExistingRunPos--;

                    // If the color matches what's already in our run, just increment the count value.
                    // This case is slightly off from the example above. This case is for if the B2 above was actually Y2.
                    // That Y2 from the existing run is the same color as the Y2 we just filled a few columns left in the final run
                    // so we can just adjust the final run's column count instead of adding another segment here.
                    if (newRun.back().first == pExistingRunPos->first)
                    {
                        size_type length = newRun.back().second;
                        length += (iExistingRunCoverage - (endIndex + 1));
                        newRun.back().second = length;
                    }
                    else
                    {
                        // If the color didn't match, then we just need to copy the piece we skipped and adjust
                        // its length for the discrepancy in columns not yet covered by the final/new run.

                        // Move forward to a blank spot in the new run
                        newRun.emplace_back();

                        // Copy the existing run's color information to the new run
                        newRun.back().first = pExistingRunPos->first;

                        // Adjust the length of that copied color to cover only the reduced number of columns needed
                        // now that some have been replaced by the insert run.
                        newRun.back().second = iExistingRunCoverage - (endIndex + 1);
                    }

                    // Now that we're done recovering a piece of the existing run we skipped, move the pointer forward again.
                    pExistingRunPos++;
                }

                // OK. In this case, we didn't skip anything. The end of the insert run fell right at a boundary
                // in columns that was in the original existing run.
                // However, the next piece of the original existing run might happen to have the same color attribute
                // as the final piece of what we just copied.
                // As an example...
                // Existing Run = R3 -> G5 -> B2.
                // Insert Run = B5 @ startIndex = 3 and endIndex = 7
                // New Run so far = R3 -> B5
                // New Run desired when done = R3 -> B7
                // Existing run pointer is on B2.
                // We want to merge the 2 from the B2 into the B5 so we get B7.
                else if (newRun.back().first == pExistingRunPos->first)
                {
                    // Add the value from the existing run into the current new run position.
                    size_type length = newRun.back().second;
                    length += pExistingRunPos->second;
                    newRun.back().second = length;

                    // Advance the existing run position since we consumed its value and merged it in.
                    pExistingRunPos++;
                }

                // Now bulk copy any segments left in the original existing run
                if (pExistingRunPos < pExistingRunEnd)
                {
                    std::copy_n(pExistingRunPos, (pExistingRunEnd - pExistingRunPos), std::back_inserter(newRun));
                }
            }

            // OK, phew. We're done. Now we just need to free the existing run and store the new run in its place.
            _list.swap(newRun);
        }

        Container _list;
        S _size{ 0 };

#ifdef UNIT_TESTING
        friend class ::RunLengthEncodingTests;
#endif
    };

    template<typename T, typename S = std::size_t>
    using rle = basic_rle<T, S, std::vector<std::pair<T, S>>>;

    template<typename T, typename S = std::size_t, std::size_t N = 1>
    using small_rle = basic_rle<T, S, boost::container::small_vector<std::pair<T, S>, N>>;
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
