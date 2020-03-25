/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- u8u16convert.h

Abstract:
- Defines classes which hold the status of the current partials handling.
- Defines functions for converting between UTF-8 and UTF-16 strings.

Tests have been made in order to investigate whether or not own algorithms
could overcome disadvantages of syscalls. Test results can be read up
in PR #4093 and the test algorithms are available in src\tools\U8U16Test.
Based on the results the decision was made to keep using the platform
functions MultiByteToWideChar and WideCharToMultiByte.

Author(s):
- Steffen Illhardt (german-one) 2020
--*/

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    template<class charT>
    class u8u16state final
    {
    public:
        u8u16state() noexcept :
            _buffer{},
            _utfPartials{}
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
        template<class T = charT>
        [[nodiscard]] typename std::enable_if<std::is_same<T, char>::value, HRESULT>::type
        operator()(const std::basic_string_view<T> in, std::basic_string_view<T>& out) noexcept
        {
            try
            {
                size_t capacity{};
                RETURN_HR_IF(E_ABORT, !base::CheckAdd(in.length(), _partialsLen).AssignIfValid(&capacity));

                _buffer.clear();
                _buffer.reserve(capacity);

                // copy UTF-8 code units that were remaining from the previous call (if any)
                if (_partialsLen != 0u)
                {
                    _buffer.assign(_utfPartials.cbegin(), _utfPartials.cbegin() + _partialsLen);
                    _partialsLen = 0u;
                }

                if (in.empty())
                {
                    out = _buffer;
                    if (_buffer.empty())
                    {
                        return S_OK;
                    }

                    return S_FALSE; // the partial is populated
                }

                _buffer.append(in);
                size_t remainingLength{ _buffer.length() };

                auto backIter = _buffer.end();
                // If the last byte in the string was a byte belonging to a UTF-8 multi-byte character
                if ((*(backIter - 1) & _Utf8BitMasks::MaskAsciiByte) > _Utf8BitMasks::IsAsciiByte)
                {
                    // Check only up to 3 last bytes, if no Lead Byte was found then the byte before must be the Lead Byte and no partials are in the string
                    const size_t stopLen{ std::min(_buffer.length(), gsl::narrow_cast<size_t>(3u)) };
                    for (size_t sequenceLen{ 1u }; sequenceLen <= stopLen; ++sequenceLen)
                    {
                        --backIter;
                        // If Lead Byte found
                        if ((*backIter & _Utf8BitMasks::MaskContinuationByte) > _Utf8BitMasks::IsContinuationByte)
                        {
                            // If the Lead Byte indicates that the last bytes in the string is a partial UTF-8 code point then cache them:
                            //  Use the bitmask at index `sequenceLen`. Compare the result with the operand having the same index. If they
                            //  are not equal then the sequence has to be cached because it is a partial code point. Otherwise the
                            //  sequence is a complete UTF-8 code point and the whole string is ready for the conversion into a UTF-16 string.
                            if ((*backIter & _cmpMasks.at(sequenceLen)) != _cmpOperands.at(sequenceLen))
                            {
                                std::move(backIter, _buffer.end(), _utfPartials.begin());
                                remainingLength -= sequenceLen;
                                _partialsLen = sequenceLen;
                            }

                            break;
                        }
                    }
                }

                // populate the part of the string that contains complete code points only
                out = { _buffer.data(), remainingLength };

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
        template<class T = charT>
        [[nodiscard]] typename std::enable_if<std::is_same<T, wchar_t>::value, HRESULT>::type
        operator()(const std::basic_string_view<T> in, std::basic_string_view<T>& out) noexcept
        {
            try
            {
                size_t remainingLength{ in.length() };
                size_t capacity{};

                RETURN_HR_IF(E_ABORT, !base::CheckAdd(remainingLength, _partialsLen).AssignIfValid(&capacity));

                _buffer.clear();
                _buffer.reserve(capacity);

                // copy UTF-8 code units that were remaining from the previous call (if any)
                if (_partialsLen != 0u)
                {
                    _buffer.push_back(_utfPartials.front());
                    _partialsLen = 0u;
                }

                if (in.empty())
                {
                    out = _buffer;
                    if (_buffer.empty())
                    {
                        return S_OK;
                    }

                    return S_FALSE; // the high surrogate is populated
                }

                // cache the last value in the string if it is in the range of high surrogates
                if (in.back() >= 0xD800u && in.back() <= 0xDBFFu)
                {
                    _utfPartials.front() = in.back();
                    --remainingLength;
                    _partialsLen = 1u;
                }
                else
                {
                    _partialsLen = 0u;
                }

                // populate the part of the string that contains complete code points only
                _buffer.append(in, 0u, remainingLength);
                out = _buffer;

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
        void reset() noexcept
        {
            _partialsLen = 0u;
        }

    private:
        enum _Utf8BitMasks : BYTE
        {
            IsAsciiByte = 0b0'0000000, // Any byte representing an ASCII character has the MSB set to 0
            MaskAsciiByte = 0b1'0000000, // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsAsciiByte pattern
            IsContinuationByte = 0b10'000000, // Continuation bytes of any UTF-8 non-ASCII character have the MSB set to 1 and the adjacent bit set to 0
            MaskContinuationByte = 0b11'000000, // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsContinuationByte pattern
            IsLeadByteTwoByteSequence = 0b110'00000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of two bytes has the two highest bits set to 1 and the adjacent bit set to 0
            MaskLeadByteTwoByteSequence = 0b111'00000, // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteTwoByteSequence pattern
            IsLeadByteThreeByteSequence = 0b1110'0000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of three bytes has the three highest bits set to 1 and the adjacent bit set to 0
            MaskLeadByteThreeByteSequence = 0b1111'0000, // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteThreeByteSequence pattern
            IsLeadByteFourByteSequence = 0b11110'000, // A lead byte that indicates a UTF-8 non-ASCII character consisting of four bytes has the four highest bits set to 1 and the adjacent bit set to 0
            MaskLeadByteFourByteSequence = 0b11111'000 // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLeadByteFourByteSequence pattern
        };

        // array of bitmasks
        constexpr static std::array<BYTE, 4> _cmpMasks{
            0, // unused
            _Utf8BitMasks::MaskContinuationByte,
            _Utf8BitMasks::MaskLeadByteTwoByteSequence,
            _Utf8BitMasks::MaskLeadByteThreeByteSequence,
        };

        // array of values for the comparisons
        constexpr static std::array<BYTE, 4> _cmpOperands{
            0, // unused
            _Utf8BitMasks::IsAsciiByte, // intentionally conflicts with MaskContinuationByte
            _Utf8BitMasks::IsLeadByteTwoByteSequence,
            _Utf8BitMasks::IsLeadByteThreeByteSequence,
        };

        std::basic_string<charT> _buffer; // buffer to which the populated string_view refers
        std::array<charT, 4> _utfPartials; // buffer for code units of a partial code point that have to be cached
        size_t _partialsLen{}; // number of cached code units
    };

    // make clear what incoming string type the state is for
    typedef u8u16state<char> u8state;
    typedef u8u16state<wchar_t> u16state;

    // Routine Description:
    // - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
    // Arguments:
    // - in - UTF-8 string to be converted
    // - out - reference to the resulting UTF-16 string
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, char>::value && std::is_same<typename outT::value_type, wchar_t>::value, HRESULT>::type
    u8u16(const inT in, outT& out) noexcept
    {
        try
        {
            out.clear();

            if (in.empty())
            {
                return S_OK;
            }

            int lengthRequired{};
            // The worst ratio of UTF-8 code units to UTF-16 code units is 1 to 1 if UTF-8 consists of ASCII only.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthRequired));
            out.resize(in.length()); // avoid to call MultiByteToWideChar twice only to get the required size
            const int lengthOut = MultiByteToWideChar(gsl::narrow_cast<UINT>(CP_UTF8), 0ul, in.data(), lengthRequired, out.data(), lengthRequired);
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
    // - in - UTF-8 string to be converted
    // - out - reference to the resulting UTF-16 string
    // - state - reference to a til::u8state class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, char>::value && std::is_same<typename outT::value_type, wchar_t>::value, HRESULT>::type
    u8u16(const inT in, outT& out, u8state& state) noexcept
    {
        std::string_view sv{};
        RETURN_IF_FAILED(state(std::string_view{ in }, sv));
        return til::u8u16(sv, out);
    }

    // Routine Description:
    // - Takes a UTF-16 string and performs the conversion to UTF-8. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
    // Arguments:
    // - in - UTF-16 string to be converted
    // - out - reference to the resulting UTF-8 string
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value && std::is_same<typename outT::value_type, char>::value, HRESULT>::type
    u16u8(const inT in, outT& out) noexcept
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
            // Code Point U+0000..U+FFFF: 1 UTF-16 code unit --> 1..3 UTF-8 code units.
            // Code Points >U+FFFF: 2 UTF-16 code units --> 4 UTF-8 code units.
            // Thus, the worst ratio of UTF-16 code units to UTF-8 code units is 1 to 3.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthIn) || !base::CheckMul(lengthIn, 3).AssignIfValid(&lengthRequired));
            out.resize(gsl::narrow_cast<size_t>(lengthRequired)); // avoid to call WideCharToMultiByte twice only to get the required size
            const int lengthOut = WideCharToMultiByte(gsl::narrow_cast<UINT>(CP_UTF8), 0ul, in.data(), lengthIn, out.data(), lengthRequired, nullptr, nullptr);
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
    // - Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
    // Arguments:
    // - in - UTF-16 string to be converted
    // - out - reference to the resulting UTF-8 string
    // - state - reference to a til::u16state class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded without any change of the represented code points
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value && std::is_same<typename outT::value_type, char>::value, HRESULT>::type
    u16u8(const inT in, outT& out, u16state& state) noexcept
    {
        std::wstring_view sv{};
        RETURN_IF_FAILED(state(std::wstring_view{ in }, sv));
        return u16u8(sv, out);
    }

    // Routine Description:
    // - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
    // Arguments:
    // - in - UTF-8 string to be converted
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, char>::value, std::wstring>::type
    u8u16(const inT in)
    {
        std::wstring out{};
        THROW_IF_FAILED(u8u16(std::string_view{ in }, out));
        return out;
    }

    // Routine Description:
    // Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
    // Arguments:
    // - in - UTF-8 string to be converted
    // - state - reference to a til::u8state class holding the status of the current partials handling
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, char>::value, std::wstring>::type
    u8u16(const inT in, u8state& state)
    {
        std::wstring out{};
        THROW_IF_FAILED(u8u16(std::string_view{ in }, out, state));
        return out;
    }

    // Routine Description:
    // - Takes a UTF-16 string and performs the conversion to UTF-8. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
    // Arguments:
    // - in - UTF-16 string to be converted
    // Return Value:
    // - the resulting UTF-8 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value, std::string>::type
    u16u8(const inT in)
    {
        std::string out{};
        THROW_IF_FAILED(u16u8(std::wstring_view{ in }, out));
        return out;
    }

    // Routine Description:
    // Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
    // Arguments:
    // - in - UTF-16 string to be converted
    // - state - reference to a til::u16state class holding the status of the current partials handling
    // Return Value:
    // - the resulting UTF-8 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value, std::string>::type
    u16u8(const inT in, u16state& state)
    {
        std::string out{};
        THROW_IF_FAILED(u16u8(std::wstring_view{ in }, out, state));
        return out;
    }
}
