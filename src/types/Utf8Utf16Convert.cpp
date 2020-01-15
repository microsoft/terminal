// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Utf8Utf16Convert.hpp"

til::u8state::u8state() noexcept :
    _buffer8{},
    _utf8Partials{}
{
}

// Method Description:
// - Takes a UTF-8 string and populates it with *complete* UTF-8 codepoints.
//   If it receives an incomplete codepoint, it will cache it until it can be completed.
// Arguments:
// - in - UTF-8 string_view potentially containing partial code points
// - out - on return, populated with complete codepoints at the string end
// Return Value:
// - S_OK          - the resulting string doesn't end with a partial
// - S_FALSE       - the resulting string contains the previously cached partials only
// - E_OUTOFMEMORY - the method failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the processing was aborted
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u8state::operator()(const std::string_view in, std::string_view& out) noexcept
{
    try
    {
        size_t remainingLength{ in.length() };
        size_t capacity{};
        if (FAILED(SizeTAdd(remainingLength, _partialsLen, &capacity)))
        {
            return E_ABORT;
        }

        _buffer8.clear();
        _buffer8.reserve(capacity);

        // copy UTF-8 code units that were remaining from the previousl call (if any)
        if (_partialsLen != 0u)
        {
            _buffer8.assign(_utf8Partials.cbegin(), _utf8Partials.cbegin() + _partialsLen);
            _partialsLen = 0u;
        }

        if (in.empty())
        {
            out = _buffer8;
            if (_buffer8.empty())
            {
                return S_OK;
            }

            return S_FALSE; // the partial is given back
        }

        auto backIter = in.end() - 1;
        // If the last byte in the string was a byte belonging to a UTF-8 multi-byte character
        if ((*backIter & _Utf8BitMasks::MaskAsciiByte) > _Utf8BitMasks::IsAsciiByte)
        {
            // Check only up to 3 last bytes, if no Lead Byte was found then the byte before must be the Lead Byte and no partials are in the string
            const size_t stopLen{ std::min(in.length(), gsl::narrow_cast<size_t>(4u)) };
            for (size_t sequenceLen{ 1u }; sequenceLen < stopLen; ++sequenceLen, --backIter)
            {
                // If Lead Byte found
                if ((*backIter & _Utf8BitMasks::MaskContinuationByte) > _Utf8BitMasks::IsContinuationByte)
                {
                    // If the Lead Byte indicates that the last bytes in the string is a partial UTF-8 code point then cache them:
                    //  Use the bitmask at index `sequenceLen`. Compare the result with the operand having the same index. If they
                    //  are not equal then the sequence has to be cached because it is a partial code point. Otherwise the
                    //  sequence is a complete UTF-8 code point and the whole string is ready for the conversion to hstring.
                    if ((*backIter & _cmpMasks.at(sequenceLen)) != _cmpOperands.at(sequenceLen))
                    {
                        std::move(backIter, in.end(), _utf8Partials.begin());
                        remainingLength -= sequenceLen;
                        _partialsLen = sequenceLen;
                    }

                    break;
                }
            }
        }

        // give back the part of the string that contains complete code points only
        _buffer8.append(in, 0u, remainingLength);
        out = _buffer8;

        return S_OK;
    }
    catch (std::length_error&)
    {
        return E_ABORT;
    }
    catch (std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Method Description:
// - Discard cached partials.
// Arguments:
// - none
// Return Value:
// - void
void til::u8state::reset() noexcept
{
    _partialsLen = 0u;
}

til::u16state::u16state() noexcept :
    _buffer16{}
{
}

// Method Description:
// - Takes a UTF-16 string and populates it with *complete* UTF-16 codepoints.
//   If it receives an incomplete codepoint, it will cache it until it can be completed.
// Arguments:
// - in - UTF-16 string_view potentially containing partial code points
// - out - on return, populated with complete codepoints at the string end
// Return Value:
// - S_OK          - the resulting string doesn't end with a partial
// - S_FALSE       - the resulting string contains the previously cached partials only
// - E_OUTOFMEMORY - the method failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the processing was aborted
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u16state::operator()(const std::wstring_view in, std::wstring_view& out) noexcept
{
    try
    {
        size_t remainingLength{ in.length() };
        size_t capacity{};
        if (FAILED(SizeTAdd(remainingLength, _cached, &capacity)))
        {
            return E_ABORT;
        }

        _buffer16.clear();
        _buffer16.reserve(capacity);

        // copy UTF-8 code units that were remaining from the previousl call (if any)
        if (_cached != 0u)
        {
            _buffer16.push_back(_highSurrogate);
            _cached = 0u;
        }

        if (in.empty())
        {
            out = _buffer16;
            if (_buffer16.empty())
            {
                return S_OK;
            }

            return S_FALSE; // the high surrogate is given back
        }

        if (in.back() >= 0xD800u && in.back() <= 0xDBFFu) // range of high surrogates
        {
            _highSurrogate = in.back();
            --remainingLength;
            _cached = 1u;
        }
        else
        {
            _cached = 0u;
        }

        // give back the part of the string that contains complete code points only
        _buffer16.append(in, 0u, remainingLength);
        out = _buffer16;

        return S_OK;
    }
    catch (std::length_error&)
    {
        return E_ABORT;
    }
    catch (std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Method Description:
// - Discard cached partials.
// Arguments:
// - none
// Return Value:
// - void
void til::u16state::reset() noexcept
{
    _cached = 0u;
}

// Routine Description:
// - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - out - reference to the resulting UTF-16 string
// Return Value:
// - S_OK          - the conversion succeded
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u8u16(const std::string_view in, std::wstring& out) noexcept
{
    try
    {
        out.clear();

        if (in.empty())
        {
            return S_OK;
        }

        int lengthRequired{};
        RETURN_HR_IF(E_ABORT, FAILED(SizeTToInt(in.length(), &lengthRequired)));

        out.resize(in.length()); // avoid to call MultiByteToWideChar twice only to get the required size
        const int lengthOut = MultiByteToWideChar(65001u, 0ul, in.data(), lengthRequired, out.data(), lengthRequired);
        out.resize(gsl::narrow_cast<size_t>(lengthOut));

        return lengthOut == 0 ? E_UNEXPECTED : S_OK;
    }
    catch (std::length_error&)
    {
        return E_ABORT;
    }
    catch (std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Routine Description:
// - Takes a UTF-16 string and performs the conversion to UTF-8. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
// Arguments:
// - in - wstring_view of the UTF-16 string to be converted
// - out - reference to the resulting UTF-8 string
// Return Value:
// - S_OK          - the conversion succeded
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u16u8(const std::wstring_view in, std::string& out) noexcept
{
    try
    {
        out.clear();

        if (in.empty())
        {
            return S_OK;
        }

        int lengthIn{};
        int lengthRequired{};
        RETURN_HR_IF(E_ABORT, FAILED(SizeTToInt(in.length(), &lengthIn)) || FAILED(IntMult(lengthIn, 3, &lengthRequired)));

        out.resize(gsl::narrow_cast<size_t>(lengthRequired)); // avoid to call WideCharToMultiByte twice only to get the required size
        const int lengthOut = WideCharToMultiByte(65001u, 0ul, in.data(), lengthIn, out.data(), lengthRequired, nullptr, nullptr);
        out.resize(gsl::narrow_cast<size_t>(lengthOut));

        return lengthOut == 0 ? E_UNEXPECTED : S_OK;
    }
    catch (std::length_error&)
    {
        return E_ABORT;
    }
    catch (std::bad_alloc&)
    {
        return E_OUTOFMEMORY;
    }
    catch (...)
    {
        return E_UNEXPECTED;
    }
}

// Routine Description:
// - Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - out - reference to the resulting UTF-16 string
// - state - reference to a til::u8state class holding the status of the current partials handling
// Return Value:
// - S_OK          - the conversion succeded
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u8u16(const std::string_view in, std::wstring& out, til::u8state& state) noexcept
{
    std::string_view sv{};
    RETURN_IF_FAILED(state(in, sv));
    return til::u8u16(sv, out);
}

// Routine Description:
// - Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// - out - reference to the resulting UTF-8 string
// - state - reference to a til::u16state class holding the status of the current partials handling
// Return Value:
// - S_OK          - the conversion succeded without any change of the represented code points
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT til::u16u8(const std::wstring_view in, std::string& out, til::u16state& state) noexcept
{
    std::wstring_view sv{};
    RETURN_IF_FAILED(state(in, sv));
    return til::u16u8(sv, out);
}

// Routine Description:
// - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// Return Value:
// - the resulting UTF-16 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::wstring til::u8u16(const std::string_view in)
{
    std::wstring out{};
    THROW_IF_FAILED(til::u8u16(in, out));
    return out;
}

// Routine Description:
// - Takes a UTF-16 string and performs the conversion to UTF-8. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// Return Value:
// - the resulting UTF-8 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::string til::u16u8(const std::wstring_view in)
{
    std::string out{};
    THROW_IF_FAILED(til::u16u8(in, out));
    return out;
}

// Routine Description:
// Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - state - reference to a til::u8state class holding the status of the current partials handling
// Return Value:
// - the resulting UTF-16 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::wstring til::u8u16(const std::string_view in, til::u8state& state)
{
    std::wstring out{};
    THROW_IF_FAILED(til::u8u16(in, out, state));
    return out;
}

// Routine Description:
// Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// - state - reference to a til::u16state class holding the status of the current partials handling
// Return Value:
// - the resulting UTF-8 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::string til::u16u8(const std::wstring_view in, til::u16state& state)
{
    std::string out{};
    THROW_IF_FAILED(til::u16u8(in, out, state));
    return out;
}
