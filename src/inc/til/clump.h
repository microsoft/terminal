// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <vector>
#include <optional>
#include <gsl/span>

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    namespace details
    {
        template<typename T, typename TValIterator, typename TSizeIterator>
        class _clump_iterator_base
        {
        public:
            using difference_type = std::ptrdiff_t;
            using value_type = gsl::span<const T>;
            using const_reference = const value_type&;
            _clump_iterator_base(const TValIterator& c, const std::optional<TSizeIterator>& s) :
                _c(c),
                _s(s) {}

            _clump_iterator_base& operator++()
            {
                const auto s{ _s ? *(*_s)++ : 1 };
                _c += s;
                return *this;
            }

            _clump_iterator_base operator++(int)
            {
                auto ret{ *this };
                ++ret;
                return ret;
            }

            bool operator==(const _clump_iterator_base& other) const
            {
                return _c == other._c && _s == other._s;
            }

            bool operator!=(const _clump_iterator_base& other) const
            {
                return !(*this == other);
            }

            const value_type operator*()
            {
                const auto s{ _s ? **_s : 1 };
                return { &*_c, s };
            }

        private:
            TValIterator _c;
            std::optional<TSizeIterator> _s;
        };

        template<typename T>
        using clump_iterator = _clump_iterator_base<T, typename std::vector<T>::const_iterator, typename std::vector<size_t>::const_iterator>;
        template<typename T>
        using clump_view_iterator = _clump_iterator_base<T, typename gsl::span<const T>::iterator, typename gsl::span<const size_t>::iterator>;

        // Returns nullopt if the optional has no value,
        // or the result of evaluating lambda() if it does.
        // This allows for member access off an optional
        // somewhat like C#'s "foo.?bar" syntax. Just, way uglier.
        template<typename T, typename TLambda>
        auto eval_or_nullopt(const std::optional<T>& opt, TLambda&& l) -> std::optional<typename std::decay<decltype(l(*opt))>::type>
        {
            if (opt)
            {
                return l(*opt);
            }
            return std::nullopt;
        }
    }

    // clump is a vector intended to be consumed in chunks.
    // It is stored as a vector of T with an optional vector
    // of lengths as a sidecar.
    // If the length vector is missing, it is assumed that
    // each component is of length 1.
    //
    //          +-----------------------------+---------+----+
    // Sizes    | 6                           | 2       | 1  |
    //          +--------Region 1-------------+--Rgn 2--+-R3-+
    // Contents | 38 |  2 |  0 | 12 | 34 | 56 |  4 |  2 |  2 |
    //          +----+----+----+----+----+----+----+----+----+
    //
    // During iteration, this clump will produce three spans:
    // {38, 2, 0, 12, 34, 56}
    // {4, 2}
    // {2}
    //
    // Sizes    [ UNSPECIFIED       ]
    //          +----+----+----+----+
    // Contents | 38 |  5 | 68 |  8 |
    //          +----+----+----+----+
    //
    // During iteration, this clump will produce four spans:
    // {38}
    // {5}
    // {68}
    // {8}
    template<typename T>
    class clump
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using const_iterator = details::clump_iterator<T>;

        clump(){};

        void clear()
        {
            _contents.clear();
            _sizes.reset();
        }

        size_t size() const
        {
            // if we have no sizes, each one is 1 (so we can use _contents' size)
            return _sizes ? _sizes->size() : _contents.size();
        }

        bool empty() const
        {
            return _contents.empty();
        }

        reference front()
        {
            return _contents.front();
        }

        reference back()
        {
            return _contents.back();
        }

        void push_back(const T& v)
        {
            emplace_back(T{ v });
        }

        void push_glom(const T& v)
        {
            emplace_glom(T{ v });
        }

        template<typename... Args>
        void emplace_back(Args&&... args)
        {
            _contents.emplace_back(std::forward<Args>(args)...);
            if (_sizes)
            {
                _sizes->emplace_back(1u);
            }
        }

        template<typename... Args>
        void emplace_glom(Args&&... args)
        {
            _ensureSizes(); // might fill _sizes with {}
            if (_sizes->size() > 0)
            {
                _sizes->back()++;
            }
            else
            {
                _sizes->emplace_back(1u);
            }
            _contents.emplace_back(std::forward<Args>(args)...);
        }

        const_iterator begin()
        {
            return const_iterator{ _contents.cbegin(), details::eval_or_nullopt(_sizes, [](auto&& s) { return s.cbegin(); }) };
        }

        const_iterator end()
        {
            return const_iterator{ _contents.cend(), details::eval_or_nullopt(_sizes, [](auto&& s) { return s.cend(); }) };
        }

    private:
        template<typename U>
        friend class clump_view;

        void _ensureSizes()
        {
            if (!_sizes)
            {
                typename std::decay<decltype(*_sizes)>::type v;
                v.resize(_contents.size(), 1u); // fill with 1s
                _sizes = std::move(v);
            }
        }

        std::vector<T> _contents;
        std::optional<std::vector<size_type>> _sizes;
    };

    // clump_view is a read-only view over a clump
    template<typename T>
    class clump_view
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using const_iterator = details::clump_view_iterator<T>;

        clump_view(const clump<T>& clump) :
            _contents{ clump._contents },
            _sizes{ std::nullopt }
        {
            if (clump._sizes)
            {
                _sizes = clump._sizes;
            }
        }

        size_t size() const
        {
            // if we have no sizes, each one is 1 (so we can use _contents' size)
            return _sizes ? _sizes->size() : _contents.size();
        }

        bool empty() const
        {
            return _contents.empty();
        }

        const value_type front()
        {
            return _contents.front();
        }

        const value_type back()
        {
            return _contents.back();
        }

        const_iterator begin()
        {
            return const_iterator{ _contents.begin(), details::eval_or_nullopt(_sizes, [](auto&& s) { return s.begin(); }) };
        }

        const_iterator end()
        {
            return const_iterator{ _contents.end(), details::eval_or_nullopt(_sizes, [](auto&& s) { return s.end(); }) };
        }

    private:
        gsl::span<const T> _contents;
        std::optional<gsl::span<const size_t>> _sizes;
    };
}
