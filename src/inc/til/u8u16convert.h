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
    // state structure for maintenance of UTF-8 partials
    struct u8state
    {
        std::array<char, 4> partials{}; // first 3 bytes are for the code units of a partial code point, last byte is the number of cached code units

        constexpr void reset() noexcept
        {
            *this = {};
        }
    };

    // state structure for maintenance of UTF-16 partials
    struct u16state
    {
        std::array<wchar_t, 2> partials{}; // first element is for the code unit of a partial code point, last element indicates a cached code unit

        constexpr void reset() noexcept
        {
            *this = {};
        }
    };

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
    template<class outT>
    [[nodiscard]] typename std::enable_if_t<std::is_same_v<typename outT::value_type, wchar_t>, HRESULT>
    u8u16(const std::string_view& in, outT& out) noexcept
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

#pragma warning(push)
#pragma warning(disable : 26429 26446 26459 26481 26482) // use not_null,  subscript operator, use span, pointer arithmetic,  dynamic array indexing
    // Routine Description:
    // - Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
    // Arguments:
    // - in - UTF-8 string to be converted
    // - out - reference to the resulting UTF-16 string
    // - state - reference to a til::u8state holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - S_FALSE       - the resulting string is converted from the previously cached partials only
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class outT>
    [[nodiscard]] HRESULT u8u16(const std::string_view& in, outT& out, u8state& state) noexcept
    {
        try
        {
            /* clang-format off */
            enum Utf8BitMasks : char
            {
                IsAscii   = gsl::narrow_cast<char>(0b0'0000000), // Any byte representing an ASCII character has the MSB set to 0
                MaskAscii = gsl::narrow_cast<char>(0b1'0000000), // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsAscii pattern
                IsCont    = gsl::narrow_cast<char>(0b10'000000), // Continuation bytes of any UTF-8 non-ASCII character have the MSB set to 1 and the adjacent bit set to 0
                MaskCont  = gsl::narrow_cast<char>(0b11'000000), // Bit mask to be used in a bitwise AND operation to find out whether or not a byte match the IsCont pattern
                IsLead2   = gsl::narrow_cast<char>(0b110'00000), // A lead byte that indicates a UTF-8 non-ASCII character consisting of two bytes has the two highest bits set to 1 and the adjacent bit set to 0
                MaskLead2 = gsl::narrow_cast<char>(0b111'00000), // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLead2 pattern
                IsLead3   = gsl::narrow_cast<char>(0b1110'0000), // A lead byte that indicates a UTF-8 non-ASCII character consisting of three bytes has the three highest bits set to 1 and the adjacent bit set to 0
                MaskLead3 = gsl::narrow_cast<char>(0b1111'0000), // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLead3 pattern
                IsLead4   = gsl::narrow_cast<char>(0b11110'000), // A lead byte that indicates a UTF-8 non-ASCII character consisting of four bytes has the four highest bits set to 1 and the adjacent bit set to 0
                MaskLead4 = gsl::narrow_cast<char>(0b11111'000), // Bit mask to be used in a bitwise AND operation to find out whether or not a lead byte match the IsLead4 pattern
            };
            /* clang-format on */

            constexpr const std::array<char, 4> cmpMasks{
                '\0', // unused
                MaskCont,
                MaskLead2,
                MaskLead3,
            };

            constexpr const std::array<char, 4> cmpOperands{
                '\0', // unused
                IsAscii, // intentionally conflicts with MaskCont
                IsLead2,
                IsLead3,
            };

            int capa16{};
            // The worst ratio of UTF-8 code units to UTF-16 code units is 1 to 1 if UTF-8 consists of ASCII only.
            RETURN_HR_IF(E_ABORT, !base::CheckAdd(in.length(), state.partials[3]).AssignIfValid(&capa16));

            out.clear();
            RETURN_HR_IF(S_OK, !capa16); // nothing to convert, however this is not an error

            out.resize(gsl::narrow_cast<size_t>(capa16));
            auto len8{ gsl::narrow_cast<int>(in.length()) };
            int len16{};
            auto cursor8{ in.data() };
            if (!!state.partials[3])
            {
                if (!len8) // only the partial code point will be converted and published
                {
                    len16 = MultiByteToWideChar(CP_UTF8, 0UL, state.partials.data(), gsl::narrow_cast<int>(state.partials[3]), out.data(), capa16);
                    RETURN_HR_IF(E_UNEXPECTED, !len16);

                    state.reset();
                    out.resize(gsl::narrow_cast<size_t>(len16));
                    return S_FALSE;
                }

                // determine the number of bytes of the code point indicated by the lead byte in the cache
                const auto codePointLen{
                    2 + // the code point consists of at least 2 bytes because if we cached data it will never be ASCII
                    gsl::narrow_cast<int>((state.partials[0] & IsLead3) == IsLead3) + // add one if 3 high-order bits of the lead byte are set
                    gsl::narrow_cast<int>((state.partials[0] & IsLead4) == IsLead4) // add another one if 4 high-order bits of the lead byte are set
                };

                const auto fetch8{ codePointLen - gsl::narrow_cast<int>(state.partials[3]) }; // number of bytes we need from the begin of the chunk to complete the code point
                if (fetch8 > len8) // we still didn't get enough data to complete the code point, however this is not an error
                {
                    std::move(cursor8, cursor8 + len8, state.partials.data() + state.partials[3]);
                    state.partials[3] += gsl::narrow_cast<char>(len8);
                    out.clear();
                    return S_OK;
                }

                std::move(cursor8, cursor8 + fetch8, state.partials.data() + state.partials[3]);
                len16 = MultiByteToWideChar(CP_UTF8, 0UL, state.partials.data(), gsl::narrow_cast<int>(codePointLen), out.data(), capa16);
                RETURN_HR_IF(E_UNEXPECTED, !len16);

                state.reset();
                capa16 -= len16;
                len8 -= fetch8;
                cursor8 += fetch8;
            }

            if (!!len8)
            {
                auto backIter{ cursor8 + len8 };
                // If the last byte in the string was a byte belonging to a UTF-8 multi-byte character
                if (!!(backIter[-1] & MaskAscii))
                {
                    // Check only up to 3 last bytes, if no Lead Byte is found then the byte before will be the Lead Byte and no partials are in the string
                    const auto stopLen{ std::min(len8, 3) };
                    for (auto sequenceLen{ 1 }; sequenceLen <= stopLen; ++sequenceLen)
                    {
                        --backIter;
                        // If Lead Byte found
                        if ((*backIter & MaskCont) > IsCont)
                        {
                            // If the Lead Byte indicates that the last bytes in the string is a partial UTF-8 code point then cache them:
                            //  Use the bitmask at index `sequenceLen`. Compare the result with the operand having the same index. If they
                            //  are not equal then the sequence has to be cached because it is a partial code point. Otherwise the
                            //  sequence is a complete UTF-8 code point and the whole string is ready for the conversion into a UTF-16 string.
                            if ((*backIter & cmpMasks[gsl::narrow_cast<size_t>(sequenceLen)]) != cmpOperands[gsl::narrow_cast<size_t>(sequenceLen)])
                            {
                                std::move(backIter, backIter + sequenceLen, state.partials.data());
                                len8 -= sequenceLen;
                                state.partials[3] = gsl::narrow_cast<char>(sequenceLen);
                            }

                            break;
                        }
                    }
                }
            }

            if (!!len8)
            {
                const auto convLen{ MultiByteToWideChar(CP_UTF8, 0UL, cursor8, len8, out.data() + len16, capa16) };
                RETURN_HR_IF(E_UNEXPECTED, !convLen);

                len16 += convLen;
            }

            out.resize(gsl::narrow_cast<size_t>(len16));
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
#pragma warning(pop)

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
    template<class outT>
    [[nodiscard]] typename std::enable_if_t<std::is_same_v<typename outT::value_type, char>, HRESULT>
    u16u8(const std::wstring_view& in, outT& out) noexcept
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

#pragma warning(push)
#pragma warning(disable : 26446 26459 26481) // subscript operator, use span, pointer arithmetic
    // Routine Description:
    // - Takes a UTF-16 string, complements and/or caches partials, and performs the conversion to UTF-8.
    // Arguments:
    // - in - UTF-16 string to be converted
    // - out - reference to the resulting UTF-8 string
    // - state - reference to a til::u16state class holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded without any change of the represented code points
    // - S_FALSE       - the resulting string is converted from the previously cached partial only
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class outT>
    [[nodiscard]] HRESULT u16u8(const std::wstring_view& in, outT& out, u16state& state) noexcept
    {
        try
        {
            int len16{};
            int capa8{};
            // The worst ratio of UTF-16 code units to UTF-8 code units is 1 to 3.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&len16) || !base::CheckAdd(len16, state.partials[1]).AssignIfValid(&capa8) || !base::CheckMul(capa8, 3).AssignIfValid(&capa8));

            out.clear();
            RETURN_HR_IF(S_OK, !capa8); // nothing to convert, however this is not an error

            out.resize(gsl::narrow_cast<size_t>(capa8));
            int len8{};
            auto cursor16{ in.data() };
            if (!!state.partials[1])
            {
                if (!len16) // only the partial code point will be converted and published
                {
                    len8 = WideCharToMultiByte(CP_UTF8, 0UL, state.partials.data(), 1, out.data(), capa8, nullptr, nullptr);
                    RETURN_HR_IF(E_UNEXPECTED, !len8);

                    state.reset();
                    out.resize(gsl::narrow_cast<size_t>(len8));
                    return S_FALSE;
                }

                std::move(in.data(), in.data() + 1, state.partials.data() + 1);
                len8 = WideCharToMultiByte(CP_UTF8, 0UL, state.partials.data(), 2, out.data(), capa8, nullptr, nullptr);
                RETURN_HR_IF(E_UNEXPECTED, !len8);

                state.reset();
                capa8 -= len8;
                --len16;
                ++cursor16;
            }

            if (!!len16)
            {
                const auto back = *(cursor16 + len16 - 1);
                if (back >= 0xD800 && back <= 0xDBFF) // cache the last value in the string if it is in the range of high surrogates
                {
                    state.partials = { back, L'\1' };
                    --len16;
                }
            }

            if (!!len16)
            {
                const auto convLen{ WideCharToMultiByte(CP_UTF8, 0UL, cursor16, len16, out.data() + len8, capa8, nullptr, nullptr) };
                RETURN_HR_IF(E_UNEXPECTED, !convLen);

                len8 += convLen;
            }

            out.resize(gsl::narrow_cast<size_t>(len8));
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
#pragma warning(pop)

    // Routine Description:
    // - Takes a UTF-8 string and performs the conversion to UTF-16. NOTE: The function relies on getting complete UTF-8 characters at the string boundaries.
    // Arguments:
    // - in - UTF-8 string to be converted
    // Return Value:
    // - the resulting UTF-16 string
    // - NOTE: Throws HRESULT errors that the non-throwing sibling returns
    template<class inT>
    typename std::enable_if_t<std::is_same_v<typename inT::value_type, char>, std::wstring>
    u8u16(const inT& in)
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
    template<class stateT>
    typename std::enable_if_t<std::is_same_v<stateT, u8state>, std::wstring>
    u8u16(const std::string_view& in, stateT& state)
    {
        std::wstring out{};
        THROW_IF_FAILED(u8u16(in, out, state));
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
    typename std::enable_if_t<std::is_same_v<typename inT::value_type, wchar_t>, std::string>
    u16u8(const inT& in)
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
    template<class stateT>
    typename std::enable_if_t<std::is_same_v<stateT, u16state>, std::string>
    u16u8(const std::wstring_view& in, stateT& state)
    {
        std::string out{};
        THROW_IF_FAILED(u16u8(in, out, state));
        return out;
    }
}
