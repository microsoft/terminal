// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include <til/rand.h>
#include <wincrypt.h>

#include "base64.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace Microsoft
{
    namespace Console
    {
        namespace VirtualTerminal
        {
            class Base64Test;
        };
    };
};

using namespace Microsoft::Console::VirtualTerminal;

class Microsoft::Console::VirtualTerminal::Base64Test
{
    TEST_CLASS(Base64Test);

    TEST_METHOD(DecodeFuzz)
    {
        // NOTE: Modify testRounds to get the feeling of running a fuzz test on Base64::Decode.
        static constexpr auto testRounds = 8;
        pcg_engines::oneseq_dxsm_64_32 rng{ til::gen_random<uint64_t>() };

        // Fills referenceData with random ASCII characters.
        // We use ASCII as Base64::Decode uses til:u8u16 internally and I don't want to test that.
        char referenceData[128];
        {
            uint32_t randomData[sizeof(referenceData) / sizeof(uint32_t)];
            for (auto& i : randomData)
            {
                i = rng();
            }

            const std::string_view randomDataView{ reinterpret_cast<const char*>(randomData), sizeof(randomData) };
            auto out = std::begin(referenceData);

            for (const auto& ch : randomDataView)
            {
                *out++ = static_cast<char>(ch & 0x7f);
            }
        }

        wchar_t wideReferenceData[std::size(referenceData)];
        std::copy_n(std::begin(referenceData), std::size(referenceData), std::begin(wideReferenceData));

        std::wstring encoded;
        std::wstring decoded;

        for (auto i = 0; i < testRounds; ++i)
        {
            const auto referenceLength = rng(static_cast<uint32_t>(std::size(referenceData)));
            const std::wstring_view wideReference{ std::begin(wideReferenceData), referenceLength };

            if (!referenceLength)
            {
                encoded.clear();
            }
            else
            {
                const auto reference = reinterpret_cast<const BYTE*>(std::begin(referenceData));
                DWORD encodedLen;
                THROW_IF_WIN32_BOOL_FALSE(CryptBinaryToStringW(reference, referenceLength, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encodedLen));

                // encodedLen is returned by CryptBinaryToStringW including the trailing null byte.
                encoded.resize(encodedLen - 1);

                THROW_IF_WIN32_BOOL_FALSE(CryptBinaryToStringW(reference, referenceLength, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, encoded.data(), &encodedLen));
            }

            // Test whether Decode() handles strings with and without trailing "=".
            if (rng(2))
            {
                while (!encoded.empty() && encoded.back() == '=')
                {
                    encoded.pop_back();
                }
            }

            // Test whether Decode() handles null-pointer arguments correctly.
            std::wstring_view encodedView{ encoded };
            if (encodedView.empty() && rng(2))
            {
                encodedView = {};
            }

            Base64::Decode(encodedView, decoded);
            VERIFY_ARE_EQUAL(wideReference, decoded);
        }
    }

    TEST_METHOD(DecodeUTF8)
    {
        std::wstring result;

        // U+306b U+307b U+3093 U+3054 U+6c49 U+8bed U+d55c U+ad6d
        Base64::Decode(L"44Gr44G744KT44GU5rGJ6K+t7ZWc6rWt", result);
        VERIFY_ARE_EQUAL(L"にほんご汉语한국", result);

        // U+d83d U+dc4d U+d83d U+dc4d U+d83c U+dffb U+d83d U+dc4d U+d83c U+dffc U+d83d
        // U+dc4d U+d83c U+dffd U+d83d U+dc4d U+d83c U+dffe U+d83d U+dc4d U+d83c U+dfff
        Base64::Decode(L"8J+RjfCfkY3wn4+78J+RjfCfj7zwn5GN8J+PvfCfkY3wn4++8J+RjfCfj78=", result);
        VERIFY_ARE_EQUAL(L"👍👍🏻👍🏼👍🏽👍🏾👍🏿", result);
    }
};
