// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <array>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<class T, size_t N>
    class some
    {
    private:
        std::array<T, N> _array;
        size_t _used;

#ifdef UNIT_TESTING
        friend class SomeTests;
#endif

    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        using iterator = typename decltype(_array)::iterator;
        using const_iterator = typename decltype(_array)::const_iterator;

        using reverse_iterator = typename decltype(_array)::reverse_iterator;
        using const_reverse_iterator = typename decltype(_array)::const_reverse_iterator;

        constexpr some() noexcept :
            _array{},
            _used{ 0 }
        {
        }

        constexpr some(std::initializer_list<T> init)
        {
            if (init.size() > N)
            {
                _invalidArg();
            }

            std::copy(init.begin(), init.end(), _array.begin());
            _used = init.size();
        }

        constexpr bool operator==(const til::some<T, N>& other) const noexcept
        {
            return std::equal(cbegin(), cend(), other.cbegin(), other.cend());
        }

        constexpr bool operator!=(const til::some<T, N>& other) const noexcept
        {
            return !(*this == other);
        }

        constexpr void fill(const T& _Value)
        {
            _array.fill(_Value);
            _used = N;
        }

        constexpr void swap(some& _Other) noexcept(std::is_nothrow_swappable_v<T>)
        {
            _array.swap(_Other._array);
            std::swap(_used, _Other._used);
        }

        constexpr const_iterator begin() const noexcept
        {
            return _array.begin();
        }

        constexpr const_iterator end() const noexcept
        {
            return _array.begin() + _used;
        }

        constexpr const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }

        constexpr const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(begin());
        }

        constexpr const_iterator cbegin() const noexcept
        {
            return begin();
        }

        constexpr const_iterator cend() const noexcept
        {
            return end();
        }

        constexpr const_reverse_iterator crbegin() const noexcept
        {
            return rbegin();
        }

        constexpr const_reverse_iterator crend() const noexcept
        {
            return rend();
        }

        constexpr size_type size() const noexcept
        {
            return _used;
        }

        constexpr size_type max_size() const noexcept
        {
            return N;
        }

        constexpr bool empty() const noexcept
        {
            return !_used;
        }

        constexpr void clear() noexcept
        {
            _used = 0;
            _array = {}; // should free members, if necessary.
        }

        constexpr const_reference at(size_type pos) const
        {
            if (_used <= pos)
            {
                _outOfRange();
            }

            return _array[pos];
        }

        constexpr const_reference operator[](size_type pos) const noexcept
        {
            return _array[pos];
        }

        constexpr const_reference front() const noexcept
        {
            return _array[0];
        }

        constexpr const_reference back() const noexcept
        {
            return _array[_used - 1];
        }

        constexpr const T* data() const noexcept
        {
            return _array.data();
        }

        constexpr void push_back(const T& val)
        {
            if (_used >= N)
            {
                _outOfRange();
            }

            til::at(_array, _used) = val;

            ++_used;
        }

        constexpr void push_back(T&& val)
        {
            if (_used >= N)
            {
                _outOfRange();
            }

            til::at(_array, _used) = std::move(val);

            ++_used;
        }

        constexpr void pop_back()
        {
            if (_used <= 0)
            {
                _outOfRange();
            }

            --_used;

            til::at(_array, _used) = 0;
        }

        [[noreturn]] constexpr void _invalidArg() const
        {
            throw std::invalid_argument("invalid argument");
        }

        [[noreturn]] constexpr void _outOfRange() const
        {
            throw std::out_of_range("invalid some<T, N> subscript");
        }

        std::wstring to_string() const
        {
            std::wstringstream wss;
            wss << std::endl
                << L"Some contains " << size() << " of max size " << max_size() << ":" << std::endl;
            wss << L"Elements:" << std::endl;

            for (auto& item : *this)
            {
                wss << L"\t- " << item.to_string() << std::endl;
            }

            return wss.str();
        }
    };
}

#ifdef __WEX_COMMON_H__
namespace WEX::TestExecution
{
    template<class T, size_t N>
    class VerifyOutputTraits<::til::some<T, N>>
    {
    public:
        static WEX::Common::NoThrowString ToString(const ::til::some<T, N>& some)
        {
            return WEX::Common::NoThrowString(some.to_string().c_str());
        }
    };

    template<class T, size_t N>
    class VerifyCompareTraits<::til::some<T, N>, ::til::some<T, N>>
    {
    public:
        static bool AreEqual(const ::til::some<T, N>& expected, const ::til::some<T, N>& actual) noexcept
        {
            return expected == actual;
        }

        static bool AreSame(const ::til::some<T, N>& expected, const ::til::some<T, N>& actual) noexcept
        {
            return &expected == &actual;
        }

        static bool IsLessThan(const ::til::some<T, N>& expectedLess, const ::til::some<T, N>& expectedGreater) = delete;

        static bool IsGreaterThan(const ::til::some<T, N>& expectedGreater, const ::til::some<T, N>& expectedLess) = delete;

        static bool IsNull(const ::til::some<T, N>& object) noexcept
        {
            return object == til::some<T, N>{};
        }
    };

};
#endif
