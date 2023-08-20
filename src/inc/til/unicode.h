// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
    namespace details
    {
        inline constexpr wchar_t UNICODE_REPLACEMENT = 0xFFFD;
    }

    static constexpr bool is_surrogate(const wchar_t wch) noexcept
    {
        return (wch & 0xF800) == 0xD800;
    }

    static constexpr bool is_leading_surrogate(const wchar_t wch) noexcept
    {
        return (wch & 0xFC00) == 0xD800;
    }

    static constexpr bool is_trailing_surrogate(const wchar_t wch) noexcept
    {
        return (wch & 0xFC00) == 0xDC00;
    }

    // Verifies the beginning of the given UTF16 string and returns the first UTF16 sequence
    // or U+FFFD otherwise. It's not really useful and at the time of writing only a
    // single caller uses this. It's best to delete this if you read this comment.
    constexpr std::wstring_view utf16_next(std::wstring_view wstr) noexcept
    {
        auto it = wstr.begin();
        const auto end = wstr.end();
        auto ptr = &details::UNICODE_REPLACEMENT;
        size_t len = 1;

        if (it != end)
        {
            const auto wch = *it;
            ptr = &*it;

            if (is_surrogate(wch))
            {
                ++it;
                const auto wch2 = it != end ? *it : wchar_t{};
                if (is_leading_surrogate(wch) && is_trailing_surrogate(wch2))
                {
                    len = 2;
                    ++it;
                }
                else
                {
                    ptr = &details::UNICODE_REPLACEMENT;
                }
            }
        }

        return { ptr, len };
    }

    // Returns the index of the next codepoint in the given wstr (i.e. after the codepoint that idx points at).
    constexpr size_t utf16_iterate_next(const std::wstring_view& wstr, size_t idx) noexcept
    {
        if (idx < wstr.size() && is_leading_surrogate(til::at(wstr, idx++)))
        {
            if (idx < wstr.size() && is_trailing_surrogate(til::at(wstr, idx)))
            {
                ++idx;
            }
        }
        return idx;
    }

    // Returns the index of the preceding codepoint in the given wstr (i.e. in front of the codepoint that idx points at).
    constexpr size_t utf16_iterate_prev(const std::wstring_view& wstr, size_t idx) noexcept
    {
        if (idx > 0 && is_trailing_surrogate(til::at(wstr, --idx)))
        {
            if (idx > 0 && is_leading_surrogate(til::at(wstr, idx - 1)))
            {
                --idx;
            }
        }
        return idx;
    }

    // Splits a UTF16 string into codepoints, yielding `wstring_view`s of UTF16 text. Use it as:
    //   for (const auto& str : til::utf16_iterator{ input }) { ... }
    struct utf16_iterator
    {
        struct sentinel
        {
        };

        struct iterator
        {
            using iterator_category = std::forward_iterator_tag;
            using value_type = std::wstring_view;
            using reference = value_type&;
            using pointer = value_type*;
            using difference_type = std::ptrdiff_t;

            explicit constexpr iterator(utf16_iterator& p) noexcept :
                _iter{ p }
            {
            }

            const value_type& operator*() const noexcept
            {
                return _iter.value();
            }

            iterator& operator++() noexcept
            {
                _iter._advance = true;
                return *this;
            }

            bool operator!=(const sentinel&) const noexcept
            {
                return _iter.valid();
            }

        private:
            utf16_iterator& _iter;
        };

        explicit constexpr utf16_iterator(std::wstring_view wstr) noexcept :
            _it{ wstr.begin() }, _end{ wstr.end() }, _advance{ _it != _end }
        {
        }

        iterator begin() noexcept
        {
            return iterator{ *this };
        }

        sentinel end() noexcept
        {
            return sentinel{};
        }

    private:
        bool valid() const noexcept
        {
            return _it != _end;
        }

        void advance() noexcept
        {
            const auto wch = *_it;
            auto ptr = &*_it;
            size_t len = 1;

            ++_it;

            if (is_surrogate(wch))
            {
                const auto wch2 = _it != _end ? *_it : wchar_t{};
                if (is_leading_surrogate(wch) && is_trailing_surrogate(wch2))
                {
                    len = 2;
                    ++_it;
                }
                else
                {
                    ptr = &details::UNICODE_REPLACEMENT;
                }
            }

            _value = { ptr, len };
            _advance = false;
        }

        const std::wstring_view& value() noexcept
        {
            if (_advance)
            {
                advance();
            }
            return _value;
        }

        std::wstring_view::iterator _it;
        std::wstring_view::iterator _end;
        std::wstring_view _value;
        bool _advance = true;
    };
}
