// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <array>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<class _Ty, size_t _Size>
    class some : std::array<_Ty, _Size>
    {
    public:
        using value_type = _Ty;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = _Ty*;
        using const_pointer = const _Ty*;
        using reference = _Ty&;
        using const_reference = const _Ty&;

        using iterator = _STD _Array_iterator<_Ty, _Size>;
        using const_iterator = _STD _Array_const_iterator<_Ty, _Size>;

        using reverse_iterator = _STD reverse_iterator<iterator>;
        using const_reverse_iterator = _STD reverse_iterator<const_iterator>;

        void fill(const _Ty& _Value)
        {
            array::fill(_Value);
        }

        void swap(some& _Other) noexcept(_Is_nothrow_swappable<_Ty>::value)
        {
            array::swap(_Other);
            std::swap(_Used, _Other._Used);
        }

        _NODISCARD _CONSTEXPR17 iterator begin() noexcept
        {
            return iterator(_Elems, 0);
        }

        _NODISCARD _CONSTEXPR17 const_iterator begin() const noexcept
        {
            return const_iterator(_Elems, 0);
        }

        _NODISCARD _CONSTEXPR17 iterator end() noexcept
        {
            return iterator(_Elems, _Used);
        }

        _NODISCARD _CONSTEXPR17 const_iterator end() const noexcept
        {
            return const_iterator(_Elems, _Used);
        }

        _NODISCARD _CONSTEXPR17 reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(end());
        }

        _NODISCARD _CONSTEXPR17 const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }

        _NODISCARD _CONSTEXPR17 reverse_iterator rend() noexcept
        {
            return reverse_iterator(begin());
        }

        _NODISCARD _CONSTEXPR17 const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(begin());
        }

        _NODISCARD _CONSTEXPR17 const_iterator cbegin() const noexcept
        {
            return begin();
        }

        _NODISCARD _CONSTEXPR17 const_iterator cend() const noexcept
        {
            return end();
        }

        _NODISCARD _CONSTEXPR17 const_reverse_iterator crbegin() const noexcept
        {
            return rbegin();
        }

        _NODISCARD _CONSTEXPR17 const_reverse_iterator crend() const noexcept
        {
            return rend();
        }

        _NODISCARD constexpr size_type size() const noexcept
        {
            return _Used;
        }

        _NODISCARD constexpr size_type max_size() const noexcept
        {
            return _Size;
        }

        _NODISCARD constexpr bool empty() const noexcept
        {
            return !_Used;
        }

        _NODISCARD _CONSTEXPR17 reference at(size_type _Pos)
        {
            if (_Used <= _Pos)
            {
                _Xran();
            }

            return _Elems[_Pos];
        }

        _NODISCARD constexpr const_reference at(size_type _Pos) const
        {
            if (_Used <= _Pos)
            {
                _Xran();
            }

            return _Elems[_Pos];
        }

        _NODISCARD _CONSTEXPR17 reference operator[](_In_range_(0, _Used - 1) size_type _Pos) noexcept /* strengthened */
        {
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(_Pos < _Used, "some subscript out of range");
#endif // _CONTAINER_DEBUG_LEVEL > 0

            return _Elems[_Pos];
        }

        _NODISCARD constexpr const_reference operator[](_In_range_(0, _Used - 1) size_type _Pos) const
            noexcept /* strengthened */
        {
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(_Pos < _Used, "some subscript out of range");
#endif // _CONTAINER_DEBUG_LEVEL > 0

            return _Elems[_Pos];
        }

        _NODISCARD _CONSTEXPR17 reference front() noexcept /* strengthened */
        {
            return _Elems[0];
        }

        _NODISCARD constexpr const_reference front() const noexcept /* strengthened */
        {
            return _Elems[0];
        }

        _NODISCARD _CONSTEXPR17 reference back() noexcept /* strengthened */
        {
            return _Elems[_Used - 1];
        }

        _NODISCARD constexpr const_reference back() const noexcept /* strengthened */
        {
            return _Elems[_Used - 1];
        }

        _NODISCARD _CONSTEXPR17 _Ty* data() noexcept
        {
            return _Elems;
        }

        _NODISCARD _CONSTEXPR17 const _Ty* data() const noexcept
        {
            return _Elems;
        }

        void push_back(const _Ty& _Val)
        {
            if (_Used >= _Size)
            {
                _Xran();
            }

            _Elems[_Used] = _Val;

            ++_Used;
        }

        void pop_back() noexcept
        {
            if (_Used <= 0)
            {
                _Xran();
            }

            --_Used;

            _Elems[_Used] = 0;
        }

        [[noreturn]] void _Xran() const
        {
            _Xout_of_range("invalid some<T, N> subscript");
        }

        size_t _Used;
    };
}
