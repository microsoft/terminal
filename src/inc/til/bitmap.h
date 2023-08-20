// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "rect.h"

#ifdef UNIT_TESTING
class BitmapTests;
#endif

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        template<typename Allocator>
        class _bitmap_const_iterator
        {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = const til::rect;
            using difference_type = ptrdiff_t;
            using pointer = const til::rect*;
            using reference = const til::rect&;

            _bitmap_const_iterator(const dynamic_bitset<unsigned long long, Allocator>& values, til::rect rc, ptrdiff_t pos) :
                _values(values),
                _rc(rc),
                _pos(pos),
                _end(rc.size().area())
            {
                _calculateArea();
            }

            _bitmap_const_iterator& operator++()
            {
                _pos = _nextPos;
                _calculateArea();
                return (*this);
            }

            _bitmap_const_iterator operator++(int)
            {
                const auto prev = *this;
                ++*this;
                return prev;
            }

            constexpr bool operator==(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos == other._pos && _values == other._values;
            }

            constexpr bool operator!=(const _bitmap_const_iterator& other) const noexcept
            {
                return !(*this == other);
            }

            constexpr bool operator<(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos < other._pos;
            }

            constexpr bool operator>(const _bitmap_const_iterator& other) const noexcept
            {
                return _pos > other._pos;
            }

            constexpr reference operator*() const noexcept
            {
                return _run;
            }

            constexpr pointer operator->() const noexcept
            {
                return &_run;
            }

        private:
            const dynamic_bitset<unsigned long long, Allocator>& _values;
            const til::rect _rc;
            size_t _pos;
            size_t _nextPos;
            const size_t _end;
            til::rect _run;

            // Update _run to contain the next rectangle of consecutively set bits within this bitmap.
            // _calculateArea may be called repeatedly to yield all those rectangles.
            void _calculateArea()
            {
                // The following logic first finds the next set bit in this bitmap and the next unset bit past that.
                // The area in between those positions are thus all set bits and will end up being the next _run.

                // dynamic_bitset allows you to quickly find the next set bit using find_next(prev),
                // where "prev" is the position _past_ which should be searched (i.e. excluding position "prev").
                // If _pos is still 0, we thus need to use the counterpart find_first().
                _nextPos = _pos == 0 ? _values.find_first() : _values.find_next(_pos - 1);

                // If we haven't reached the end yet...
                if (_nextPos < _end)
                {
                    // pos is now at the first on bit.
                    // If no next set bit can be found, npos is returned, which is SIZE_T_MAX.
                    // saturated_cast can ensure that this will be converted to CoordType's max (which is greater than _end).
                    const auto runStart = _rc.point_at(base::saturated_cast<CoordType>(_nextPos));

                    // We'll only count up until the end of this row.
                    // a run can be a max of one row tall.
                    const size_t rowEndIndex = _rc.index_of<size_t>(til::point(_rc.right - 1, runStart.y)) + 1;

                    // Find the length for the rectangle.
                    size_t runLength = 0;

                    // We have at least 1 so start with a do/while.
                    do
                    {
                        ++_nextPos;
                        ++runLength;
                    } while (_nextPos < rowEndIndex && _values[_nextPos]);
                    // Keep going until we reach end of row, end of the buffer, or the next bit is off.

                    // Assemble and store that run.
                    _run = til::rect{ runStart, til::size{ base::saturated_cast<CoordType>(runLength), 1 } };
                }
                else
                {
                    // If we reached the end _nextPos may be >= _end (potentially even PTRDIFF_T_MAX).
                    // ---> Mark the end of the iterator by updating the state with _end.
                    _pos = _end;
                    _nextPos = _end;
                    _run = til::rect{};
                }
            }
        };

        template<typename Allocator = std::allocator<unsigned long long>>
        class bitmap
        {
        public:
            using allocator_type = Allocator;
            using const_iterator = details::_bitmap_const_iterator<allocator_type>;

        private:
            using run_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<til::rect>;

        public:
            explicit bitmap(const allocator_type& allocator) noexcept :
                _alloc{ allocator },
                _sz{},
                _rc{},
                _bits{ _alloc },
                _runs{ _alloc }
            {
            }

            bitmap() noexcept :
                bitmap(allocator_type{})
            {
            }

            bitmap(til::size sz) :
                bitmap(sz, false, allocator_type{})
            {
            }

            bitmap(til::size sz, const allocator_type& allocator) :
                bitmap(sz, false, allocator)
            {
            }

            bitmap(til::size sz, bool fill, const allocator_type& allocator) :
                _alloc{ allocator },
                _sz(sz),
                _rc(sz),
                _bits(_sz.area(), fill ? std::numeric_limits<unsigned long long>::max() : 0, _alloc),
                _runs{ _alloc }
            {
            }

            bitmap(til::size sz, bool fill) :
                bitmap(sz, fill, allocator_type{})
            {
            }

            bitmap(const bitmap& other) :
                _alloc{ std::allocator_traits<allocator_type>::select_on_container_copy_construction(other._alloc) },
                _sz{ other._sz },
                _rc{ other._rc },
                _bits{ other._bits },
                _runs{ other._runs }
            {
                // copy constructor is required to call select_on_container_copy
            }

            bitmap& operator=(const bitmap& other)
            {
                if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value)
                {
                    _alloc = other._alloc;
                }
                _sz = other._sz;
                _rc = other._rc;
                _bits = other._bits;
                _runs = other._runs;
                return *this;
            }

            bitmap(bitmap&& other) noexcept :
                _alloc{ std::move(other._alloc) },
                _sz{ std::move(other._sz) },
                _rc{ std::move(other._rc) },
                _bits{ std::move(other._bits) },
                _runs{ std::move(other._runs) }
            {
            }

            bitmap& operator=(bitmap&& other) noexcept
            {
                if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value)
                {
                    _alloc = std::move(other._alloc);
                }
                _bits = std::move(other._bits);
                _runs = std::move(other._runs);
                _sz = std::move(other._sz);
                _rc = std::move(other._rc);
                return *this;
            }

            ~bitmap() {}

            void swap(bitmap& other)
            {
                if constexpr (std::allocator_traits<allocator_type>::propagate_on_container_swap::value)
                {
                    std::swap(_alloc, other._alloc);
                }
                std::swap(_bits, other._bits);
                std::swap(_runs, other._runs);
                std::swap(_sz, other._sz);
                std::swap(_rc, other._rc);
            }

            constexpr bool operator==(const bitmap& other) const noexcept
            {
                return _sz == other._sz &&
                       _rc == other._rc &&
                       _bits == other._bits;
                // _runs excluded because it's a cache of generated state.
            }

            constexpr bool operator!=(const bitmap& other) const noexcept
            {
                return !(*this == other);
            }

            const_iterator begin() const
            {
                return const_iterator(_bits, til::rect{ _sz }, 0);
            }

            const_iterator end() const
            {
                return const_iterator(_bits, til::rect{ _sz }, _sz.area());
            }

            const std::span<const til::rect> runs() const
            {
                // If we don't have cached runs, rebuild.
                if (!_runs.has_value())
                {
                    _runs.emplace(begin(), end());
                }

                // Return the runs.
                return _runs.value();
            }

            // optional fill the uncovered area with bits.
            void translate(const til::point delta, bool fill = false)
            {
                if (delta.x == 0)
                {
                    // fast path by using bit shifting
                    translate_y(delta.y, fill);
                    return;
                }

                // FUTURE: PERF: GH #4015: This could use in-place walk semantics instead of a temporary.
                bitmap<allocator_type> other{ _sz, _alloc };

                for (auto run : *this)
                {
                    // Offset by the delta
                    run += delta;

                    // Intersect with the bounds of our bitmap area
                    // as part of it could have slid out of bounds.
                    run &= _rc;

                    // Set it into the new bitmap.
                    other.set(run);
                }

                // If we were asked to fill... find the uncovered region.
                if (fill)
                {
                    // Original Rect of As.
                    //
                    // X <-- origin
                    // A A A A
                    // A A A A
                    // A A A A
                    // A A A A
                    const auto originalRect = _rc;

                    // If Delta = (2, 2)
                    // Translated Rect of Bs.
                    //
                    // X <-- origin
                    //
                    //
                    //     B B B B
                    //     B B B B
                    //     B B B B
                    //     B B B B
                    const auto translatedRect = _rc + delta;

                    // Subtract the B from the A one to see what wasn't filled by the move.
                    // C is the overlap of A and B:
                    //
                    // X <-- origin
                    // A A A A                     1 1 1 1
                    // A A A A                     1 1 1 1
                    // A A C C B B     subtract    2 2
                    // A A C C B B    --------->   2 2
                    //     B B B B      A - B
                    //     B B B B
                    //
                    // 1 and 2 are the spaces to fill that are "uncovered".
                    const auto fillRects = originalRect - translatedRect;
                    for (const auto& f : fillRects)
                    {
                        other.set(f);
                    }
                }

                // Swap us with the temporary one.
                std::swap(other, *this);
            }

            void set(const til::point pt)
            {
                if (_rc.contains(pt))
                {
                    _runs.reset(); // reset cached runs on any non-const method
                    _bits.set(_rc.index_of(pt));
                }
            }

            void set(til::rect rc)
            {
                _runs.reset(); // reset cached runs on any non-const method

                rc &= _rc;

                const auto width = rc.width();
                const auto stride = _rc.width();
                auto idx = _rc.index_of({ rc.left, rc.top });

                for (auto row = rc.top; row < rc.bottom; ++row, idx += stride)
                {
                    _bits.set(idx, width, true);
                }
            }

            void set_all() noexcept
            {
                _runs.reset(); // reset cached runs on any non-const method
                _bits.set();
            }

            void reset_all() noexcept
            {
                _runs.reset(); // reset cached runs on any non-const method
                _bits.reset();
            }

            // True if we resized. False if it was the same size as before.
            // Set fill if you want the new region (on growing) to be marked dirty.
            bool resize(til::size size, bool fill = false)
            {
                _runs.reset(); // reset cached runs on any non-const method

                // Don't resize if it's not different
                if (_sz != size)
                {
                    // Make a new bitmap for the other side, empty initially.
                    bitmap<allocator_type> newMap{ size, false, _alloc };

                    // Copy any regions that overlap from this map to the new one.
                    // Just iterate our runs...
                    for (const auto& run : *this)
                    {
                        // intersect them with the new map
                        // so we don't attempt to set bits that fit outside
                        // the new one.
                        const auto intersect = run & newMap._rc;

                        // and if there is still anything left, set them.
                        if (!intersect.empty())
                        {
                            newMap.set(intersect);
                        }
                    }

                    // Then, if we were requested to fill the new space on growing,
                    // find the space in the new rectangle that wasn't in the old
                    // and fill it up.
                    if (fill)
                    {
                        // A subtraction will yield anything in the new that isn't
                        // a part of the old.
                        const auto newAreas = newMap._rc - _rc;
                        for (const auto& area : newAreas)
                        {
                            newMap.set(area);
                        }
                    }

                    // Swap and return.
                    std::swap(newMap, *this);

                    return true;
                }
                else
                {
                    return false;
                }
            }

            constexpr bool one() const noexcept
            {
                return _bits.count() == 1;
            }

            constexpr bool any() const noexcept
            {
                return !none();
            }

            constexpr bool none() const noexcept
            {
                return _bits.none();
            }

            constexpr bool all() const noexcept
            {
                return _bits.all();
            }

            constexpr til::size size() const noexcept
            {
                return _sz;
            }

            std::wstring to_string() const
            {
                std::wstringstream wss;
                wss << std::endl
                    << L"Bitmap of size " << _sz.to_string() << " contains the following dirty regions:" << std::endl;
                wss << L"Runs:" << std::endl;

                for (auto& item : *this)
                {
                    wss << L"\t- " << item.to_string() << std::endl;
                }

                return wss.str();
            }

        private:
            void translate_y(ptrdiff_t delta_y, bool fill)
            {
                if (delta_y == 0)
                {
                    return;
                }

                const auto bitShift = delta_y * _sz.width;

#pragma warning(push)
                // we can't depend on GSL here, so we use static_cast for explicit narrowing
#pragma warning(disable : 26472)
                const auto newBits = static_cast<size_t>(std::abs(bitShift));
#pragma warning(pop)
                const bool isLeftShift = bitShift > 0;

                if (newBits >= _bits.size())
                {
                    if (fill)
                    {
                        set_all();
                    }
                    else
                    {
                        reset_all();
                    }
                    return;
                }

                if (isLeftShift)
                {
                    // This operator doesn't modify the size of `_bits`: the
                    // new bits are set to 0.
                    _bits <<= newBits;
                }
                else
                {
                    _bits >>= newBits;
                }

                if (fill)
                {
                    if (isLeftShift)
                    {
                        _bits.set(0, newBits, true);
                    }
                    else
                    {
                        _bits.set(_bits.size() - newBits, newBits, true);
                    }
                }

                _runs.reset(); // reset cached runs on any non-const method
            }

            allocator_type _alloc;
            til::size _sz;
            til::rect _rc;
            dynamic_bitset<unsigned long long, allocator_type> _bits;

            mutable std::optional<std::vector<til::rect, run_allocator_type>> _runs;

#ifdef UNIT_TESTING
            friend class ::BitmapTests;
#endif
        };

    }

    using bitmap = ::til::details::bitmap<>;

    namespace pmr
    {
        using bitmap = ::til::details::bitmap<std::pmr::polymorphic_allocator<unsigned long long>>;
    }
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<typename T>
    class VerifyOutputTraits<::til::details::bitmap<T>>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::details::bitmap<T>& rect)
        {
            return WEX::Common::NoThrowString(rect.to_string().c_str());
        }
    };

    template<typename T>
    class VerifyCompareTraits<::til::details::bitmap<T>, ::til::details::bitmap<T>>
    {
    public:
        static bool AreEqual(const ::til::details::bitmap<T>& expected, const ::til::details::bitmap<T>& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::details::bitmap<T>& expected, const ::til::details::bitmap<T>& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::details::bitmap<T>& expectedLess, const ::til::details::bitmap<T>& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::details::bitmap<T>& expectedGreater, const ::til::details::bitmap<T>& expectedLess) = delete;

        static bool IsNull(const ::til::details::bitmap<T>& object) noexcept
        {
            return object == til::details::bitmap<T>{};
        }
    };

};
#endif
