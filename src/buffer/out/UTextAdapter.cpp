// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UTextAdapter.h"

#include "textBuffer.hpp"

struct RowRange
{
    til::CoordType begin;
    til::CoordType end;
};

constexpr size_t& accessLength(UText* ut) noexcept
{
    return *std::bit_cast<size_t*>(&ut->p);
}

constexpr RowRange& accessRowRange(UText* ut) noexcept
{
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
    if (*status <= U_ZERO_ERROR)
    {
        memcpy(dest, src, sizeof(UText));
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
            length += textBuffer.GetRowByOffset(y).GetText().size();
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
    if (!forward)
    {
        // Even after reading the ICU documentation I'm a little unclear on how to handle the forward flag.
        // I _think_ it's basically nothing but "nativeIndex--" for us, but I didn't want to verify it
        // because right now we never use any ICU functions that require backwards text access anyways.
        return false;
    }

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    const auto range = accessRowRange(ut);
    auto start = ut->chunkNativeStart;
    auto limit = ut->chunkNativeLimit;
    auto y = accessCurrentRow(ut);
    std::wstring_view text;

    if (nativeIndex >= start && nativeIndex < limit)
    {
        return true;
    }

    if (nativeIndex < start)
    {
        for (;;)
        {
            --y;
            if (y < range.begin)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            limit = start;
            start -= text.size();

            if (nativeIndex >= start)
            {
                break;
            }
        }
    }
    else
    {
        for (;;)
        {
            ++y;
            if (y >= range.end)
            {
                return false;
            }

            text = textBuffer.GetRowByOffset(y).GetText();
            start = limit;
            limit += text.size();

            if (nativeIndex < limit)
            {
                break;
            }
        }
    }

    accessCurrentRow(ut) = y;
    ut->chunkNativeStart = start;
    ut->chunkNativeLimit = limit;
    ut->chunkOffset = gsl::narrow_cast<int32_t>(nativeIndex - start);
    ut->chunkLength = gsl::narrow_cast<int32_t>(text.size());
#pragma warning(suppress : 26490) // Don't use reinterpret_cast (type.1).
    ut->chunkContents = reinterpret_cast<const char16_t*>(text.data());
    ut->nativeIndexingLimit = ut->chunkLength;
    return true;
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
    const auto length = std::min(gsl::narrow_cast<size_t>(destCapacity), text.size());

    memcpy(dest, text.data(), length * sizeof(char16_t));

    if (length < destCapacity)
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

static constexpr UTextFuncs utextFuncs{
    .tableSize = sizeof(UTextFuncs),
    .clone = utextClone,
    .nativeLength = utextNativeLength,
    .access = utextAccess,
    .extract = utextExtract,
};

// Creates a UText from the given TextBuffer that spans rows [rowBeg,RowEnd).
UText* UTextFromTextBuffer(UText* ut, const TextBuffer& textBuffer, til::CoordType rowBeg, til::CoordType rowEnd, UErrorCode* status) noexcept
{
    __assume(status != nullptr);

    ut = utext_setup(ut, 0, status);
    if (*status > U_ZERO_ERROR)
    {
        return nullptr;
    }

    ut->providerProperties = (1 << UTEXT_PROVIDER_LENGTH_IS_EXPENSIVE) | (1 << UTEXT_PROVIDER_STABLE_CHUNKS);
    ut->pFuncs = &utextFuncs;
    ut->context = &textBuffer;
    accessCurrentRow(ut) = rowBeg - 1; // the utextAccess() below will advance this by 1.
    accessRowRange(ut) = { rowBeg, rowEnd };

    utextAccess(ut, 0, true);
    return ut;
}

// Returns an inclusive point range given a text start and end position. This function is designed to be used with uregex_start64/uregex_end64.
til::point_span BufferRangeFromUText(UText* ut, int64_t nativeIndexBeg, int64_t nativeIndexEnd)
{
    // The parameters are given as a half-open [beg,end) range, but the point_span we return in closed [beg,end].
    nativeIndexEnd--;

    const auto& textBuffer = *static_cast<const TextBuffer*>(ut->context);
    til::point_span ret;

    if (utextAccess(ut, nativeIndexBeg, true))
    {
        const auto y = accessCurrentRow(ut);
        ret.start.x = textBuffer.GetRowByOffset(y).GetLeadingColumnAtCharOffset(gsl::narrow_cast<ptrdiff_t>(nativeIndexBeg - ut->chunkNativeStart));
        ret.start.y = y;
    }
    else
    {
        ret.start.y = accessRowRange(ut).begin;
    }

    if (utextAccess(ut, nativeIndexEnd, true))
    {
        const auto y = accessCurrentRow(ut);
        ret.end.x = textBuffer.GetRowByOffset(y).GetTrailingColumnAtCharOffset(gsl::narrow_cast<ptrdiff_t>(nativeIndexEnd - ut->chunkNativeStart));
        ret.end.y = y;
    }
    else
    {
        ret.end = ret.start;
    }

    return ret;
}
