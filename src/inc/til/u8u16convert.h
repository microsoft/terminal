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
        char partials[4]{};
        uint8_t have{};
        uint8_t want{};

        constexpr void reset() noexcept
        {
            *this = {};
        }
    };

    // state structure for maintenance of UTF-16 partials
    struct u16state
    {
        wchar_t partials[2]{};

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
            RETURN_HR_IF(S_OK, in.empty());

            int lengthRequired{};
            // The worst ratio of UTF-8 code units to UTF-16 code units is 1 to 1 if UTF-8 consists of ASCII only.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthRequired));
            out.resize(in.length()); // avoid to call MultiByteToWideChar twice only to get the required size
            const int lengthOut = MultiByteToWideChar(CP_UTF8, 0ul, in.data(), lengthRequired, out.data(), lengthRequired);
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
#pragma warning(disable : 26429 26446 26459 26481 26482 26485) // use not_null, subscript operator, use span, pointer arithmetic, dynamic array indexing, array to pointer decay
    // Routine Description:
    // - Takes a UTF-8 string, complements and/or caches partials, and performs the conversion to UTF-16.
    // Arguments:
    // - in - UTF-8 string to be converted
    // - out - reference to the resulting UTF-16 string
    // - state - reference to a til::u8state holding the status of the current partials handling
    // Return Value:
    // - S_OK          - the conversion succeeded
    // - E_OUTOFMEMORY - the function failed to allocate memory for the resulting string
    // - E_ABORT       - the resulting string length would exceed the upper boundary of an int and thus, the conversion was aborted before the conversion has been completed
    // - E_UNEXPECTED  - an unexpected error occurred
    template<class outT>
    [[nodiscard]] HRESULT u8u16(const std::string_view& in, outT& out, u8state& state) noexcept
    {
        try
        {
            out.clear();
            RETURN_HR_IF(S_OK, in.empty());

            int capa16{};
            // The worst ratio of UTF-8 code units to UTF-16 code units is 1 to 1 if UTF-8 consists of ASCII only.
            RETURN_HR_IF(E_ABORT, !base::CheckAdd(in.length(), state.have).AssignIfValid(&capa16));

            out.resize(gsl::narrow_cast<size_t>(capa16));
            auto len8{ gsl::narrow_cast<int>(in.length()) };
            int len16{};
            auto cursor8{ in.data() };
            if (state.have)
            {
                if (state.want > len8) // we still didn't get enough data to complete the code point, however this is not an error
                {
                    std::move(cursor8, cursor8 + len8, state.partials + state.have);
                    state.have += gsl::narrow_cast<uint8_t>(len8);
                    state.want -= gsl::narrow_cast<uint8_t>(len8);
                    out.clear();
                    return S_OK;
                }

                std::move(cursor8, cursor8 + state.want, state.partials + state.have);
                len16 = MultiByteToWideChar(CP_UTF8, 0UL, state.partials, gsl::narrow_cast<int>(state.have) + state.want, out.data(), capa16);
                RETURN_HR_IF(E_UNEXPECTED, !len16);

                capa16 -= len16;
                len8 -= state.want;
                cursor8 += state.want;
                state.reset();
            }

            if (len8)
            {
                auto backIter{ cursor8 + len8 };
                // If the last byte in the string was a byte belonging to a UTF-8 multi-byte character
                if (backIter[-1] & 0b1'0000000)
                {
                    // Check only up to 3 last bytes, if no Lead Byte is found then the byte before will be the Lead Byte and no partials are in the string
                    const auto stopLen{ std::min(len8, 3) };
                    for (auto sequenceLen{ 1 }; sequenceLen <= stopLen; ++sequenceLen)
                    {
                        --backIter;
                        // we found a Lead Byte if at least 2 high-order bits are set
                        if ((*backIter & 0b11'000000) == 0b11'000000)
                        {
                            // determine the number of bytes of the code point indicated by the lead byte
                            const auto codePointLen{
                                2 + // the code point consists of at least 2 bytes because we found a lead byte
                                gsl::narrow_cast<int>((*backIter & 0b111'00000) == 0b111'00000) + // add one if at least 3 high-order bits of the lead byte are set
                                gsl::narrow_cast<int>((*backIter & 0b1111'0000) == 0b1111'0000) // add another one if 4 high-order bits of the lead byte are set
                            };

                            // if the length of the code point differs from the length we stepped backward, we will capture the last sequence which is a partial code point
                            if (codePointLen != sequenceLen)
                            {
                                std::move(backIter, backIter + sequenceLen, state.partials);
                                len8 -= sequenceLen;
                                state.have = gsl::narrow_cast<uint8_t>(sequenceLen);
                                state.want = gsl::narrow_cast<uint8_t>(codePointLen - sequenceLen);
                            }

                            break;
                        }
                    }
                }
            }

            if (len8)
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
            RETURN_HR_IF(S_OK, in.empty());

            int lengthIn{};
            int lengthRequired{};
            // Code Point U+0000..U+FFFF: 1 UTF-16 code unit --> 1..3 UTF-8 code units.
            // Code Points >U+FFFF: 2 UTF-16 code units --> 4 UTF-8 code units.
            // Thus, the worst ratio of UTF-16 code units to UTF-8 code units is 1 to 3.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&lengthIn) || !base::CheckMul(lengthIn, 3).AssignIfValid(&lengthRequired));
            out.resize(gsl::narrow_cast<size_t>(lengthRequired)); // avoid to call WideCharToMultiByte twice only to get the required size
            const int lengthOut = WideCharToMultiByte(CP_UTF8, 0ul, in.data(), lengthIn, out.data(), lengthRequired, nullptr, nullptr);
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
#pragma warning(disable : 26429 26446 26459 26481 26485) // use not_null, subscript operator, use span, pointer arithmetic, array to pointer decay
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
    template<class outT>
    [[nodiscard]] HRESULT u16u8(const std::wstring_view& in, outT& out, u16state& state) noexcept
    {
        try
        {
            out.clear();
            RETURN_HR_IF(S_OK, in.empty());

            int len16{};
            int capa8{};
            // The worst ratio of UTF-16 code units to UTF-8 code units is 1 to 3.
            RETURN_HR_IF(E_ABORT, !base::MakeCheckedNum(in.length()).AssignIfValid(&len16) || !base::CheckAdd(len16, gsl::narrow_cast<int>(state.partials[0]) != 0).AssignIfValid(&capa8) || !base::CheckMul(capa8, 3).AssignIfValid(&capa8));

            out.resize(gsl::narrow_cast<size_t>(capa8));
            int len8{};
            auto cursor16{ in.data() };
            if (state.partials[0])
            {
                std::move(in.data(), in.data() + 1, state.partials + 1);
                len8 = WideCharToMultiByte(CP_UTF8, 0UL, state.partials, 2, out.data(), capa8, nullptr, nullptr);
                RETURN_HR_IF(E_UNEXPECTED, !len8);

                state.reset();
                capa8 -= len8;
                --len16;
                ++cursor16;
            }

            if (len16)
            {
                const auto back = *(cursor16 + len16 - 1);
                if (back >= 0xD800 && back <= 0xDBFF) // cache the last value in the string if it is in the range of high surrogates
                {
                    state.partials[0] = back;
                    --len16;
                }
            }

            if (len16)
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
    inline std::wstring u8u16(const std::string_view& in)
    {
        std::wstring out{};
        THROW_IF_FAILED(u8u16(in, out));
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
    inline std::string u16u8(const std::wstring_view& in)
    {
        std::string out{};
        THROW_IF_FAILED(u16u8(in, out));
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
