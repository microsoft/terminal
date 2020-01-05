// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "inc/Utf8Utf16Convert.hpp"

u8state::u8state() noexcept :
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
[[nodiscard]] HRESULT u8state::operator()(const std::string_view in, std::string_view& out) noexcept
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
void u8state::reset() noexcept
{
    _partialsLen = 0u;
}

u16state::u16state() noexcept :
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
[[nodiscard]] HRESULT u16state::operator()(const std::wstring_view in, std::wstring_view& out) noexcept
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
void u16state::reset() noexcept
{
    _cached = 0u;
}

// Routine Description:
// - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - out - reference to the resulting UTF-16 string
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - S_OK          - the conversion succeded without any change of the represented code points
// - S_FALSE       - the incoming string contained at least one invalid character
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT u8u16(const std::string_view in, std::wstring& out, bool discardInvalids) noexcept
{
    constexpr const uint8_t contBegin{ 0x80u }; // usual begin of the range of continuation Bytes
    constexpr const uint8_t contEnd{ 0xBfu }; // usual end of the range of continuation Bytes
    constexpr const uint32_t unicodeReplacementChar{ 0xFFFDu }; // Unicode Replacement Character

    try
    {
        HRESULT hRes{ S_OK };
        out.clear();

        if (in.empty())
        {
            return hRes;
        }

        out.reserve(in.length()); // avoid any further re-allocations and copying

        const auto end8{ in.cend() };
        for (auto it8{ in.cbegin() }; it8 < end8;)
        {
            uint32_t codePoint{ unicodeReplacementChar }; // default

            // *** convert UTF-8 to a code point ***
            // valid single bytes
            // - 00..7F
            if (gsl::narrow_cast<uint8_t>(*it8) <= 0x7Fu)
            {
                codePoint = gsl::narrow_cast<uint8_t>(*it8++);
            }
            // valid two bytes
            // - C2..DF | 80..BF (first byte 0xC0 and 0xC1 invalid)
            else if (gsl::narrow_cast<uint8_t>(*it8) >= 0xC2u && gsl::narrow_cast<uint8_t>(*it8) <= 0xDFu)
            {
                size_t cnt{ 1u };
                if ((it8 + 1) < end8 && gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd)
                {
                    ++cnt;
                    codePoint = ((gsl::narrow_cast<uint8_t>(*it8) ^ 0x000000C0u) << 6u) |
                                (gsl::narrow_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u);
                }
                else
                {
                    hRes = S_FALSE;
                }

                it8 += cnt;
            }
            // valid three bytes
            // - E0     | A0..BF | 80..BF
            // - E1..EC | 80..BF | 80..BF
            // - ED     | 80..9F | 80..BF
            // - EE..EF | 80..BF | 80..BF
            else if (gsl::narrow_cast<uint8_t>(*it8) >= 0xE0u && gsl::narrow_cast<uint8_t>(*it8) <= 0xEFu)
            {
                size_t cnt{ 1u };
                if ((it8 + 1) < end8 &&
                    (( // E0     | *A0*..BF
                         gsl::narrow_cast<uint8_t>(*it8) == 0xE0u &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= 0xA0u && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                     ( // E1..EC | 80..BF
                         gsl::narrow_cast<uint8_t>(*it8) >= 0xE1u && gsl::narrow_cast<uint8_t>(*it8) <= 0xECu &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                     ( // ED     | 80..*9F*
                         gsl::narrow_cast<uint8_t>(*it8) == 0xEDu &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= 0x9Fu) ||
                     ( // EE..EF | 80..BF
                         gsl::narrow_cast<uint8_t>(*it8) >= 0xEEu && gsl::narrow_cast<uint8_t>(*it8) <= 0xEFu &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd)))
                {
                    ++cnt;
                    if ((it8 + 2) < end8 && gsl::narrow_cast<uint8_t>(*(it8 + 2)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                    {
                        ++cnt;
                        codePoint = ((gsl::narrow_cast<uint8_t>(*it8) ^ 0x000000E0u) << 12u) |
                                    ((gsl::narrow_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 6u) |
                                    (gsl::narrow_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u);
                    }
                }

                it8 += cnt;
                if (cnt < 3u)
                {
                    hRes = S_FALSE;
                }
            }
            // valid four bytes
            // - F0     | 90..BF | 80..BF | 80..BF
            // - F1..F3 | 80..BF | 80..BF | 80..BF
            // - F4     | 80..8F | 80..BF | 80..BF
            else if (gsl::narrow_cast<uint8_t>(*it8) >= 0xF0u && gsl::narrow_cast<uint8_t>(*it8) <= 0xF4u)
            {
                size_t cnt{ 1u };
                if ((it8 + 1) < end8 &&
                    (( // F0     | *90*..BF
                         gsl::narrow_cast<uint8_t>(*it8) == 0xF0u &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= 0x90u && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                     ( // F1..F3 | 80..BF
                         gsl::narrow_cast<uint8_t>(*it8) >= 0xF1u && gsl::narrow_cast<uint8_t>(*it8) <= 0xF3u &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                     ( // F4     | 80..*8F*
                         gsl::narrow_cast<uint8_t>(*it8) == 0xF4u &&
                         gsl::narrow_cast<uint8_t>(*(it8 + 1)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 1)) <= 0x8Fu)))
                {
                    ++cnt;
                    if ((it8 + 2) < end8 && gsl::narrow_cast<uint8_t>(*(it8 + 2)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                    {
                        ++cnt;
                        if ((it8 + 3) < end8 && gsl::narrow_cast<uint8_t>(*(it8 + 3)) >= contBegin && gsl::narrow_cast<uint8_t>(*(it8 + 3)) <= contEnd)
                        {
                            ++cnt;
                            codePoint = ((gsl::narrow_cast<uint8_t>(*it8) ^ 0x000000F0u) << 18u) |
                                        ((gsl::narrow_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 12u) |
                                        ((gsl::narrow_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u) << 6u) |
                                        (gsl::narrow_cast<uint8_t>(*(it8 + 3)) ^ 0x00000080u);
                        }
                    }
                }

                it8 += cnt;
                if (cnt < 4u)
                {
                    hRes = S_FALSE;
                }
            }
            else
            {
                hRes = S_FALSE;
                ++it8;
            }

            // *** convert the code point to UTF-16 ***
            if (codePoint != unicodeReplacementChar || discardInvalids == false)
            {
                if (codePoint < 0x00010000u)
                {
                    out.push_back(gsl::narrow_cast<wchar_t>(codePoint));
                }
                else
                {
                    codePoint -= 0x00010000u;
                    out.push_back(gsl::narrow_cast<wchar_t>(0x0000D800u + ((codePoint >> 10u) & 0x000003FFu)));
                    out.push_back(gsl::narrow_cast<wchar_t>(0x0000DC00u + (codePoint & 0x000003FFu)));
                }
            }
        }

        return hRes;
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
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - S_OK          - the conversion succeded without any change of the represented code points
// - S_FALSE       - the incoming string contained at least one invalid character
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT u16u8(const std::wstring_view in, std::string& out, bool discardInvalids) noexcept
{
    constexpr const uint32_t unicodeReplacementChar{ 0xFFFDu };

    try
    {
        HRESULT hRes{ S_OK };
        out.clear();

        if (in.empty())
        {
            return hRes;
        }

        size_t lengthHint{};
        if (FAILED(SizeTMult(in.length(), gsl::narrow_cast<size_t>(3u), &lengthHint)))
        {
            lengthHint = std::max(out.capacity(), in.length());
        }

        out.reserve(lengthHint); // avoid any further re-allocations and copying

        const auto end16{ in.cend() };
        for (auto it16{ in.cbegin() }; it16 < end16;)
        {
            uint32_t codePoint{ unicodeReplacementChar }; // default

            // *** convert UTF-16 to a code point ***
            if (*it16 >= 0xD800u && *it16 <= 0xDBFFu) // range of high surrogates
            {
                const uint32_t high{ *it16++ };
                if (it16 < end16 && *it16 >= 0xDC00u && *it16 <= 0xDFFFu) // range of low surrogates
                {
                    codePoint = (high << 10u) + *it16++ - gsl::narrow_cast<uint32_t>(0x035FDC00u);
                }
                else
                {
                    hRes = S_FALSE;
                }
            }
            else if (*it16 >= 0xDC00u && *it16 <= 0xDFFFu) // standing alone low surrogates are invalid
            {
                hRes = S_FALSE;
                ++it16;
            }
            else
            {
                codePoint = *it16++;
            }

            // *** convert the code point to UTF-8 ***
            if (codePoint != unicodeReplacementChar || discardInvalids == false)
            {
                // the outcome of performance tests is that subsequent calls of push_back
                // perform much better than appending a single initializer_list
                if (codePoint < 0x00000080u)
                {
                    out.push_back(gsl::narrow_cast<char>(codePoint));
                }
                else if (codePoint < 0x00000800u)
                {
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 6u & 0x1Fu) | 0xC0u));
                    out.push_back(gsl::narrow_cast<char>((codePoint & 0x3Fu) | 0x80u));
                }
                else if (codePoint < 0x00010000u)
                {
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 12u & 0x0Fu) | 0xE0u));
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                    out.push_back(gsl::narrow_cast<char>((codePoint & 0x3Fu) | 0x80u));
                }
                else
                {
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 18u & 0x07u) | 0xF0u));
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 12u & 0x3Fu) | 0x80u));
                    out.push_back(gsl::narrow_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                    out.push_back(gsl::narrow_cast<char>((codePoint & 0x3Fu) | 0x80u));
                }
            }
        }

        return hRes;
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
// - state - reference to a u8state class holding the status of the current partials handling
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - S_OK          - the conversion succeded without any change of the represented code points
// - S_FALSE       - the incoming string contained at least one invalid character
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT u8u16(const std::string_view in, std::wstring& out, u8state& state, bool discardInvalids) noexcept
{
    std::string_view sv{};
    RETURN_IF_FAILED(state(in, sv));
    return u8u16(sv, out, discardInvalids);
}

// Routine Description:
// - Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// - out - reference to the resulting UTF-8 string
// - state - reference to a u16state class holding the status of the current partials handling
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - S_OK          - the conversion succeded without any change of the represented code points
// - S_FALSE       - the incoming string contained at least one invalid character
// - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
// - E_ABORT       - the resulting string length would exceed the max_size and thus, the conversion was aborted before the conversion has been completed
// - E_UNEXPECTED  - an unexpected error occurred
[[nodiscard]] HRESULT u16u8(const std::wstring_view in, std::string& out, u16state& state, bool discardInvalids) noexcept
{
    std::wstring_view sv{};
    RETURN_IF_FAILED(state(in, sv));
    return u16u8(sv, out, discardInvalids);
}

// Routine Description:
// - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - the resulting UTF-16 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::wstring u8u16(const std::string_view in, bool discardInvalids)
{
    std::wstring out{};
    THROW_IF_FAILED(u8u16(in, out, discardInvalids));
    return out;
}

// Routine Description:
// - Takes a UTF-16 string and performs the conversion to UTF-8. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - the resulting UTF-8 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::string u16u8(const std::wstring_view in, bool discardInvalids)
{
    std::string out{};
    THROW_IF_FAILED(u16u8(in, out, discardInvalids));
    return out;
}

// Routine Description:
// Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
// Arguments:
// - in - string_view of the UTF-8 string to be converted
// - state - reference to a u8state class holding the status of the current partials handling
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - the resulting UTF-16 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::wstring u8u16(const std::string_view in, u8state& state, bool discardInvalids)
{
    std::wstring out{};
    THROW_IF_FAILED(u8u16(in, out, state, discardInvalids));
    return out;
}

// Routine Description:
// Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
// Arguments:
// - in - string_view of the UTF-16 string to be converted
// - state - reference to a u16state class holding the status of the current partials handling
// - discardInvalids - if `true` invalid characters are discarded instead of replaced with U+FFFD, default is `false`
// Return Value:
// - the resulting UTF-8 string
// - NOTE: Throws HRESULT errors that the non-throwing sibling returns
std::string u16u8(const std::wstring_view in, u16state& state, bool discardInvalids)
{
    std::string out{};
    THROW_IF_FAILED(u16u8(in, out, state, discardInvalids));
    return out;
}
