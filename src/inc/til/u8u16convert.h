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
- Steffen Illhardt (german-one), Leonard Hecker (lhecker) 2020-2021
--*/

#pragma once

namespace til // Terminal Implementation Library. Also: "Today I Learned"
{
    // state structure for maintenance of UTF-8 partials
    struct u8state
    {
        char partials[4];
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
    // - E_UNEXPECTED  - the underlying conversion function failed
    // - HRESULT value converted from a caught exception
    template<class outT>
    [[nodiscard]] HRESULT u8u16(const std::string_view& in, outT& out) noexcept
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
        CATCH_RETURN();
    }

#pragma warning(push)
#pragma warning(disable : 26429 26446 26459 26481 26482) // use not_null, subscript operator, use span, pointer arithmetic, dynamic array indexing
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
    // - E_UNEXPECTED  - the underlying conversion function failed
    // - HRESULT value converted from a caught exception
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
                const auto copyable{ std::min<int>(state.want, len8) };
                std::move(cursor8, cursor8 + copyable, &state.partials[state.have]);
                state.have += gsl::narrow_cast<uint8_t>(copyable);
                state.want -= gsl::narrow_cast<uint8_t>(copyable);
                if (state.want) // we still didn't get enough data to complete the code point, however this is not an error
                {
                    out.clear();
                    return S_OK;
                }

                len16 = MultiByteToWideChar(CP_UTF8, 0UL, &state.partials[0], gsl::narrow_cast<int>(state.have), out.data(), capa16);
                RETURN_HR_IF(E_UNEXPECTED, !len16);

                capa16 -= len16;
                len8 -= copyable;
                cursor8 += copyable;
                // state.want is already zero at this point
                state.have = 0;
            }

            if (len8)
            {
                auto backIter{ cursor8 + len8 - 1 };
                int sequenceLen{ 1 };

                // skip UTF8 continuation bytes
                while (backIter != cursor8 && (*backIter & 0b11'000000) == 0b10'000000)
                {
                    --backIter;
                    ++sequenceLen;
                }

                // credits go to Christopher Wellons for this algorithm to determine the length of a UTF-8 code point
                // it is released into the Public Domain. https://github.com/skeeto/branchless-utf8
                static constexpr uint8_t lengths[]{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
                const auto codePointLen{ lengths[gsl::narrow_cast<uint8_t>(*backIter) >> 3] };

                if (codePointLen > sequenceLen)
                {
                    std::move(backIter, backIter + sequenceLen, &state.partials[0]);
                    len8 -= sequenceLen;
                    state.have = gsl::narrow_cast<uint8_t>(sequenceLen);
                    state.want = gsl::narrow_cast<uint8_t>(codePointLen - sequenceLen);
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
        CATCH_RETURN();
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
    // - E_UNEXPECTED  - the underlying conversion function failed
    // - HRESULT value converted from a caught exception
    template<class outT>
    [[nodiscard]] HRESULT u16u8(const std::wstring_view& in, outT& out) noexcept
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
        CATCH_RETURN();
    }

#pragma warning(push)
#pragma warning(disable : 26429 26446 26459 26481) // use not_null, subscript operator, use span, pointer arithmetic
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
    // - E_UNEXPECTED  - the underlying conversion function failed
    // - HRESULT value converted from a caught exception
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
                state.partials[1] = *cursor16;
                len8 = WideCharToMultiByte(CP_UTF8, 0UL, &state.partials[0], 2, out.data(), capa8, nullptr, nullptr);
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
        CATCH_RETURN();
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
    inline std::wstring u8u16(const std::string_view& in, u8state& state)
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
    inline std::string u16u8(const std::wstring_view& in, u16state& state)
    {
        std::string out{};
        THROW_IF_FAILED(u16u8(in, out, state));
        return out;
    }
}
