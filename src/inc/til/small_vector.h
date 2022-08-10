// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <cassert>
#include <span>

#pragma warning(push)
// small_vector::_data can reference both non-owned (&_buffer[0]) and owned (new[]) data.
#pragma warning(disable : 26401) // Do not delete a raw pointer that is not an owner<T> (i.11).
// small_vector::_data can reference both non-owned (&_buffer[0]) and owned (new[]) data.
#pragma warning(disable : 26403) // Reset or explicitly delete an owner<T> pointer '...' (r.3).
// small_vector::_data can reference both non-owned (&_buffer[0]) and owned (new[]) data.
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
// That's how the STL implemented their std::vector<>::iterator. I simply copied the concept.
#pragma warning(disable : 26434) // Function '...' hides a non-virtual function '...'.
// Functions like front()/back()/operator[]() are explicitly unchecked, just like the std::vector equivalents.
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
// small_vector::_data references potentially uninitialized data and so we can't pass it regular iterators which reference initialized data.
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe ... (stl.1).
// small_vector::_data references potentially uninitialized data and so we can't pass it regular iterators which reference initialized data.
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).

namespace til
{
    template<typename T>
    struct small_vector_const_iterator
    {
        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

#if _ITERATOR_DEBUG_LEVEL == 0
        constexpr small_vector_const_iterator(pointer ptr) :
            _ptr{ ptr }
        {
        }
#else // _ITERATOR_DEBUG_LEVEL == 0
        constexpr small_vector_const_iterator(pointer ptr, pointer begin, pointer end) :
            _ptr{ ptr },
            _begin{ begin },
            _end{ end }
        {
        }
#endif // _ITERATOR_DEBUG_LEVEL == 0

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin, "cannot dereference value-initialized iterator");
            _STL_VERIFY(_ptr < _end, "cannot dereference end iterator");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return *_ptr;
        }

        [[nodiscard]] constexpr pointer operator->() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin, "cannot dereference value-initialized iterator");
            _STL_VERIFY(_ptr < _end, "cannot dereference end iterator");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr;
        }

        constexpr small_vector_const_iterator& operator++() noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin, "cannot increment value-initialized iterator");
            _STL_VERIFY(_ptr < _end, "cannot increment iterator past end");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            ++_ptr;
            return *this;
        }

        constexpr small_vector_const_iterator operator++(int) noexcept
        {
            small_vector_const_iterator _Tmp{ *this };
            ++*this;
            return _Tmp;
        }

        constexpr small_vector_const_iterator& operator--() noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin, "cannot decrement value-initialized iterator");
            _STL_VERIFY(_begin < _ptr, "cannot decrement iterator before begin");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            --_ptr;
            return *this;
        }

        constexpr small_vector_const_iterator operator--(int) noexcept
        {
            small_vector_const_iterator _Tmp{ *this };
            --*this;
            return _Tmp;
        }

        constexpr void _Verify_offset([[maybe_unused]] const difference_type _Off) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            if (_Off != 0)
            {
                _STL_VERIFY(_begin, "cannot seek value-initialized iterator");
            }

            if (_Off < 0)
            {
                _STL_VERIFY(_ptr - _begin >= -_Off, "cannot seek iterator before begin");
            }

            if (_Off > 0)
            {
                _STL_VERIFY(_end - _ptr >= _Off, "cannot seek iterator after end");
            }
#endif // _ITERATOR_DEBUG_LEVEL >= 1
        }

        constexpr small_vector_const_iterator& operator+=(const difference_type _Off) noexcept
        {
            _Verify_offset(_Off);
            _ptr += _Off;
            return *this;
        }

        [[nodiscard]] constexpr small_vector_const_iterator operator+(const difference_type _Off) const noexcept
        {
            small_vector_const_iterator _Tmp{ *this };
            _Tmp += _Off;
            return _Tmp;
        }

        constexpr small_vector_const_iterator& operator-=(const difference_type _Off) noexcept
        {
            _Verify_offset(-_Off);
            _ptr -= _Off;
            return *this;
        }

        [[nodiscard]] constexpr small_vector_const_iterator operator-(const difference_type _Off) const noexcept
        {
            small_vector_const_iterator _Tmp{ *this };
            _Tmp -= _Off;
            return _Tmp;
        }

        [[nodiscard]] constexpr difference_type operator-(const small_vector_const_iterator& _Right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin == _Right._begin && _end == _Right._end, "cannot subtract incompatible iterators");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr - _Right._ptr;
        }

        [[nodiscard]] constexpr reference operator[](const difference_type _Off) const noexcept
        {
            return *(*this + _Off);
        }

        [[nodiscard]] constexpr bool operator==(const small_vector_const_iterator& _Right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin == _Right._begin && _end == _Right._end, "cannot compare incompatible iterators for equality");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr == _Right._ptr;
        }

        [[nodiscard]] constexpr std::strong_ordering operator<=>(const small_vector_const_iterator& _Right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(
                _begin == _Right._begin && _end == _Right._end, "cannot compare incompatible iterators");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr <=> _Right._ptr;
        }

        using _Prevent_inheriting_unwrap = small_vector_const_iterator;

        [[nodiscard]] constexpr pointer _Unwrapped() const noexcept
        {
            return _ptr;
        }

        static constexpr bool _Unwrap_when_unverified = _ITERATOR_DEBUG_LEVEL == 0;

        constexpr void _Seek_to(const pointer _It) noexcept
        {
            _ptr = _It;
        }

        pointer _ptr = nullptr;
#if _ITERATOR_DEBUG_LEVEL >= 1
        pointer _begin = nullptr;
        pointer _end = nullptr;
#endif // _ITERATOR_DEBUG_LEVEL >= 1
    };

    template<class T>
    class small_vector_iterator : public small_vector_const_iterator<T>
    {
    public:
        using base = small_vector_const_iterator<T>;

        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        using base::base;

        [[nodiscard]] constexpr reference operator*() const noexcept
        {
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
            return const_cast<reference>(base::operator*());
        }

        [[nodiscard]] constexpr pointer operator->() const noexcept
        {
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
            return const_cast<pointer>(base::operator->());
        }

        constexpr small_vector_iterator& operator++() noexcept
        {
            base::operator++();
            return *this;
        }

        constexpr small_vector_iterator operator++(int) noexcept
        {
            small_vector_iterator _Tmp = *this;
            base::operator++();
            return _Tmp;
        }

        constexpr small_vector_iterator& operator--() noexcept
        {
            base::operator--();
            return *this;
        }

        constexpr small_vector_iterator operator--(int) noexcept
        {
            small_vector_iterator _Tmp = *this;
            base::operator--();
            return _Tmp;
        }

        constexpr small_vector_iterator& operator+=(const difference_type _Off) noexcept
        {
            base::operator+=(_Off);
            return *this;
        }

        [[nodiscard]] constexpr small_vector_iterator operator+(const difference_type _Off) const noexcept
        {
            small_vector_iterator _Tmp = *this;
            _Tmp += _Off;
            return _Tmp;
        }

        constexpr small_vector_iterator& operator-=(const difference_type _Off) noexcept
        {
            base::operator-=(_Off);
            return *this;
        }

        using base::operator-;

        [[nodiscard]] constexpr small_vector_iterator operator-(const difference_type _Off) const noexcept
        {
            small_vector_iterator _Tmp = *this;
            _Tmp -= _Off;
            return _Tmp;
        }

        [[nodiscard]] constexpr reference operator[](const difference_type _Off) const noexcept
        {
            return const_cast<reference>(base::operator[](_Off));
        }

        using _Prevent_inheriting_unwrap = small_vector_iterator;

        [[nodiscard]] constexpr pointer _Unwrapped() const noexcept
        {
            return operator->();
        }
    };

    template<typename T, size_t N>
    class small_vector
    {
    public:
        static_assert(N != 0, "A small_vector without a small buffer isn't very useful");
        static_assert(std::is_nothrow_default_constructible_v<T>, "small_vector::_grow doesn't guard against exceptions");
        static_assert(std::is_nothrow_move_constructible_v<T>, "small_vector::_grow doesn't guard against exceptions");

        using value_type = T;
        using allocator_type = std::allocator<T>;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        using iterator = small_vector_iterator<T>;
        using const_iterator = small_vector_const_iterator<T>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        small_vector() noexcept :
            _data{ &_buffer[0] }
        {
        }

        explicit small_vector(size_type count, const T& value = T{}) :
            small_vector{}
        {
            insert(begin(), count, value);
        }

        template<typename InputIt>
        small_vector(InputIt first, InputIt last) :
            small_vector{}
        {
            insert(begin(), first, last);
        }

        small_vector(std::initializer_list<T> list) :
            small_vector{}
        {
            insert(begin(), list.begin(), list.end());
        }

        small_vector(const small_vector& other) :
            small_vector{}
        {
            operator=(other);
        }

        small_vector& operator=(const small_vector& other)
        {
            std::destroy_n(_data, _size);
            _size = 0;

            if (other._size > _capacity)
            {
                _grow(other._size - _capacity);
            }

            std::uninitialized_copy(other.begin(), other.end(), _data);
            _size = other._size;
            return *this;
        }

        small_vector(small_vector&& other) noexcept :
            small_vector{}
        {
            operator=(std::move(other));
        }

        small_vector& operator=(small_vector&& other) noexcept
        {
            std::destroy_n(_data, _size);
            if (_capacity != N)
            {
                delete[] _data;
            }

            if (other._capacity == N)
            {
                _data = &_buffer[0];
                _capacity = N;
                _size = other._size;
                std::move(other.begin(), other.end(), begin());
            }
            else
            {
                _data = other._data;
                _capacity = other._capacity;
                _size = other._size;
            }

            other._data = &other._buffer[0];
            other._capacity = N;
            other._size = 0;

            return *this;
        }

        ~small_vector()
        {
            std::destroy_n(_data, _size);
            if (_capacity != N)
            {
                delete[] _data;
            }
        }

        constexpr size_type max_size() const noexcept { return static_cast<size_t>(-1) / sizeof(T); }

        constexpr pointer data() noexcept { return _data; }
        constexpr const_pointer data() const noexcept { return _data; }
        constexpr size_type capacity() const noexcept { return _capacity; }
        constexpr size_type size() const noexcept { return _size; }
        constexpr size_type empty() const noexcept { return !_data; }

        constexpr iterator begin() noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data, _data, _data + _size };
#else
            return { _data };
#endif
        }
        constexpr const_iterator begin() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data, _data, _data + _size };
#else
            return { _data };
#endif
        }
        constexpr const_iterator cbegin() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data, _data, _data + _size };
#else
            return { _data };
#endif
        }
        constexpr const_reverse_iterator crbegin() const noexcept
        {
            return const_reverse_iterator{ end() };
        }

        constexpr iterator end() noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data + _size, _data, _data + _size };
#else
            return { _data + _size };
#endif
        }
        constexpr const_iterator end() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data + _size, _data, _data + _size };
#else
            return { _data + _size };
#endif
        }
        constexpr const_iterator cend() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data + _size, _data, _data + _size };
#else
            return { _data + _size };
#endif
        }
        constexpr const_reverse_iterator crend() const noexcept
        {
            return const_reverse_iterator{ begin() };
        }

        bool operator==(const small_vector& other) const noexcept
        {
            return std::equal(begin(), end(), other.begin(), other.end());
        }

        reference front() noexcept
        {
            return operator[](0);
        }

        const_reference front() const noexcept
        {
            return operator[](0);
        }

        reference back() noexcept
        {
            return operator[](_size - 1);
        }

        const_reference back() const noexcept
        {
            return operator[](_size - 1);
        }

        reference operator[](size_type off) noexcept
        {
            assert(off < _size);
            return _data[off];
        }

        const_reference operator[](size_type off) const noexcept
        {
            assert(off < _size);
            return _data[off];
        }

        reference at(size_type off)
        {
            if (off >= _size)
            {
                _throw_invalid_subscript();
            }
            return _data[off];
        }

        const_reference at(size_type off) const
        {
            if (off >= _size)
            {
                _throw_invalid_subscript();
            }
            return _data[off];
        }

        void clear() noexcept
        {
            std::destroy_n(begin(), _size);

            if (_capacity != N)
            {
                delete[] _data;
            }

            _data = &_buffer[0];
            _capacity = N;
            _size = 0;
        }

        void resize(size_type newSize)
        {
            if (newSize < _size)
            {
                std::destroy_n(_data + newSize, _size - newSize);
            }
            else if (newSize > _size)
            {
                if (newSize > _capacity)
                {
                    _grow(newSize - _capacity);
                }

                std::uninitialized_value_construct_n(_data + _size, newSize - _size);
            }

            _size = newSize;
        }

        void resize(size_type newSize, const_reference value)
        {
            if (newSize < _size)
            {
                std::destroy_n(_data + newSize, _size - newSize);
            }
            else if (newSize > _size)
            {
                if (newSize > _capacity)
                {
                    _grow(newSize - _capacity);
                }

                std::uninitialized_fill_n(_data + _size, newSize - _size, value);
            }

            _size = newSize;
        }

        void shrink_to_fit()
        {
            if (_capacity == N || _size == _capacity)
            {
                return;
            }

            auto data = &_buffer[0];
            if (_size > N)
            {
                data = new T[_size];
            }

            std::uninitialized_move_n(begin(), _size, data);
            std::free(_data);

            _data = data;
            _capacity = _size;
        }

        void push_back(const T& value)
        {
            if (_size == _capacity)
            {
                _grow(1);
            }

            new (_data + _size) T(value);
            _size++;
        }

        void push_back(T&& value)
        {
            if (_size == _capacity)
            {
                _grow(1);
            }

            new (_data + _size) T(std::move(value));
            _size++;
        }

        template<typename... Args>
        reference emplace_back(Args&&... args)
        {
            if (_size == _capacity)
            {
                _grow(1);
            }

            const auto it = new (_data + _size) T(std::forward<Args>(args)...);
            _size++;
            return *it;
        }

        iterator insert(const_iterator pos, size_type count, const T& value)
        {
            assert(pos >= begin() && pos <= end());

            const auto off = pos - begin();
            const auto newSize = _size + count;

            if (newSize > _capacity)
            {
                _grow(count);
            }

            const auto it = std::uninitialized_fill_n(_data + off, count, value);
            _size = newSize;

#if _ITERATOR_DEBUG_LEVEL >= 1
            return { it, _data, _data + _size };
#else
            return { it };
#endif
        }

        template<typename InputIt, std::enable_if_t<std::_Is_iterator_v<InputIt>, int> = 0>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            assert(pos >= begin() && pos <= end());

            const auto off = pos - begin();
            const auto count = std::distance(first, last);
            const auto newSize = _size + count;

            if (newSize > _capacity)
            {
                _grow(count);
            }

            const auto it = std::uninitialized_copy_n(first, count, _data + off);
            _size = newSize;

#if _ITERATOR_DEBUG_LEVEL >= 1
            return { it, _data, _data + _size };
#else
            return { it };
#endif
        }

        void erase(const_iterator pos)
        {
            assert(pos >= begin() && pos < end());
            std::destroy_at(begin() + pos);
        }

        void erase(const_iterator first, const_iterator last)
        {
            if (first >= last)
            {
                return;
            }

            const auto beg = begin();
            const auto it = beg + (first - cbegin());
            const auto oldEnd = end();
            const auto newEnd = std::move<const_iterator, iterator>(last, oldEnd, it);
            std::destroy(newEnd, oldEnd);
            _size = newEnd - beg;
        }

    private:
        [[noreturn]] static void _throw_invalid_subscript()
        {
            throw std::out_of_range("invalid small_vector subscript");
        }

        [[noreturn]] static void _throw_alloc()
        {
            throw std::bad_alloc();
        }

        [[noreturn]] static void _throw_too_long()
        {
            throw std::length_error("small_vector too long");
        }

        void _grow(size_type add)
        {
            const auto cap = _capacity;
            const auto newCap = cap + std::max(add, cap / 2);

            if (newCap < cap || newCap > max_size())
            {
                _throw_too_long();
            }

            const auto data = new T[newCap];
            // We've asserted for std::is_nothrow_move_constructible_v<T> and thus won't leak `data`.
            std::uninitialized_move_n(_data, _size, data);

            if (_capacity != N)
            {
                delete[] _data;
            }

            _data = data;
            _capacity = newCap;
        }

        T* _data;
        size_t _capacity = N;
        size_t _size = 0;
        T _buffer[N];
    };
}

#pragma warning(pop)
