// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // The at function declares that you've already sufficiently checked that your array access
    // is in range before retrieving an item inside it at an offset.
    // This is to save double/triple/quadruple testing in circumstances where you are already
    // pivoting on the length of a set and now want to pull elements out of it by offset
    // without checking again.
    // gsl::at will do the check again. As will .at(). And using [] will have a warning in audit.
    template<class T>
    constexpr auto at(T& cont, const size_t i) -> decltype(cont[cont.size()])
    {
#pragma warning(suppress : 26482) // Suppress bounds.2 check for indexing with constant expressions
#pragma warning(suppress : 26446) // Suppress bounds.4 check for subscript operator.
        return cont[i];
    }

#pragma region some

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

        using iterator = _Array_iterator<_Ty, _Used>;
        using const_iterator = _Array_const_iterator<_Ty, _Used>;

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

#pragma endregion

}

// These sit outside the namespace because they sit outside for WIL too.

// Inspired from RETURN_IF_WIN32_BOOL_FALSE
// WIL doesn't include a RETURN_BOOL_IF_FALSE, and RETURN_IF_WIN32_BOOL_FALSE
//  will actually return the value of GLE.
#define RETURN_BOOL_IF_FALSE(b)                     \
    do                                              \
    {                                               \
        const bool __boolRet = wil::verify_bool(b); \
        if (!__boolRet)                             \
        {                                           \
            return __boolRet;                       \
        }                                           \
    } while (0, 0)

// Due to a bug (DevDiv 441931), Warning 4297 (function marked noexcept throws exception) is detected even when the throwing code is unreachable, such as the end of scope after a return, in function-level catch.
#define CATCH_LOG_RETURN_FALSE()            \
    catch (...)                             \
    {                                       \
        __pragma(warning(suppress : 4297)); \
        LOG_CAUGHT_EXCEPTION();             \
        return false;                       \
    }
