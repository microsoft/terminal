// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class Utf8Utf16ConvertTests
{
    TEST_CLASS(Utf8Utf16ConvertTests);

    TEST_METHOD(TestU8ToU16);
    TEST_METHOD(TestU16ToU8);
    TEST_METHOD(TestU8ToU16Partials);
    TEST_METHOD(TestU16ToU8Partials);
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
    const HRESULT hRes{ til::u8u16(u8String, u16Out) };
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
    const HRESULT hRes{ til::u16u8(u16String, u8Out) };
    VERIFY_ARE_EQUAL(S_OK, hRes);
    VERIFY_ARE_EQUAL(u8StringComp, u8Out);
}

void Utf8Utf16ConvertTests::TestU8ToU16Partials()
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

    til::u8state state{};

    std::wstring u16Out1{};
    const HRESULT hRes1{ til::u8u16(u8String1, u16Out1, state) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u16StringComp, u16Out1);

    std::wstring u16Out2{};
    const HRESULT hRes2{ til::u8u16(u8String2, u16Out2, state) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u16StringComp, u16Out2);
}

void Utf8Utf16ConvertTests::TestU16ToU8Partials()
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

    til::u16state state{};

    std::string u8Out1{};
    const HRESULT hRes1{ til::u16u8(u16String1, u8Out1, state) };
    VERIFY_ARE_EQUAL(S_OK, hRes1);
    VERIFY_ARE_EQUAL(u8StringComp, u8Out1);

    std::string u8Out2{};
    const HRESULT hRes2{ til::u16u8(u16String2, u8Out2, state) };
    VERIFY_ARE_EQUAL(S_OK, hRes2);
    VERIFY_ARE_EQUAL(u8StringComp, u8Out2);
}
