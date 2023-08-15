// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#pragma warning(push)
// small_vector::_data can reference both non-owned (&_buffer[0]) and owned (new[]) data.
#pragma warning(disable : 26402) // Return a scoped object instead of a heap-allocated if it has a move constructor (r.3).
// small_vector manages a _capacity of potentially uninitialized data. We can't use regular new/delete.
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
// That's how the STL implemented their std::vector<>::iterator. I simply copied the concept.
#pragma warning(disable : 26434) // Function '...' hides a non-virtual function '...'.
// Functions like front()/back()/operator[]() are explicitly unchecked, just like the std::vector equivalents.
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
// small_vector::_data references potentially uninitialized data and so we can't pass it regular iterators which reference initialized data.
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe ... (stl.1).
// small_vector::_data references potentially uninitialized data and so we can't pass it regular iterators which reference initialized data.
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
// small_vector::_buffer is explicitly uninitialized, because we manage its initialization manually.
#pragma warning(disable : 26495) // Variable '...' is uninitialized. Always initialize a member variable (type.6).

namespace til
{
    // This class was adopted from std::span<>::iterator.
    template<typename T>
    struct small_vector_const_iterator
    {
        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        small_vector_const_iterator() = default;

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
            small_vector_const_iterator tmp{ *this };
            ++*this;
            return tmp;
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
            small_vector_const_iterator tmp{ *this };
            --*this;
            return tmp;
        }

        constexpr void _Verify_offset([[maybe_unused]] const difference_type off) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            if (off != 0)
            {
                _STL_VERIFY(_begin, "cannot seek value-initialized iterator");
            }

            if (off < 0)
            {
                _STL_VERIFY(_ptr - _begin >= -off, "cannot seek iterator before begin");
            }

            if (off > 0)
            {
                _STL_VERIFY(_end - _ptr >= off, "cannot seek iterator after end");
            }
#endif // _ITERATOR_DEBUG_LEVEL >= 1
        }

        constexpr small_vector_const_iterator& operator+=(const difference_type off) noexcept
        {
            _Verify_offset(off);
            _ptr += off;
            return *this;
        }

        [[nodiscard]] constexpr small_vector_const_iterator operator+(const difference_type off) const noexcept
        {
            small_vector_const_iterator tmp{ *this };
            tmp += off;
            return tmp;
        }

        [[nodiscard]] friend constexpr small_vector_const_iterator operator+(const difference_type off, small_vector_const_iterator next) noexcept
        {
            next += off;
            return next;
        }

        constexpr small_vector_const_iterator& operator-=(const difference_type off) noexcept
        {
            _Verify_offset(-off);
            _ptr -= off;
            return *this;
        }

        [[nodiscard]] constexpr small_vector_const_iterator operator-(const difference_type off) const noexcept
        {
            small_vector_const_iterator tmp{ *this };
            tmp -= off;
            return tmp;
        }

        [[nodiscard]] constexpr difference_type operator-(const small_vector_const_iterator& right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin == right._begin && _end == right._end, "cannot subtract incompatible iterators");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr - right._ptr;
        }

        [[nodiscard]] constexpr reference operator[](const difference_type off) const noexcept
        {
            return *(*this + off);
        }

        [[nodiscard]] constexpr bool operator==(const small_vector_const_iterator& right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin == right._begin && _end == right._end, "cannot compare incompatible iterators for equality");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr == right._ptr;
        }

        [[nodiscard]] constexpr std::strong_ordering operator<=>(const small_vector_const_iterator& right) const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            _STL_VERIFY(_begin == right._begin && _end == right._end, "cannot compare incompatible iterators");
#endif // _ITERATOR_DEBUG_LEVEL >= 1
            return _ptr <=> right._ptr;
        }

#if _ITERATOR_DEBUG_LEVEL >= 1
        friend constexpr void _Verify_range(const small_vector_const_iterator& first, const small_vector_const_iterator& last) noexcept
        {
            _STL_VERIFY(first._begin == last._begin && first._end == last._end, "iterators from different views do not form a range");
            _STL_VERIFY(first._ptr <= last._ptr, "iterator range transposed");
        }
#endif // _ITERATOR_DEBUG_LEVEL >= 1

        using _Prevent_inheriting_unwrap = small_vector_const_iterator;

        [[nodiscard]] constexpr pointer _Unwrapped() const noexcept
        {
            return _ptr;
        }

        static constexpr bool _Unwrap_when_unverified = _ITERATOR_DEBUG_LEVEL == 0;

        constexpr void _Seek_to(const pointer it) noexcept
        {
            _ptr = it;
        }

        pointer _ptr = nullptr;
#if _ITERATOR_DEBUG_LEVEL >= 1
        pointer _begin = nullptr;
        pointer _end = nullptr;
#endif // _ITERATOR_DEBUG_LEVEL >= 1
    };

    // This class was adopted from std::array<>::iterator.
    template<typename T>
    struct small_vector_iterator : small_vector_const_iterator<T>
    {
        using base = small_vector_const_iterator<T>;

        using iterator_concept = std::contiguous_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
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
            small_vector_iterator tmp = *this;
            base::operator++();
            return tmp;
        }

        constexpr small_vector_iterator& operator--() noexcept
        {
            base::operator--();
            return *this;
        }

        constexpr small_vector_iterator operator--(int) noexcept
        {
            small_vector_iterator tmp = *this;
            base::operator--();
            return tmp;
        }

        constexpr small_vector_iterator& operator+=(const difference_type off) noexcept
        {
            base::operator+=(off);
            return *this;
        }

        [[nodiscard]] constexpr small_vector_iterator operator+(const difference_type off) const noexcept
        {
            small_vector_iterator tmp = *this;
            tmp += off;
            return tmp;
        }

        [[nodiscard]] friend constexpr small_vector_iterator operator+(const difference_type off, small_vector_iterator next) noexcept
        {
            next += off;
            return next;
        }

        constexpr small_vector_iterator& operator-=(const difference_type off) noexcept
        {
            base::operator-=(off);
            return *this;
        }

        using base::operator-;

        [[nodiscard]] constexpr small_vector_iterator operator-(const difference_type off) const noexcept
        {
            small_vector_iterator tmp = *this;
            tmp -= off;
            return tmp;
        }

        [[nodiscard]] constexpr reference operator[](const difference_type off) const noexcept
        {
            return const_cast<reference>(base::operator[](off));
        }

        using _Prevent_inheriting_unwrap = small_vector_iterator;

        [[nodiscard]] constexpr pointer _Unwrapped() const noexcept
        {
#pragma warning(suppress : 26492) // Don't use const_cast to cast away const or volatile (type.3).
            return const_cast<pointer>(base::_Unwrapped());
        }
    };

    template<typename T, size_t N>
    class small_vector
    {
    public:
        static_assert(N != 0, "A small_vector without a small buffer isn't very useful");
        static_assert(std::is_nothrow_move_assignable_v<T>, "_generic_insert doesn't guard against exceptions");
        static_assert(std::is_nothrow_move_constructible_v<T>, "_grow/_generic_insert don't guard against exceptions");

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
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        small_vector() noexcept :
            _data{ &_buffer[0] },
            _capacity{ N },
            _size{ 0 }
        {
        }

        explicit small_vector(size_type count, const T& value = T{}) :
            small_vector{}
        {
            insert(end(), count, value);
        }

        template<std::input_iterator InputIt>
        small_vector(InputIt first, InputIt last) :
            small_vector{}
        {
            insert(end(), first, last);
        }

        small_vector(std::initializer_list<T> list) :
            small_vector{}
        {
            insert(end(), list.begin(), list.end());
        }

        // NOTE: If an exception is thrown while copying, the vector is left empty.
        small_vector(const small_vector& other) :
            small_vector{}
        {
            _copy_assign(other);
        }

        // NOTE: If an exception is thrown while copying, the vector is left empty.
        small_vector& operator=(const small_vector& other)
        {
            if (this != &other)
            {
                clear();
                _copy_assign(other);
            }

            return *this;
        }

        small_vector(small_vector&& other) noexcept
        {
            _move_assign(other);
        }

        small_vector& operator=(small_vector&& other) noexcept
        {
            if (this != &other)
            {
                std::destroy(begin(), end());
                if (_capacity != N)
                {
                    _deallocate(_data);
                }

                _move_assign(other);
            }

            return *this;
        }

        ~small_vector()
        {
            std::destroy(begin(), end());
            if (_capacity != N)
            {
                _deallocate(_data);
            }
        }

        constexpr size_type max_size() const noexcept { return static_cast<size_t>(-1) / sizeof(T); }

        constexpr pointer data() noexcept { return _data; }
        constexpr const_pointer data() const noexcept { return _data; }
        constexpr size_type capacity() const noexcept { return _capacity; }
        constexpr size_type size() const noexcept { return _size; }
        constexpr size_type empty() const noexcept { return _size == 0; }

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

        constexpr reverse_iterator rbegin() noexcept
        {
            return std::make_reverse_iterator(end());
        }

        constexpr const_reverse_iterator rbegin() const noexcept
        {
            return std::make_reverse_iterator(end());
        }

        constexpr const_reverse_iterator crbegin() const noexcept
        {
            return std::make_reverse_iterator(end());
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

        constexpr reverse_iterator rend() noexcept
        {
            return std::make_reverse_iterator(begin());
        }

        constexpr const_reverse_iterator rend() const noexcept
        {
            return std::make_reverse_iterator(begin());
        }

        constexpr const_reverse_iterator crend() const noexcept
        {
            return std::make_reverse_iterator(begin());
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
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(off < _size, "subscript out of range");
#endif // _CONTAINER_DEBUG_LEVEL > 0
            return _data[off];
        }

        const_reference operator[](size_type off) const noexcept
        {
#if _CONTAINER_DEBUG_LEVEL > 0
            _STL_VERIFY(off < _size, "subscript out of range");
#endif // _CONTAINER_DEBUG_LEVEL > 0
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
            std::destroy(begin(), end());
            _size = 0;
        }

        void reserve(size_type capacity)
        {
            if (capacity > _capacity)
            {
                _grow(capacity);
            }
        }

        void resize(size_type new_size)
        {
            _generic_resize(new_size, [](auto&& beg, auto&& end) {
                std::uninitialized_value_construct(beg, end);
            });
        }

        void resize(size_type new_size, const_reference value)
        {
            _generic_resize(new_size, [&](auto&& beg, auto&& end) {
                std::uninitialized_fill(beg, end, value);
            });
        }

        void shrink_to_fit()
        {
            if (_capacity == N || _size == _capacity)
            {
                return;
            }

            auto data = &_buffer[0];
            auto capacity = N;
            if (_size > N)
            {
                data = _allocate(_size);
                capacity = _size;
            }

            std::uninitialized_move(begin(), end(), data);
            std::destroy(begin(), end());
            _deallocate(_data);

            _data = data;
            _capacity = capacity;
        }

        // This is a very unsafe shortcut to free the buffer and get a direct
        // hold to the _buffer. The caller can then fill it with `size` items.
        [[nodiscard]] T* unsafe_shrink_to_size(size_t size) noexcept
        {
            assert(size <= N);

            if (_capacity != N)
            {
                _deallocate(_data);
            }

            _data = &_buffer[0];
            _capacity = N;
            _size = size;

            return &_buffer[0];
        }

        void push_back(const T& value)
        {
            emplace_back(value);
        }

        void push_back(T&& value)
        {
            emplace_back(std::move(value));
        }

        template<typename... Args>
        reference emplace_back(Args&&... args)
        {
            const auto new_size = _ensure_fits(1);
            const auto it = new (_data + _size) T(std::forward<Args>(args)...);
            _size = new_size;
            return *it;
        }

        void pop_back()
        {
#if _ITERATOR_DEBUG_LEVEL == 2
            _STL_VERIFY(_size != 0, "empty before pop");
#endif // _ITERATOR_DEBUG_LEVEL == 2
            std::destroy_at(_data + _size - 1);
            _size--;
        }

        // NOTE: If the copy constructor throws, the range [pos, end()) is erased.
        iterator insert(const_iterator pos, const T& value)
        {
            return _generic_insert(pos, 1, [&](auto&& it) noexcept(std::is_nothrow_copy_constructible_v<T>) {
                std::construct_at(&*it, value);
            });
        }

        // NOTE: If the move constructor throws, the range [pos, end()) is erased.
        iterator insert(const_iterator pos, T&& value)
        {
            // The earlier static_assert(std::is_nothrow_move_constructible_v<T>)
            // ensures that this lambda is noexcept.
            return _generic_insert(pos, 1, [&](auto&& it) noexcept {
                std::construct_at(&*it, std::move(value));
            });
        }

        // NOTE: If the copy constructor throws, the range [pos, end()) is erased.
        iterator insert(const_iterator pos, size_type count, const T& value)
        {
            return _generic_insert(pos, count, [&](auto&& it) noexcept(std::is_nothrow_copy_constructible_v<T>) {
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
                std::uninitialized_fill_n(it, count, value);
            });
        }

        // NOTE: If the copy/move constructor throws, the range [pos, end()) is erased.
        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            return _generic_insert(pos, std::distance(first, last), [&](auto&& it) noexcept(std::is_nothrow_constructible_v<T, decltype(*first)>) {
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
                std::uninitialized_copy(first, last, it);
            });
        }

        // NOTE: If the copy constructor throws, the range [pos, end()) is erased.
        iterator insert(const_iterator pos, std::initializer_list<T> list)
        {
            return insert(pos, list.begin(), list.end());
        }

        void erase(const_iterator pos)
        {
            erase(pos, pos + 1);
        }

        void erase(const_iterator first, const_iterator last)
        {
            if (first >= last)
            {
                return;
            }

            const auto beg = begin();
            const auto off = first - cbegin();
            const auto it = beg + off;
            const auto end_old = end();
            const auto end_new = std::move<const_iterator, iterator>(last, end_old, it);
            std::destroy(end_new, end_old);
            _size = end_new - beg;
        }

    private:
        [[noreturn]] static void _throw_invalid_subscript()
        {
            throw std::out_of_range("invalid small_vector subscript");
        }

        [[noreturn]] static void _throw_too_long()
        {
            throw std::length_error("small_vector too long");
        }

        static T* _allocate(size_t size)
        {
            if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                return static_cast<T*>(::operator new(size * sizeof(T)));
            }
            else
            {
                return static_cast<T*>(::operator new(size * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
            }
        }

        static void _deallocate(T* data) noexcept
        {
            if constexpr (alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(data);
            }
            else
            {
                ::operator delete(data, static_cast<std::align_val_t>(alignof(T)));
            }
        }

        iterator _uninitialized_begin() const noexcept
        {
#if _ITERATOR_DEBUG_LEVEL >= 1
            return { _data, _data + _size, _data + _capacity };
#else
            return { _data };
#endif
        }

        void _copy_assign(const small_vector& other)
        {
            reserve(other._size);
            std::uninitialized_copy(other.begin(), other.end(), _uninitialized_begin());
            _size = other._size;
        }

        void _move_assign(small_vector& other) noexcept
        {
            if (other._capacity == N)
            {
                _data = &_buffer[0];
                _capacity = N;
                _size = other._size;
                // The earlier static_assert(std::is_nothrow_move_constructible_v<T>)
                // ensures that we don't exit in a weird state with invalid `_size`.
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
                std::uninitialized_move(other.begin(), other.end(), _uninitialized_begin());
                std::destroy(other.begin(), other.end());
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
        }

        size_type _ensure_fits(size_type add)
        {
            const auto new_size = _size + add;
            if (new_size < _size)
            {
                _throw_too_long();
            }
            if (new_size > _capacity)
            {
                _grow(new_size);
            }
            return new_size;
        }

        // What use is a small_vector when you need it to heap allocate often?
        // --> _grow() is noinline because we expect this to be called in slow paths only.
        __declspec(noinline) void _grow(size_type min_cap)
        {
            const auto new_cap = std::max(min_cap, _capacity + _capacity / 2);
            // Protect against overflow in the multiplication in _allocate.
            if (new_cap > max_size())
            {
                _throw_too_long();
            }

            const auto data = _allocate(new_cap);

            // The earlier static_assert(std::is_nothrow_move_constructible_v<T>)
            // ensures that we don't leak `data` here since no exceptions will be thrown.
            std::uninitialized_move(begin(), end(), data);
            std::destroy(begin(), end());

            if (_capacity != N)
            {
                _deallocate(_data);
            }

            _data = data;
            _capacity = new_cap;
        }

        void _generic_resize(size_type new_size, auto&& func)
        {
            if (new_size < _size)
            {
                std::destroy(begin() + new_size, end());
            }
            else if (new_size > _size)
            {
                reserve(new_size);
                func(_uninitialized_begin() + _size, _uninitialized_begin() + new_size);
            }

            _size = new_size;
        }

        iterator _generic_insert(const_iterator pos, size_type count, auto&& func)
        {
            const auto offset = pos - cbegin();
            const auto old_size = _size;
            const auto new_size = _ensure_fits(count);
            const auto moveable = old_size - offset;

            // An optimization for the most common vector type which is trivially constructible, destructible and copyable.
            // This allows us to drop exception handlers (= no need to push onto the stack) and replace two moves with just one.
            if constexpr (noexcept(func(begin())) && std::is_trivially_destructible_v<T> && std::is_trivially_copyable_v<T>)
            {
                _size = new_size;

                // Make space for `count` uninitialized items, to be filled by `func`.
                const auto it = begin() + offset;
                const auto dest = it + count;
                memmove(dest._Unwrapped(), it._Unwrapped(), moveable * sizeof(T));

                func(it);
                return it;
            }
            else
            {
                // The earlier static_assert(std::is_nothrow_move_assignable_v<T>)
                // ensures that we don't exit in a weird state with invalid `_size`.

                // 1. We've resized the vector to fit an additional `count` items.
                //    Initialize the `count` items at the end by moving existing items over.
                //
                // This line is noexcept:
                // It can't throw because of the earlier static_assert(std::is_nothrow_move_assignable_v<T>).
                const auto displacement = std::min(count, moveable);
                std::uninitialized_move(end() - displacement, end(), _uninitialized_begin() + (new_size - displacement));
                _size = new_size;

                // 2. Now that all `new_size` items are initialized we can move as many
                //    items towards the end as we need to make space for `count` items.
                //
                // This line is noexcept:
                // It can't throw because of the earlier static_assert(std::is_nothrow_move_assignable_v<T>).
                const auto displacement_beg = begin() + offset;
                const auto displacement_end = displacement_beg + displacement;
                std::move_backward(displacement_beg, displacement_beg + (moveable - displacement), end() - displacement);

                // 3. Destroy the displaced existing items, so that we can use
                //    std::uninitialized_*/std::construct_* functions inside `func`.
                //    This isn't optimal, but better than nothing.
                std::destroy(displacement_beg, displacement_end);

                try
                {
                    func(displacement_beg);
                }
                catch (...)
                {
                    std::move(displacement_beg + count, end(), displacement_beg);
                    std::destroy(displacement_end, end());
                    _size = old_size;
                    throw;
                }

                return displacement_beg;
            }
        }

        T* _data;
        size_t _capacity;
        size_t _size;
        T _buffer[N];
    };
}

#pragma warning(pop)
