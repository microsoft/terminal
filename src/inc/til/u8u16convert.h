/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- u8u16convert.h

Abstract:
- Defines classes which hold the status of the current partials handling.
- Defines functions for converting between ANSI or UTF-8, and UTF-16 strings.

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
    class au16state final
    {
    public:
        au16state() noexcept :
            _codepage{ std::numeric_limits<unsigned int>::max() }, // since 0 is an alias for the default ANSI codepage, we use an invalid codepage id
            _cpInfo{},
            _buffer{},
            _partials{}
        {
        }

        // Method Description:
        // - Takes an ANSI or UTF-8 string and populates it with *complete* code points.
        //   If it receives an incomplete code point, it will cache it until it can be completed.
        //   NOTE: The caching of incomplete code points is not supported if the codepage id is either of
        //         50220, 50221, 50222, 50225, 50227, 50229, 52936, 57002, 57003, 57004, 57005, 57006, 57007, 57008, 57009, 57010, 57011, 65000.
        //         In these cases incomplete code points are silently replaced with the default replacement
        //         character in the involved conversion functions, and the conversion is treated as successful.
        // Arguments:
        // - codepage - id of the codepage the received string is encoded
        // - in - ANSI or UTF-8 string_view potentially containing partial code points
        // - out - on return, populated with complete code points at the string end
        // Return Value:
        // - S_OK          - the resulting string doesn't end with a partial, or caching is not supported (see above)
        // - S_FALSE       - the resulting string contains the previously cached partials only
        // - E_INVALIDARG  - the codepage id is invalid
        // - E_OUTOFMEMORY - the method failed to allocate memory for the resulting string
        // - E_ABORT       - the resulting string length would exceed the max_size and thus, the processing was aborted
        // - E_UNEXPECTED  - an unexpected error occurred
        template<class T = charT>
        [[nodiscard]] typename std::enable_if<std::is_same<T, char>::value, HRESULT>::type
        operator()(const unsigned int codepage, const std::basic_string_view<T> in, std::basic_string_view<T>& out) noexcept
        {
            try
            {
                // discard partials if the codepage changed
                if (_codepage != codepage)
                {
                    _partialsLen = 0u;
                    _codepage = codepage;
                    // update _cpInfo if necessary
                    if (codepage != gsl::narrow_cast<unsigned int>(CP_UTF8) && !GetCPInfo(codepage, &_cpInfo))
                    {
                        return E_INVALIDARG;
                    }
                }

                size_t capacity{};
                RETURN_HR_IF(E_ABORT, !base::CheckAdd(in.length(), _partialsLen).AssignIfValid(&capacity));

                _buffer.clear();
                _buffer.reserve(capacity);

                // copy partials that were remaining from the previous call (if any)
                if (_partialsLen != 0u)
                {
                    _buffer.assign(_partials.cbegin(), _partials.cbegin() + _partialsLen);
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

                if (codepage == gsl::narrow_cast<unsigned int>(CP_UTF8)) // UTF-8
                {
                    auto backIter{ _buffer.end() };
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
                                //  sequence is a complete UTF-8 code point and the whole string is ready for the conversion to wstring.
                                if ((*backIter & _cmpMasks.at(sequenceLen)) != _cmpOperands.at(sequenceLen))
                                {
                                    std::move(backIter, _buffer.end(), _partials.begin());
                                    remainingLength -= sequenceLen;
                                    _partialsLen = sequenceLen;
                                }

                                break;
                            }
                        }
                    }
                }
                else // ANSI codepages
                {
                    if (_cpInfo.MaxCharSize == 2u && !!_cpInfo.LeadByte[0]) // DBCS codepages
                    {
                        auto foundLeadByte{ false };

                        // Start at the beginning of the string (or the second byte if a lead byte has been cached in the previous call) and scan forward,
                        // keep track when it encounters a lead byte, and treat the next byte as the trailing part of the same character.
                        std::for_each(_buffer.cbegin(), _buffer.cend(), [&](const auto& ch) {
                            if (foundLeadByte)
                            {
                                foundLeadByte = false; // because it's the trailing part
                            }
                            else
                            {
                                // We use our own algorithm because IsDBCSLeadByteEx() supports only a subset of DBCS codepages.
                                const auto uCh{ gsl::narrow_cast<byte>(ch) };
                                for (ptrdiff_t idx{}; til::at(&(*_cpInfo.LeadByte), idx) != 0; idx += 2) // OK because the LeadByte array is guaranteed to end with two 0 bytes.
                                {
                                    if (uCh >= til::at(&(*_cpInfo.LeadByte), idx) && uCh <= til::at(&(*_cpInfo.LeadByte), idx + 1))
                                    {
                                        foundLeadByte = true;
                                        break;
                                    }
                                }
                            }
                        });

                        if (foundLeadByte)
                        {
                            _partials.front() = _buffer.back();
                            --remainingLength;
                            _partialsLen = 1u;
                        }
                        else
                        {
                            _partialsLen = 0u;
                        }
                    }
                    else if (codepage == 54936u) // GB 18030
                    {
                        auto foundLeadByte{ false };
                        size_t left{};
                        auto sequence{ gsl::narrow_cast<size_t>(1u) };
                        std::for_each(_buffer.cbegin(), _buffer.cend(), [&](const auto& ch) {
                            if (left > 0u && !foundLeadByte) // skip 3rd and 4th byte of a 4-byte sequence
                            {
                                --left;
                            }
                            else
                            {
                                const auto uCh{ gsl::narrow_cast<byte>(ch) };
                                if (foundLeadByte) // check the second byte after a lead byte was found
                                {
                                    foundLeadByte = false;
                                    if (uCh < byte{ 0x40 }) // if the second byte is < 0x40 then the sequence is 4 bytes long
                                    {
                                        left = 2u;
                                        sequence = 4u;
                                    }
                                    else // the sequence was 2 bytes long and we read them all
                                    {
                                        left = 0u;
                                    }
                                }
                                else if (uCh > byte{ 0x80 }) // if the first byte is > 0x80 then it indicates the begin of a 2- or 4-byte sequence
                                {
                                    foundLeadByte = true;
                                    left = 1u;
                                    sequence = 2u;
                                }
                                else // single byte
                                {
                                    foundLeadByte = false;
                                    left = 0u;
                                    sequence = 1u;
                                }
                            }
                        });

                        if (left == 0u)
                        {
                            _partialsLen = 0u;
                        }
                        else
                        {
                            _partialsLen = sequence - left;
                            remainingLength -= _partialsLen;
                            std::move(_buffer.end() - _partialsLen, _buffer.end(), _partials.begin());
                        }
                    }
                    else if (codepage >= 55000u && codepage <= 55004u) // GSM 7-bit
                    {
                        if (_buffer.back() == '\x1B') // escape character for a 2-byte sequence
                        {
                            _partialsLen = 1u;
                            --remainingLength;
                            _partials.front() = _buffer.back();
                        }
                        else
                        {
                            _partialsLen = 0u;
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
        // - Takes a UTF-16 string and populates it with *complete* UTF-16 code points.
        //   If it receives an incomplete code point, it will cache it until it can be completed.
        // Arguments:
        // - in - UTF-16 string_view potentially containing partial code points
        // - out - on return, populated with complete code points at the string end
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

                // copy the high surrogate that was remaining from the previous call (if any)
                if (_partialsLen != 0u)
                {
                    _buffer.push_back(_partials.front());
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
                    _partials.front() = in.back();
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

        // Method Description:
        // - Check if partials from text in the given codepage were cached.
        // Arguments:
        // - codepage - codepage from which the partials were cached
        // Return Value:
        // - true if partials were cached from text encoded in the given codepage, false otherwise
        template<class T = charT>
        [[nodiscard]] typename std::enable_if<std::is_same<T, char>::value, bool>::type
        empty(const unsigned int codepage) noexcept
        {
            return _codepage != codepage || _partialsLen == 0u;
        }

        // Method Description:
        // - Check if a high surrogate was cached.
        // Arguments:
        // - none
        // Return Value:
        // - true if a high surrogate was cached, false otherwise
        template<class T = charT>
        [[nodiscard]] typename std::enable_if<std::is_same<T, wchar_t>::value, bool>::type
        empty() noexcept
        {
            return _partialsLen == 0u;
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

        unsigned int _codepage; // previously used codepage
        CPINFO _cpInfo;
        std::basic_string<charT> _buffer; // buffer to which the populated string_view refers
        std::array<charT, 4> _partials; // buffer for code units of a partial code point that have to be cached
        size_t _partialsLen{}; // number of cached code units
    };

    // make clear what incoming string type the state is for
    using astate = au16state<char>; // ANSI and UTF-8
    using u16state = au16state<wchar_t>; // UTF-16

    // Routine Description:
    // - Takes an ANSI string and performs the conversion to UTF-16. NOTE: The function relies on getting complete ANSI characters at the string boundaries.
    // Arguments:
    // - codepage - id of the codepage the received string is encoded
    // - in - ANSI string to be converted
    // - out - reference to the resulting UTF-16 string
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_INVALIDARG  - the codepage id is invalid
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, char>::value && std::is_same<typename outT::value_type, wchar_t>::value, HRESULT>::type
    au16(const unsigned int codepage, const inT in, outT& out) noexcept
    {
        try
        {
            out.clear();

            if (codepage != gsl::narrow_cast<unsigned int>(CP_UTF8) && !IsValidCodePage(codepage))
            {
                return E_INVALIDARG;
            }

            if (in.empty())
            {
                return S_OK;
            }

            int lengthRequired{};
            // The worst ratio of ANSI code units to UTF-16 code units is 1 to 1 if ANSI consists of ASCII only.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthRequired));
            out.resize(in.length()); // avoid to call MultiByteToWideChar twice only to get the required size
            const int lengthOut{ MultiByteToWideChar(codepage, 0ul, in.data(), lengthRequired, out.data(), lengthRequired) };
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
    // - Takes an ANSI string, complements and/or caches partials, and performs the conversion to UTF-16. NOTE: The support is restricted. See description of au16state<char>.
    // Arguments:
    // - codepage - id of the codepage the received string is encoded
    // - in - ANSI string to be converted
    // - out - reference to the resulting UTF-16 string
    // - state - reference to a til::astate class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_INVALIDARG  - the codepage id is invalid
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, char>::value && std::is_same<typename outT::value_type, wchar_t>::value, HRESULT>::type
    au16(const unsigned int codepage, const inT in, outT& out, astate& state) noexcept
    {
        std::string_view sv{};
        RETURN_IF_FAILED(state(codepage, std::string_view{ in }, sv));
        return til::au16(codepage, sv, out);
    }

    // Routine Description:
    // - Takes an ANSI string and performs the conversion to UTF-16. NOTE: The function relies on getting complete ANSI characters at the string boundaries.
    // Arguments:
    // - codepage - id of the codepage the received string is encoded
    // - in - ANSI string to be converted
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, char>::value, std::wstring>::type
    au16(const unsigned int codepage, const inT in)
    {
        std::wstring out{};
        THROW_IF_FAILED(au16(codepage, std::string_view{ in }, out));
        return out;
    }

    // Routine Description:
    // Takes an ANSI string, complements and/or caches partials, and performs the conversion to UTF-16. NOTE: The support is restricted. See description of au16state<char>.
    // Arguments:
    // - codepage - id of the codepage the received string is encoded
    // - in - ANSI string to be converted
    // - state - reference to a til::astate class holding the status of the current partials handling
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, char>::value, std::wstring>::type
    au16(const unsigned int codepage, const inT in, astate& state)
    {
        std::wstring out{};
        THROW_IF_FAILED(au16(codepage, std::string_view{ in }, out, state));
        return out;
    }

    // Routine Description:
    // - Takes a UTF-16 string and performs the conversion to ANSI. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
    // Arguments:
    // - codepage - id of the codepage to be used for the conversion
    // - in - UTF-16 string to be converted
    // - out - reference to the resulting ANSI string
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_INVALIDARG  - the codepage id is invalid
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value && std::is_same<typename outT::value_type, char>::value, HRESULT>::type
    u16a(const unsigned int codepage, const inT in, outT& out) noexcept
    {
#pragma warning(push)
#pragma prefast(suppress \
                : __WARNING_W2A_BEST_FIT, "WC_NO_BEST_FIT_CHARS doesn't work in many codepages. Retain old behavior.")
        try
        {
            out.clear();

            if (!IsValidCodePage(codepage))
            {
                return E_INVALIDARG;
            }

            if (in.empty())
            {
                return S_OK;
            }

            int lengthIn{};
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthIn));

            int lengthRequired{};
            if (codepage == gsl::narrow_cast<unsigned int>(CP_UTF8)) // UTF-8
            {
                // Avoid to call WideCharToMultiByte twice only to get the required size.
                // Code Point U+0000..U+FFFF: 1 UTF-16 code unit --> 1..3 UTF-8 code units.
                // Code Points >U+FFFF: 2 UTF-16 code units --> 4 UTF-8 code units.
                // Thus, the worst ratio of UTF-16 code units to UTF-8 code units is 1 to 3.
                RETURN_HR_IF(E_ABORT, !base::CheckMul(lengthIn, 3).AssignIfValid(&lengthRequired));
            }
            else // ANSI codepages
            {
                // We have to call WideCharToMultiByte to get the required size because we can't predict it for all possible codepages.
                lengthRequired = WideCharToMultiByte(codepage, 0ul, in.data(), lengthIn, nullptr, 0, nullptr, nullptr);
                if (lengthRequired == 0)
                {
                    return E_ABORT;
                }
            }

            out.resize(gsl::narrow_cast<size_t>(lengthRequired));
            const int lengthOut{ WideCharToMultiByte(codepage, 0ul, in.data(), lengthIn, out.data(), lengthRequired, nullptr, nullptr) };
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
#pragma warning(pop)
    }

    // Routine Description:
    // - Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to ANSI.
    // Arguments:
    // - codepage - id of the codepage to be used for the conversion
    // - in - UTF-16 string to be converted
    // - out - reference to the resulting ANSI string
    // - state - reference to a til::u16state class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded without any change of the represented code points
    // - E_INVALIDARG  - the codepage id is invalid
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value && std::is_same<typename outT::value_type, char>::value, HRESULT>::type
    u16a(const unsigned int codepage, const inT in, outT& out, u16state& state) noexcept
    {
        std::wstring_view sv{};
        RETURN_IF_FAILED(state(std::wstring_view{ in }, sv));
        return u16a(codepage, sv, out);
    }

    // Routine Description:
    // - Takes a UTF-16 string and performs the conversion to ANSI. NOTE: The function relies on getting complete UTF-16 characters at the string boundaries.
    // Arguments:
    // - codepage - id of the codepage to be used for the conversion
    // - in - UTF-16 string to be converted
    // Return Value:
    // - the resulting ANSI string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value, std::string>::type
    u16a(const unsigned int codepage, const inT in)
    {
        std::string out{};
        THROW_IF_FAILED(u16a(codepage, std::wstring_view{ in }, out));
        return out;
    }

    // Routine Description:
    // Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to ANSI.
    // Arguments:
    // - codepage - id of the codepage to be used for the conversion
    // - in - UTF-16 string to be converted
    // - state - reference to a til::u16state class holding the status of the current partials handling
    // Return Value:
    // - the resulting ANSI string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, wchar_t>::value, std::string>::type
    u16a(const unsigned int codepage, const inT in, u16state& state)
    {
        std::string out{};
        THROW_IF_FAILED(u16a(codepage, std::wstring_view{ in }, out, state));
        return out;
    }

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
        return au16(gsl::narrow_cast<unsigned int>(CP_UTF8), std::string_view{ in }, out);
    }

    // Routine Description:
    // - Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
    // Arguments:
    // - in - UTF-8 string to be converted
    // - out - reference to the resulting UTF-16 string
    // - state - reference to a til::astate class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class inT, class outT>
    [[nodiscard]] typename std::enable_if<std::is_same<typename inT::value_type, char>::value && std::is_same<typename outT::value_type, wchar_t>::value, HRESULT>::type
    u8u16(const inT in, outT& out, astate& state) noexcept
    {
        std::string_view sv{};
        RETURN_IF_FAILED(state(gsl::narrow_cast<unsigned int>(CP_UTF8), std::string_view{ in }, sv));
        return til::u8u16(sv, out);
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
    // - state - reference to a til::astate class holding the status of the current partials handling
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if<std::is_same<typename inT::value_type, char>::value, std::wstring>::type
    u8u16(const inT in, astate& state)
    {
        std::wstring out{};
        THROW_IF_FAILED(u8u16(std::string_view{ in }, out, state));
        return out;
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
        return u16a(gsl::narrow_cast<unsigned int>(CP_UTF8), std::wstring_view{ in }, out);
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
