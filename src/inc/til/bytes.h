// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "type_traits.h"

namespace til
{
    template<ContiguousBytes Target>
    constexpr void bytes_advance(Target& target, size_t count)
    {
        if (count > target.size())
        {
            throw std::length_error{ "insufficient space left" };
        }

        target = { target.data() + count, target.size() - count };
    }

    template<TriviallyCopyable T, ContiguousBytes Target>
    constexpr bool bytes_can_put(const Target& target)
    {
        return target.size() >= sizeof(T);
    }

    template<TriviallyCopyable T, ContiguousBytes Target>
    constexpr void bytes_put(Target& target, const T& value)
    {
        using TargetType = typename Target::value_type;
        constexpr auto size = sizeof(value);

        if (size > target.size())
        {
            throw std::length_error{ "insufficient space left" };
        }

        std::copy_n(reinterpret_cast<const TargetType*>(&value), size, target.data());
        target = { target.data() + size, target.size() - size };
    }

    template<ContiguousBytes Target, ContiguousView Source>
        requires TriviallyCopyable<typename Source::value_type>
    constexpr void bytes_transfer(Target& target, Source& source)
    {
        using TargetType = typename Target::value_type;
        constexpr auto elementSize = sizeof(typename Source::value_type);

        const auto sourceCount = std::min(source.size(), target.size() / elementSize);
        const auto targetCount = sourceCount * elementSize;

        std::copy_n(reinterpret_cast<const TargetType*>(source.data()), targetCount, target.data());

        target = { target.data() + targetCount, target.size() - targetCount };
        source = { source.data() + sourceCount, source.size() - sourceCount };
    }
}
