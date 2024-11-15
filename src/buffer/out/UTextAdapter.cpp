// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UTextAdapter.h"

#include "textBuffer.hpp"

// All of these are somewhat annoying when trying to implement RefcountBuffer.
// You can't stuff a unique_ptr into ut->q (= void*) after all.
#pragma warning(disable : 26402) // Return a scoped object instead of a heap-allocated if it has a move constructor (r.3).
#pragma warning(disable : 26403) // Reset or explicitly delete an owner<T> pointer '...' (r.3).
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).

struct RowRange
{
    til::CoordType begin;
    til::CoordType end;
};

struct RefcountBuffer
{
    size_t references;
    size_t capacity;
    wchar_t data[1];

    static RefcountBuffer* EnsureCapacityForOverwrite(RefcountBuffer* buffer, size_t capacity)
    {
        // We must not just ensure that `buffer` has at least `capacity`, but also that its reference count is <= 1, because otherwise we would resize a shared buffer.
        if (buffer != nullptr && buffer->references <= 1 && buffer->capacity >= capacity)
        {
            return buffer;
        }

        const auto oldCapacity = buffer ? buffer->capacity << 1 : 0;
        const auto newCapacity = std::max(capacity + 128, oldCapacity);
        const auto newBuffer = static_cast<RefcountBuffer*>(::operator new(sizeof(RefcountBuffer) - sizeof(data) + newCapacity * sizeof(wchar_t)));

        if (!newBuffer)
        {
            return nullptr;
        }

        if (buffer)
        {
            buffer->Release();
        }

        // Copying the old buffer's data is not necessary because utextAccess() will scribble right over it.
        newBuffer->references = 1;
        newBuffer->capacity = newCapacity;
        return newBuffer;
    }

    void AddRef() noexcept
    {
        // With our usage patterns, either of these two would indicate
        // an unbalanced AddRef/Release or a memory corruption.
        assert(references > 0 && references < 1000);
        references++;
    }

    void Release() noexcept
    {
        // With our usage patterns, either of these two would indicate
        // an unbalanced AddRef/Release or a memory corruption.
        assert(references > 0 && references < 1000);
        if (--references == 0)
        {
            ::operator delete(this);
        }
    }
};

constexpr size_t& accessLength(UText* ut) noexcept
{
    static_assert(sizeof(ut->p) == sizeof(size_t));
    return *std::bit_cast<size_t*>(&ut->p);
}

constexpr RefcountBuffer*& accessBuffer(UText* ut) noexcept
{
    static_assert(sizeof(ut->q) == sizeof(RefcountBuffer*));
    return *std::bit_cast<RefcountBuffer**>(&ut->q);
}

constexpr RowRange& accessRowRange(UText* ut) noexcept
{
    static_assert(sizeof(ut->a) == sizeof(RowRange));
    return *std::bit_cast<RowRange*>(&ut->a);
}

constexpr til::CoordType& accessCurrentRow(UText* ut) noexcept
{
    return ut->b;
}

// An excerpt from the ICU documentation:
//
// Clone a UText. Much like opening a UText where the source text is itself another UText.
//
// A shallow clone replicates only the UText data structures; it does not make
// a copy of the underlying text. Shallow clones can be used as an efficient way to
// have multiple iterators active in a single text string that is not being modified.
//
// A shallow clone operation must not fail except for truly exceptional conditions such
// as memory allocation failures.
//
// @param dest   A UText struct to be filled in with the result of the clone operation,
//               or NULL if the clone function should heap-allocate a new UText struct.
// @param src    The UText to be cloned.
// @param deep   true to request a deep clone, false for a shallow clone.
// @param status Errors are returned here.  For deep clones, U_UNSUPPORTED_ERROR should
//               be returned if the text provider is unable to clone the original text.
// @return       The newly created clone, or NULL if the clone operation failed.
static UText* U_CALLCONV utextClone(UText* dest, const UText* src, UBool deep, UErrorCode* status) noexcept
{
    __assume(status != nullptr);

    if (deep)
    {
        *status = U_UNSUPPORTED_ERROR;
        return dest;
    }

    dest = utext_setup(dest, 0, status);
    if (*status > U_ZERO_ERROR)
    {
        return dest;
    }

    memcpy(dest, src, sizeof(UText));
    if (const auto buf = accessBuffer(dest))
    {
        buf->AddRef();
    }
    return dest;
}

// An excerpt from the ICU documentation:
//
// Gets the length of the text.
//
// @param ut the UText to get the length of.
// @return the length, in the native units of the original text string.
static int64_t U_CALLCONV utextNativeLength(UText* ut) noexcept
try
{
    auto length = accessLength(ut);

    if (!length)
    {
        const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
        const auto range = accessRowRange(ut);

        for (til::CoordType y = range.begin; y < range.end; ++y)
        {
            const auto& row = textBuffer.GetRowByOffset(y);
            // Later down below we'll add a newline to the text if !wasWrapForced, so we need to account for that here.
            length += row.GetText().size() + !row.WasWrapForced();
        }

        accessLength(ut) = length;
    }

    return gsl::narrow_cast<int64_t>(length);
}
catch (...)
{
    return 0;
}

// An excerpt from the ICU documentation:
//
// Get the description of the text chunk containing the text at a requested native index.
// The UText's iteration position will be left at the requested index.
// If the index is out of bounds, the iteration position will be left
// at the start or end of the string, as appropriate.
//
// @param ut          the UText being accessed.
// @param nativeIndex Requested index of the text to be accessed.
// @param forward     If true, then the returned chunk must contain text starting from the index, so that start<=index<limit.
//                    If false, then the returned chunk must contain text before the index, so that start<index<=limit.
// @return            True if the requested index could be accessed. The chunk will contain the requested text.
//                    False value if a chunk cannot be accessed (the requested index is out of bounds).
static UBool U_CALLCONV utextAccess(UText* ut, int64_t nativeIndex, UBool forward) noexcept
try
{
    auto neededIndex = nativeIndex;
    // This will make it simpler for us to search the row that contains the nativeIndex,
    // because we'll now only need to check for `start<=index<limit` and nothing else.
    if (!forward)
    {
        neededIndex--;
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto range = accessRowRange(ut);
    const auto startOld = ut->chunkNativeStart;
    const auto limitOld = ut->chunkNativeLimit;
    auto start = startOld;
    auto limit = limitOld;

    if (neededIndex < startOld || neededIndex >= limitOld)
    {
        auto y = accessCurrentRow(ut);
        std::wstring_view text;
        bool wasWrapForced = false;

        if (neededIndex < start)
        {
            do
            {
                --y;
                if (y < range.begin)
                {
                    break;
                }

                const auto& row = textBuffer.GetRowByOffset(y);
                text = row.GetText();
                wasWrapForced = row.WasWrapForced();

                limit = start;
                // Later down below we'll add a newline to the text if !wasWrapForced, so we need to account for that here.
                start -= text.size() + !wasWrapForced;
            } while (neededIndex < start);
        }
        else
        {
            do
            {
                ++y;
                if (y >= range.end)
                {
                    break;
                }

                const auto& row = textBuffer.GetRowByOffset(y);
                text = row.GetText();
                wasWrapForced = row.WasWrapForced();

                start = limit;
                // Later down below we'll add a newline to the text if !wasWrapForced, so we need to account for that here.
                limit += text.size() + !wasWrapForced;
            } while (neededIndex >= limit);
        }

        assert(start >= 0);
        // If we have already calculated the total length we can also assert that the limit is in range.
        assert(ut->p == nullptr || static_cast<size_t>(limit) <= accessLength(ut));

        // Even if we went out-of-bounds, we still need to update the chunkContents to contain the first/last chunk.
        if (limit != limitOld)
        {
            if (!wasWrapForced)
            {
                const auto newSize = text.size() + 1;
                const auto buffer = RefcountBuffer::EnsureCapacityForOverwrite(accessBuffer(ut), newSize);

                memcpy(&buffer->data[0], text.data(), text.size() * sizeof(wchar_t));
                til::at(buffer->data, text.size()) = L'\n';

                text = { &buffer->data[0], newSize };
                accessBuffer(ut) = buffer;
            }

            accessCurrentRow(ut) = y;
            ut->chunkNativeStart = start;
            ut->chunkNativeLimit = limit;
            ut->chunkLength = gsl::narrow_cast<int32_t>(text.size());
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
            ut->chunkContents = reinterpret_cast<const char16_t*>(text.data());
            ut->nativeIndexingLimit = ut->chunkLength;
        }
    }

    // The ICU documentation is a little bit misleading. It states:
    // > @param forward    [...] If true, start<=index<limit. If false, [...] start<index<=limit.
    // but that's just for finding the target chunk. The chunkOffset is not actually constrained to that!
    // std::clamp will perform a<=b<=c, which is what we want.
    const auto clampedIndex = std::clamp(nativeIndex, start, limit);
    auto offset = gsl::narrow_cast<int32_t>(clampedIndex - start);
    // Don't leave the offset on a trailing surrogate pair. See U16_SET_CP_START.
    // This assumes that the TextBuffer contains valid UTF-16 which may theoretically not be the case.
    if (offset > 0 && offset < ut->chunkLength && U16_IS_TRAIL(til::at(ut->chunkContents, offset)))
    {
        offset--;
    }
    ut->chunkOffset = offset;

    return neededIndex >= start && neededIndex < limit;
}
catch (...)
{
    return false;
}

// An excerpt from the ICU documentation:
//
// Extract text from a UText into a UChar buffer.
// The size (number of 16 bit UChars) in the data to be extracted is returned.
// The full amount is returned, even when the specified buffer size is smaller.
// The extracted string must be NUL-terminated if there is sufficient space in the destination buffer.
//
// @param  ut            the UText from which to extract data.
// @param  nativeStart   the native index of the first character to extract.
// @param  nativeLimit   the native string index of the position following the last character to extract.
// @param  dest          the UChar (UTF-16) buffer into which the extracted text is placed
// @param  destCapacity  The size, in UChars, of the destination buffer. May be zero for precomputing the required size.
// @param  status        receives any error status. If U_BUFFER_OVERFLOW_ERROR: Returns number of UChars for preflighting.
// @return Number of UChars in the data.  Does not include a trailing NUL.
//
// NOTE: utextExtract's correctness hasn't been verified yet. The code remains, just incase its functionality is needed in the future.
#pragma warning(suppress : 4505) // 'utextExtract': unreferenced function with internal linkage has been removed
static int32_t U_CALLCONV utextExtract(UText* ut, int64_t nativeStart, int64_t nativeLimit, char16_t* dest, int32_t destCapacity, UErrorCode* status) noexcept
try
{
    __assume(status != nullptr);

    if (*status > U_ZERO_ERROR)
    {
        return 0;
    }
    if (destCapacity < 0 || (dest == nullptr && destCapacity > 0) || nativeStart > nativeLimit)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (!utextAccess(ut, nativeStart, true))
    {
        return 0;
    }

    nativeLimit = std::min(ut->chunkNativeLimit, nativeLimit);

    if (destCapacity <= 0)
    {
        return gsl::narrow_cast<int32_t>(nativeLimit - nativeStart);
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto y = accessCurrentRow(ut);
    const auto offset = ut->chunkNativeStart - nativeStart;
    const auto text = textBuffer.GetRowByOffset(y).GetText().substr(gsl::narrow_cast<size_t>(std::max<int64_t>(0, offset)));
    const auto destCapacitySizeT = gsl::narrow_cast<size_t>(destCapacity);
    const auto length = std::min(destCapacitySizeT, text.size());

    memcpy(dest, text.data(), length * sizeof(char16_t));

    if (length < destCapacitySizeT)
    {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
        dest[length] = 0;
    }

    return gsl::narrow_cast<int32_t>(length);
}
catch (...)
{
    // The only thing that can fail is GetRowByOffset() which in turn can only fail when VirtualAlloc() fails.
    *status = U_MEMORY_ALLOCATION_ERROR;
    return 0;
}

static void U_CALLCONV utextClose(UText* ut) noexcept
{
    if (const auto buffer = accessBuffer(ut))
    {
        buffer->Release();
    }
}

static constexpr UTextFuncs utextFuncs{
    .tableSize = sizeof(UTextFuncs),
    .clone = utextClone,
    .nativeLength = utextNativeLength,
    .access = utextAccess,
    .close = utextClose,
};

// Creates a UText from the given TextBuffer that spans rows [rowBeg,RowEnd).
Microsoft::Console::ICU::unique_utext Microsoft::Console::ICU::UTextFromTextBuffer(const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd) noexcept
{
#pragma warning(suppress : 26477) // Use 'nullptr' rather than 0 or NULL (es.47).
    unique_utext ut{ UTEXT_INITIALIZER };

    UErrorCode status = U_ZERO_ERROR;
    utext_setup(&ut, 0, &status);
    FAIL_FAST_IF(status > U_ZERO_ERROR);

    ut.providerProperties = (1 << UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE) | (1 << UTEXT_PROVIDER_STABLE_CHUNKS);
    ut.pFuncs = &utextFuncs;
    ut.context = &textBuffer;
    accessCurrentRow(&ut) = rowBeg - 1; // the utextAccess() below will advance this by 1.
    accessRowRange(&ut) = { rowBeg, rowEnd };

    utextAccess(&ut, 0, true);
    return ut;
}

Microsoft::Console::ICU::unique_uregex Microsoft::Console::ICU::CreateRegex(const std::wstring_view& pattern, uint32_t flags, UErrorCode* status) noexcept
{
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
    const auto re = uregex_open(reinterpret_cast<const char16_t*>(pattern.data()), gsl::narrow_cast<int32_t>(pattern.size()), flags, nullptr, status);
    // ICU describes the time unit as being dependent on CPU performance and "typically [in] the order of milliseconds",
    // but this claim seems highly outdated already. On my CPU from 2021, a limit of 4096 equals roughly 600ms.
    uregex_setTimeLimit(re, 4096, status);
    uregex_setStackLimit(re, 4 * 1024 * 1024, status);
    return unique_uregex{ re };
}

// Returns a half-open [beg,end) range given a text start and end position.
// This function is designed to be used with uregex_start64/uregex_end64.
til::point_span Microsoft::Console::ICU::BufferRangeFromMatch(UText* ut, URegularExpression* re)
{
    UErrorCode status = U_ZERO_ERROR;
    const auto nativeIndexBeg = uregex_start64(re, 0, &status);
    const auto nativeIndexEnd = uregex_end64(re, 0, &status);

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    til::point_span ret;

    if (utextAccess(ut, nativeIndexBeg, true))
    {
        const auto y = accessCurrentRow(ut);
        ret.start.x = textBuffer.GetRowByOffset(y).GetLeadingColumnAtCharOffset(ut->chunkOffset);
        ret.start.y = y;
    }
    else
    {
        ret.start.y = accessRowRange(ut).begin;
    }

    if (utextAccess(ut, nativeIndexEnd, true))
    {
        const auto y = accessCurrentRow(ut);
        ret.end.x = textBuffer.GetRowByOffset(y).GetLeadingColumnAtCharOffset(ut->chunkOffset);
        ret.end.y = y;
    }
    else
    {
        ret.end = ret.start;
    }

    return ret;
}
