// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

namespace til
{
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

    constexpr std::wstring_view utf16_next(std::wstring_view wstr) noexcept
    {
        auto it = wstr.begin();
        const auto end = wstr.end();
        auto p = L"\uFFFD";
        size_t l = 1;

        if (it != end)
        {
            const auto wch = *it;
            p = &*it;

            if (is_surrogate(wch))
            {
                ++it;
                const auto wch2 = it != end ? *it : wchar_t{};
                if (is_leading_surrogate(wch) && is_trailing_surrogate(wch2))
                {
                    l = 2;
                    ++it;
                }
                else
                {
                    p = L"\uFFFD";
                }
            }
        }

        return { p, l };
    }

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
                _p{ p }
            {
            }

            const value_type& operator*() const noexcept
            {
                return _p.value();
            }

            iterator& operator++() noexcept
            {
                _p._advance = true;
                return *this;
            }

            bool operator!=(const sentinel&) const noexcept
            {
                return _p.valid();
            }

        private:
            utf16_iterator& _p;
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
            auto p = &*_it;
            size_t l = 1;

            ++_it;

            if (is_surrogate(wch))
            {
                const auto wch2 = _it != _end ? *_it : wchar_t{};
                if (is_leading_surrogate(wch) && is_trailing_surrogate(wch2))
                {
                    l = 2;
                    ++_it;
                }
                else
                {
                    p = L"\uFFFD";
                }
            }

            _v = { p, l };
            _advance = false;
        }

        const std::wstring_view& value() noexcept
        {
            if (_advance)
            {
                advance();
            }
            return _v;
        }

        std::wstring_view::iterator _it;
        std::wstring_view::iterator _end;
        std::wstring_view _v;
        bool _advance = true;
    };
}
