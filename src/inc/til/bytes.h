// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

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

    // memmove(), but you can specify a stride! This can be useful for copying between bitmaps.
    // A stride is (usually) the number of bytes between two rows in a bitmap. The stride doesn't necessarily
    // equal the actual number of pixels between rows, for instance for memory alignment purposes.
    //
    // All sizes are in bytes.
    //
    // Higher abstractions could be built on top of this function.
    void bytes_strided_copy(void* target, size_t targetStride, size_t targetSize, const void* source, size_t sourceStride, size_t sourceSize) noexcept
    {
        // Strides are supposed to be smaller than the whole bitmap size and the remaining code assumes that too.
        targetStride = std::min(targetStride, targetSize);
        sourceStride = std::min(sourceStride, sourceSize);

        auto targetPtr = static_cast<uint8_t*>(target);
        auto sourcePtr = static_cast<const uint8_t*>(source);

        // If the two bitmaps have the same stride we can just copy them in one go.
        if (sourceStride == targetStride)
        {
            memmove(targetPtr, sourcePtr, std::min(targetSize, sourceSize));
        }
        else
        {
            const auto targetEnd = targetPtr + targetSize;
            const auto sourceEnd = sourcePtr + sourceSize;
            // The max. amount we can copy per row is the min. width (the intersection).
            const auto width = std::min(targetStride, sourceStride);

            while (targetPtr < targetEnd && sourcePtr < sourceEnd)
            {
                memmove(targetPtr, sourcePtr, width);
                targetPtr += targetStride;
                sourcePtr += sourceStride;
            }
        }
    }

    // Fills the given rectangle inside target with the given value.
    // All values are in units of T, not in bytes.
    template<TriviallyCopyable T>
    void rect_fill(T* target, size_t targetStride, size_t targetSize, T value, size_t left, size_t top, size_t right, size_t bottom) noexcept
    {
        // Strides are supposed to be smaller than the whole bitmap size and the remaining code assumes that too.
        targetStride = std::min(targetStride, targetSize);

        // Ensure that the rectangle is valid (left <= right && top <= bottom)
        // and within bounds (right <= width && bottom <= height).
        right = std::min(right, targetStride);
        left = std::min(left, right);
        bottom = std::min(bottom, targetSize / targetStride);
        top = std::min(top, bottom);

        const auto height = bottom - top;
        const auto width = right - left;
        const auto offsetBeg = top * targetStride + left;
        const auto offsetEnd = bottom * targetStride + right;

        const auto targetEnd = target + offsetEnd;
        target = target + offsetBeg;

        // If we're allowed to fill entire rows at a time, we don't need to loop around the memset().
        if (width == targetStride)
        {
            if constexpr (sizeof(T) == 1)
            {
                // Memset is generally expected to be the fasted way to clear memory.
                memset(target, static_cast<unsigned char>(value), static_cast<size_t>(targetEnd - target));
            }
            else
            {
                // This should ideally compile down to a `rep stosb` or similar.
                for (; target < targetEnd; ++target)
                {
                    *target = value;
                }
            }
        }
        else
        {
            // Same as the above but with a loop around it.
            while (target < targetEnd)
            {
                if constexpr (sizeof(T) == 1)
                {
                    memset(target, static_cast<unsigned char>(value), width * sizeof(T));
                }
                else
                {
                    for (auto it = target, end = it + width; it < end; ++it)
                    {
                        *it = value;
                    }
                }

                target += targetStride;
            }
        }
    }
}
