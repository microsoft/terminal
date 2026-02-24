#pragma once

namespace mem
{
    template<typename T>
    struct is_std_view : std::false_type
    {
    };
    template<typename U, std::size_t E>
    struct is_std_view<std::span<U, E>> : std::true_type
    {
    };
    template<typename U, typename V>
    struct is_std_view<std::basic_string_view<U, V>> : std::true_type
    {
    };

    template<typename T>
    concept is_trivially_constructible = std::is_trivially_constructible_v<T> || is_std_view<std::remove_cv_t<T>>::value;

    template<typename T>
    concept is_trivially_copyable = std::is_trivially_copyable_v<T>;

    struct Arena
    {
        explicit Arena(size_t bytes);
        ~Arena();

        void clear();
        size_t get_pos() const;
        void pop_to(size_t pos);

        template<is_trivially_copyable T>
        T* push_zeroed(size_t count = 1)
        {
            return static_cast<T*>(_push_zeroed(sizeof(T) * count, alignof(T)));
        }

        template<is_trivially_copyable T>
        std::span<T> push_zeroed_span(size_t count)
        {
            return { static_cast<T*>(_push_zeroed(sizeof(T) * count, alignof(T))), count };
        }

        template<is_trivially_copyable T>
        T* push_uninitialized(size_t count = 1)
        {
            return static_cast<T*>(_push_uninitialized(sizeof(T) * count, alignof(T)));
        }

        template<is_trivially_copyable T>
        std::span<T> push_uninitialized_span(size_t count)
        {
            return { static_cast<T*>(_push_uninitialized(sizeof(T) * count, alignof(T))), count };
        }

    private:
        void* _push_raw(size_t bytes, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        void* _push_zeroed(size_t bytes, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
        void* _push_uninitialized(size_t bytes, size_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__);

        uint8_t* m_alloc = nullptr;
        size_t m_commit = 0;
        size_t m_pos = 0;
    };

    struct ScopedArena
    {
        Arena& arena;

        ScopedArena(Arena& arena);
        ~ScopedArena();

    private:
        size_t m_pos_backup;
    };

    [[nodiscard]] ScopedArena get_scratch_arena();
    [[nodiscard]] ScopedArena get_scratch_arena(const Arena& conflict);

    std::string_view format(Arena& arena, _Printf_format_string_ const char* fmt, va_list args);
    std::string_view format(Arena& arena, _Printf_format_string_ const char* fmt, ...);
    std::wstring_view format(Arena& arena, _Printf_format_string_ const wchar_t* fmt, va_list args);
    std::wstring_view format(Arena& arena, _Printf_format_string_ const wchar_t* fmt, ...);

    void print_literal(const char* str);
    void print_format(Arena& arena, _Printf_format_string_ const char* fmt, ...);
    std::string_view read_line(Arena& arena, size_t max_bytes);

    std::wstring_view u8u16(Arena& arena, const char* ptr, size_t count);
    std::string_view u16u8(Arena& arena, const wchar_t* ptr, size_t count);

    template<is_trivially_copyable T>
    void copy(T* dst, const T* src, size_t count)
    {
        memcpy(dst, src, count * sizeof(T));
    }

    template<typename T>
    auto repeat(Arena& arena, const T& in, size_t count) -> decltype(auto)
    {
        if constexpr (is_std_view<T>::value)
        {
            const auto data = in.data();
            const auto size = in.size();
            const auto len = count * size;
            const auto buf = arena.push_uninitialized<typename T::value_type>(len);

            for (size_t i = 0; i < count; ++i)
            {
                mem::copy(buf + i * size, data, size);
            }

            return T{ buf, len };
        }
        else
        {
            const auto buf = arena.push_uninitialized<T>(count);

            for (size_t i = 0; i < count; ++i)
            {
                memcpy(buf + i, &in, sizeof(T));
            }

            return std::span{ buf, count };
        }
    }
}
