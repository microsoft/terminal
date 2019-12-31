// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../inc/Utf8Utf16Convert.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class Utf8Utf16ConvertTests
{
    TEST_CLASS(Utf8Utf16ConvertTests);

    TEST_METHOD(TestU8ToU16);
    TEST_METHOD(TestU16ToU8);
    TEST_METHOD(TestUTF8PartialHandler);
    TEST_METHOD(TestUTF16PartialHandler);
    TEST_METHOD(TestUTF8ChunkToUTF16Converter);
    TEST_METHOD(TestUTF16ChunkToUTF8Converter);
};

void Utf8Utf16ConvertTests::TestU8ToU16()
{
    const std::string u8String{
        '\x7E', // TILDE (1 byte)
        '\xC3', // LATIN SMALL LETTER O WITH DIAERESIS (2 bytes)
        '\xB6',
        '\xE2', // EURO SIGN (3 bytes)
        '\x82',
        '\xAC',
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C (4 bytes)
        '\xA4',
        '\xBD',
        '\x9C'
    };

    const std::wstring u16StringComp{
        gsl::narrow_cast<wchar_t>(0x007eU), // TILDE
        gsl::narrow_cast<wchar_t>(0x00f6U), // LATIN SMALL LETTER O WITH DIAERESIS
        gsl::narrow_cast<wchar_t>(0x20acU), // EURO SIGN
        gsl::narrow_cast<wchar_t>(0xd853U), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xdf5cU)
    };

    std::wstring u16Out{};
    const HRESULT hRes{ U8ToU16(u8String, u16Out) };
    VERIFY_ARE_EQUAL(S_OK, hRes);
    VERIFY_ARE_EQUAL(u16StringComp, u16Out);
}

void Utf8Utf16ConvertTests::TestU16ToU8()
{
    const std::wstring u16String{
        gsl::narrow_cast<wchar_t>(0x007eU), // TILDE
        gsl::narrow_cast<wchar_t>(0x00f6U), // LATIN SMALL LETTER O WITH DIAERESIS
        gsl::narrow_cast<wchar_t>(0x20acU), // EURO SIGN
        gsl::narrow_cast<wchar_t>(0xd853U), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xdf5cU)
    };

    const std::string u8StringComp{
        '\x7E', // TILDE (1 byte)
        '\xC3', // LATIN SMALL LETTER O WITH DIAERESIS (2 bytes)
        '\xB6',
        '\xE2', // EURO SIGN (3 bytes)
        '\x82',
        '\xAC',
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C (4 bytes)
        '\xA4',
        '\xBD',
        '\x9C'
    };

    std::string u8Out{};
    const HRESULT hRes{ U16ToU8(u16String, u8Out) };
    VERIFY_ARE_EQUAL(S_OK, hRes);
    VERIFY_ARE_EQUAL(u8StringComp, u8Out);
}

void Utf8Utf16ConvertTests::TestUTF8PartialHandler()
{
    const std::string u8String1{
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C (4 bytes)
        '\xA4',
        '\xBD',
        '\x9C',
        '\xF0' // CJK UNIFIED IDEOGRAPH-24F5C (lead byte only)
    };

    const std::string u8String2{
        '\xA4', // CJK UNIFIED IDEOGRAPH-24F5C (complementary bytes)
        '\xBD',
        '\x9C'
    };

    const std::string u8StringComp{
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C (4 bytes)
        '\xA4',
        '\xBD',
        '\x9C'
    };

    UTF8PartialHandler handleU8Partials{};

    std::string_view u8Sv1{ u8String1 };
    const HRESULT hRes1{ handleU8Partials(u8Sv1) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u8StringComp, u8Sv1);

    std::string_view u8Sv2{ u8String2 };
    const HRESULT hRes2{ handleU8Partials(u8Sv2) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u8StringComp, u8Sv2);
}

void Utf8Utf16ConvertTests::TestUTF16PartialHandler()
{
    const std::wstring u16String1{
        gsl::narrow_cast<wchar_t>(0xD853), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xDF5C),
        gsl::narrow_cast<wchar_t>(0xD853) // CJK UNIFIED IDEOGRAPH-24F5C (high surrogate only)
    };

    const std::wstring u16String2{
        gsl::narrow_cast<wchar_t>(0xDF5C) // CJK UNIFIED IDEOGRAPH-24F5C (low surrogate only)
    };

    const std::wstring u16StringComp{
        gsl::narrow_cast<wchar_t>(0xD853), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xDF5C)
    };

    UTF16PartialHandler handleU16Partials{};

    std::wstring_view u16Sv1{ u16String1 };
    const HRESULT hRes1{ handleU16Partials(u16Sv1) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u16StringComp, u16Sv1);

    std::wstring_view u16Sv2{ u16String2 };
    const HRESULT hRes2{ handleU16Partials(u16Sv2) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u16StringComp, u16Sv2);
}

void Utf8Utf16ConvertTests::TestUTF8ChunkToUTF16Converter()
{
    const std::string u8String1{
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C (4 bytes)
        '\xA4',
        '\xBD',
        '\x9C',
        '\xF0' // CJK UNIFIED IDEOGRAPH-24F5C (lead byte only)
    };

    const std::string u8String2{
        '\xA4', // CJK UNIFIED IDEOGRAPH-24F5C (complementary bytes)
        '\xBD',
        '\x9C'
    };

    const std::wstring u16StringComp{
        gsl::narrow_cast<wchar_t>(0xD853), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xDF5C)
    };

    UTF8ChunkToUTF16Converter convertUTF8ChunkToUTF16{};

    std::wstring_view u16SvOut1{};
    const HRESULT hRes1{ convertUTF8ChunkToUTF16(u8String1, u16SvOut1) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u16StringComp, u16SvOut1);

    std::wstring_view u16SvOut2{};
    const HRESULT hRes2{ convertUTF8ChunkToUTF16(u8String2, u16SvOut2) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u16StringComp, u16SvOut2);
}

void Utf8Utf16ConvertTests::TestUTF16ChunkToUTF8Converter()
{
    const std::wstring u16String1{
        gsl::narrow_cast<wchar_t>(0xD853), // CJK UNIFIED IDEOGRAPH-24F5C (surrogate pair)
        gsl::narrow_cast<wchar_t>(0xDF5C),
        gsl::narrow_cast<wchar_t>(0xD853) // CJK UNIFIED IDEOGRAPH-24F5C (high surrogate only)
    };

    const std::wstring u16String2{
        gsl::narrow_cast<wchar_t>(0xDF5C) // CJK UNIFIED IDEOGRAPH-24F5C (low surrogate only)
    };

    const std::string u8StringComp{
        '\xF0', // CJK UNIFIED IDEOGRAPH-24F5C
        '\xA4',
        '\xBD',
        '\x9C'
    };

    UTF16ChunkToUTF8Converter convertUTF16ChunkToUTF8{};

    std::string_view u8SvOut1{};
    const HRESULT hRes1{ convertUTF16ChunkToUTF8(u16String1, u8SvOut1) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u8StringComp, u8SvOut1);

    std::string_view u8SvOut2{};
    const HRESULT hRes2{ convertUTF16ChunkToUTF8(u16String2, u8SvOut2) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u8StringComp, u8SvOut2);
}
