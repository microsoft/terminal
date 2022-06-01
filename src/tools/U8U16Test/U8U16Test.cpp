// TEST TOOL U8U16Test
// Performance tests for UTF-8 <--> UTF-16 conversions, related to PR #4093
// NOTE The functions u8u16 and u16u8 contain own algorithms. Tests have shown that they perform
// worse than the platform API functions.
// Thus, these functions are *unrelated* to the til::u8u16 and til::u16u8 implementation.

#include "U8U16Test.hpp"

u8state::u8state() noexcept :
    _buffer8{},
    _utf8Partials{}
{
}

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

        // copy UTF-8 code units that were remaining from the previous call (if any)
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
            const size_t stopLen{ std::min(in.length(), static_cast<size_t>(4u)) };
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

void u8state::reset() noexcept
{
    _partialsLen = 0u;
}

u16state::u16state() noexcept :
    _buffer16{}
{
}

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

        // copy UTF-8 code units that were remaining from the previous call (if any)
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

void u16state::reset() noexcept
{
    _cached = 0u;
}

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
            // *** convert ASCII directly to UTF-16 ***
            // valid single bytes
            // - 00..7F
            if (static_cast<uint8_t>(*it8) <= 0x7Fu)
            {
                out.push_back(static_cast<wchar_t>(*it8++));
            }
            else
            {
                uint32_t codePoint{ unicodeReplacementChar }; // default

                // valid two bytes
                // - C2..DF | 80..BF (first byte 0xC0 and 0xC1 invalid)
                if (static_cast<uint8_t>(*it8) >= 0xC2u && static_cast<uint8_t>(*it8) <= 0xDFu)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 && static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd)
                    {
                        ++cnt;
                        codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000C0u) << 6u) |
                                    (static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u);
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
                else if (static_cast<uint8_t>(*it8) >= 0xE0u && static_cast<uint8_t>(*it8) <= 0xEFu)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 &&
                        (( // E0     | *A0*..BF
                             static_cast<uint8_t>(*it8) == 0xE0u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= 0xA0u && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // E1..EC | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xE1u && static_cast<uint8_t>(*it8) <= 0xECu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // ED     | 80..*9F*
                             static_cast<uint8_t>(*it8) == 0xEDu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= 0x9Fu) ||
                         ( // EE..EF | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xEEu && static_cast<uint8_t>(*it8) <= 0xEFu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd)))
                    {
                        ++cnt;
                        if ((it8 + 2) < end8 && static_cast<uint8_t>(*(it8 + 2)) >= contBegin && static_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                        {
                            ++cnt;
                            codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000E0u) << 12u) |
                                        ((static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 6u) |
                                        (static_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u);
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
                else if (static_cast<uint8_t>(*it8) >= 0xF0u && static_cast<uint8_t>(*it8) <= 0xF4u)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 &&
                        (( // F0     | *90*..BF
                             static_cast<uint8_t>(*it8) == 0xF0u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= 0x90u && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // F1..F3 | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xF1u && static_cast<uint8_t>(*it8) <= 0xF3u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // F4     | 80..*8F*
                             static_cast<uint8_t>(*it8) == 0xF4u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= 0x8Fu)))
                    {
                        ++cnt;
                        if ((it8 + 2) < end8 && static_cast<uint8_t>(*(it8 + 2)) >= contBegin && static_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                        {
                            ++cnt;
                            if ((it8 + 3) < end8 && static_cast<uint8_t>(*(it8 + 3)) >= contBegin && static_cast<uint8_t>(*(it8 + 3)) <= contEnd)
                            {
                                ++cnt;
                                codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000F0u) << 18u) |
                                            ((static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 12u) |
                                            ((static_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u) << 6u) |
                                            (static_cast<uint8_t>(*(it8 + 3)) ^ 0x00000080u);
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
                if (codePoint != unicodeReplacementChar || !discardInvalids)
                {
                    if (codePoint < 0x00010000u)
                    {
                        out.push_back(static_cast<wchar_t>(codePoint));
                    }
                    else
                    {
                        codePoint -= 0x00010000u;
                        out.push_back(static_cast<wchar_t>(0x0000D800u + ((codePoint >> 10u) & 0x000003FFu)));
                        out.push_back(static_cast<wchar_t>(0x0000DC00u + (codePoint & 0x000003FFu)));
                    }
                }
            }
        }

        // out.shrink_to_fit();
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

[[nodiscard]] HRESULT u8u16_ptr(const std::string_view in, std::wstring& out, bool discardInvalids) noexcept
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

        out.resize(in.length()); // avoid any further re-allocations and copying

        wchar_t* it16{ out.data() };
        const auto end8{ in.cend() };
        for (auto it8{ in.cbegin() }; it8 < end8;)
        {
            // *** convert ASCII directly to UTF-16 ***
            // valid single bytes
            // - 00..7F
            if (static_cast<uint8_t>(*it8) <= 0x7Fu)
            {
                *it16++ = (static_cast<wchar_t>(*it8++));
            }
            else
            {
                uint32_t codePoint{ unicodeReplacementChar }; // default

                // valid two bytes
                // - C2..DF | 80..BF (first byte 0xC0 and 0xC1 invalid)
                if (static_cast<uint8_t>(*it8) >= 0xC2u && static_cast<uint8_t>(*it8) <= 0xDFu)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 && static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd)
                    {
                        ++cnt;
                        codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000C0u) << 6u) |
                                    (static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u);
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
                else if (static_cast<uint8_t>(*it8) >= 0xE0u && static_cast<uint8_t>(*it8) <= 0xEFu)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 &&
                        (( // E0     | *A0*..BF
                             static_cast<uint8_t>(*it8) == 0xE0u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= 0xA0u && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // E1..EC | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xE1u && static_cast<uint8_t>(*it8) <= 0xECu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // ED     | 80..*9F*
                             static_cast<uint8_t>(*it8) == 0xEDu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= 0x9Fu) ||
                         ( // EE..EF | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xEEu && static_cast<uint8_t>(*it8) <= 0xEFu &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd)))
                    {
                        ++cnt;
                        if ((it8 + 2) < end8 && static_cast<uint8_t>(*(it8 + 2)) >= contBegin && static_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                        {
                            ++cnt;
                            codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000E0u) << 12u) |
                                        ((static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 6u) |
                                        (static_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u);
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
                else if (static_cast<uint8_t>(*it8) >= 0xF0u && static_cast<uint8_t>(*it8) <= 0xF4u)
                {
                    size_t cnt{ 1u };
                    if ((it8 + 1) < end8 &&
                        (( // F0     | *90*..BF
                             static_cast<uint8_t>(*it8) == 0xF0u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= 0x90u && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // F1..F3 | 80..BF
                             static_cast<uint8_t>(*it8) >= 0xF1u && static_cast<uint8_t>(*it8) <= 0xF3u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= contEnd) ||
                         ( // F4     | 80..*8F*
                             static_cast<uint8_t>(*it8) == 0xF4u &&
                             static_cast<uint8_t>(*(it8 + 1)) >= contBegin && static_cast<uint8_t>(*(it8 + 1)) <= 0x8Fu)))
                    {
                        ++cnt;
                        if ((it8 + 2) < end8 && static_cast<uint8_t>(*(it8 + 2)) >= contBegin && static_cast<uint8_t>(*(it8 + 2)) <= contEnd)
                        {
                            ++cnt;
                            if ((it8 + 3) < end8 && static_cast<uint8_t>(*(it8 + 3)) >= contBegin && static_cast<uint8_t>(*(it8 + 3)) <= contEnd)
                            {
                                ++cnt;
                                codePoint = ((static_cast<uint8_t>(*it8) ^ 0x000000F0u) << 18u) |
                                            ((static_cast<uint8_t>(*(it8 + 1)) ^ 0x00000080u) << 12u) |
                                            ((static_cast<uint8_t>(*(it8 + 2)) ^ 0x00000080u) << 6u) |
                                            (static_cast<uint8_t>(*(it8 + 3)) ^ 0x00000080u);
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
                if (codePoint != unicodeReplacementChar || !discardInvalids)
                {
                    if (codePoint < 0x00010000u)
                    {
                        *it16++ = (static_cast<wchar_t>(codePoint));
                    }
                    else
                    {
                        codePoint -= 0x00010000u;
                        *it16++ = (static_cast<wchar_t>(0x0000D800u + ((codePoint >> 10u) & 0x000003FFu)));
                        *it16++ = (static_cast<wchar_t>(0x0000DC00u + (codePoint & 0x000003FFu)));
                    }
                }
            }
        }

        out.resize(static_cast<size_t>(it16 - out.data()));
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
        if (FAILED(SizeTMult(in.length(), static_cast<size_t>(3u), &lengthHint)))
        {
            lengthHint = std::max(out.capacity(), in.length());
        }

        out.reserve(lengthHint); // avoid any further re-allocations and copying

        const auto end16{ in.cend() };
        for (auto it16{ in.cbegin() }; it16 < end16;)
        {
            // *** convert ASCII directly to UTF-8 ***
            if (*it16 <= 0x007Fu)
            {
                out.push_back(static_cast<char>(*it16++));
            }
            else
            {
                uint32_t codePoint{ unicodeReplacementChar }; // default

                // *** convert UTF-16 to a code point ***
                if (*it16 >= 0xD800u && *it16 <= 0xDBFFu) // range of high surrogates
                {
                    const uint32_t high{ *it16++ };
                    if (it16 < end16 && *it16 >= 0xDC00u && *it16 <= 0xDFFFu) // range of low surrogates
                    {
                        codePoint = (high << 10u) + *it16++ - static_cast<uint32_t>(0x035FDC00u);
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
                if (codePoint != unicodeReplacementChar || !discardInvalids)
                {
                    // the outcome of performance tests is that subsequent calls of push_back
                    // perform much better than appending a single initializer_list
                    if (codePoint < 0x00000800u)
                    {
                        out.push_back(static_cast<char>((codePoint >> 6u & 0x1Fu) | 0xC0u));
                        out.push_back(static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                    else if (codePoint < 0x00010000u)
                    {
                        out.push_back(static_cast<char>((codePoint >> 12u & 0x0Fu) | 0xE0u));
                        out.push_back(static_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                        out.push_back(static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                    else
                    {
                        out.push_back(static_cast<char>((codePoint >> 18u & 0x07u) | 0xF0u));
                        out.push_back(static_cast<char>((codePoint >> 12u & 0x3Fu) | 0x80u));
                        out.push_back(static_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                        out.push_back(static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                }
            }
        }

        // out.shrink_to_fit();
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

[[nodiscard]] HRESULT u16u8_ptr(const std::wstring_view in, std::string& out, bool discardInvalids) noexcept
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
        if (FAILED(SizeTMult(in.length(), static_cast<size_t>(3u), &lengthHint)))
        {
            return E_ABORT;
        }

        out.resize(lengthHint); // avoid any further re-allocations and copying

        char* it8{ out.data() };
        const auto end16{ in.cend() };
        for (auto it16{ in.cbegin() }; it16 < end16;)
        {
            // *** convert ASCII directly to UTF-8 ***
            if (*it16 <= 0x007Fu)
            {
                *it8++ = (static_cast<char>(*it16++));
            }
            else
            {
                uint32_t codePoint{ unicodeReplacementChar }; // default

                // *** convert UTF-16 to a code point ***
                if (*it16 >= 0xD800u && *it16 <= 0xDBFFu) // range of high surrogates
                {
                    const uint32_t high{ *it16++ };
                    if (it16 < end16 && *it16 >= 0xDC00u && *it16 <= 0xDFFFu) // range of low surrogates
                    {
                        codePoint = (high << 10u) + *it16++ - static_cast<uint32_t>(0x035FDC00u);
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
                if (codePoint != unicodeReplacementChar || !discardInvalids)
                {
                    // the outcome of further performance tests is that using pointers
                    // perform even better than subsequent calls of push_back
                    if (codePoint < 0x00000800u)
                    {
                        *it8++ = (static_cast<char>((codePoint >> 6u & 0x1Fu) | 0xC0u));
                        *it8++ = (static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                    else if (codePoint < 0x00010000u)
                    {
                        *it8++ = (static_cast<char>((codePoint >> 12u & 0x0Fu) | 0xE0u));
                        *it8++ = (static_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                        *it8++ = (static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                    else
                    {
                        *it8++ = (static_cast<char>((codePoint >> 18u & 0x07u) | 0xF0u));
                        *it8++ = (static_cast<char>((codePoint >> 12u & 0x3Fu) | 0x80u));
                        *it8++ = (static_cast<char>((codePoint >> 6u & 0x3Fu) | 0x80u));
                        *it8++ = (static_cast<char>((codePoint & 0x3Fu) | 0x80u));
                    }
                }
            }
        }

        out.resize(static_cast<size_t>(it8 - out.data()));
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

[[nodiscard]] HRESULT u8u16(const std::string_view in, std::wstring& out, u8state& state, bool discardInvalids) noexcept
{
    std::string_view sv{};
    //RETURN_IF_FAILED(state(in, sv));
    const HRESULT hRes{ state(in, sv) };
    if (FAILED(hRes))
    {
        return hRes;
    }
    return u8u16(sv, out, discardInvalids);
}

[[nodiscard]] HRESULT u16u8(const std::wstring_view in, std::string& out, u16state& state, bool discardInvalids) noexcept
{
    std::wstring_view sv{};
    //RETURN_IF_FAILED(state(in, sv));
    const HRESULT hRes{ state(in, sv) };
    if (FAILED(hRes))
    {
        return hRes;
    }
    return u16u8(sv, out, discardInvalids);
}

std::wstring u8u16(const std::string_view in, bool discardInvalids)
{
    std::wstring out{};
    //THROW_IF_FAILED(u8u16(in, out, discardInvalids));
    const HRESULT hRes{ u8u16(in, out, discardInvalids) };
    if (FAILED(hRes))
    {
        throw std::runtime_error("error");
    }
    return out;
}

std::string u16u8(const std::wstring_view in, bool discardInvalids)
{
    std::string out{};
    //THROW_IF_FAILED(u16u8(in, out, discardInvalids));
    const HRESULT hRes{ u16u8(in, out, discardInvalids) };
    if (FAILED(hRes))
    {
        throw std::runtime_error("error");
    }
    return out;
}

std::wstring u8u16(const std::string_view in, u8state& state, bool discardInvalids)
{
    std::wstring out{};
    //THROW_IF_FAILED(u8u16(in, out, state, discardInvalids));
    const HRESULT hRes{ u8u16(in, out, state, discardInvalids) };
    if (FAILED(hRes))
    {
        throw std::runtime_error("error");
    }
    return out;
}

std::string u16u8(const std::wstring_view in, u16state& state, bool discardInvalids)
{
    std::string out{};
    //THROW_IF_FAILED(u16u8(in, out, state, discardInvalids));
    const HRESULT hRes{ u16u8(in, out, state, discardInvalids) };
    if (FAILED(hRes))
    {
        throw std::runtime_error("error");
    }
    return out;
}
